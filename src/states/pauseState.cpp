#include "pauseState.h"

#include <stormengine2/xmlLoader.h>

#include "menuState.h"

PauseState::PauseState(SDL_Renderer *renderer, int windowWidth, int windowHeight,
                       bool isDebugging, AssetStore *assetStore, bool &isRunning,
                       GameStateMachine &machine)
    : renderer_{renderer}, windowWidth_{windowWidth}, windowHeight_{windowHeight},
      isDebugging_{isDebugging}, assetStore_{assetStore},
      isRunning_{isRunning}, machine_{machine}
{
    LoadTexturesFromXml("./data/conan.xml", "PAUSE", "./data/gfx/",
                        renderer_, assetStore_, &logger_);

    XmlLoader xml("./data/conan.xml");
    buttons_ = ButtonsFromXml(xml.GetObjects("PAUSE"));

    millisecondsPreviousFrame_ = SDL_GetTicks();
}

PauseState::~PauseState() { onExit(); }

bool PauseState::onEnter() {
    m_loadingComplete = true;
    return true;
}

bool PauseState::onExit() {
    m_exiting = true;
    return true;
}

void PauseState::processInput() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) { isRunning_ = false; return; }
        if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
            switch (event.key.keysym.sym) {
            case SDLK_ESCAPE: case SDLK_p: wantResume_ = true; break;
            case SDLK_UP:     case SDLK_w: navUp_      = true; break;
            case SDLK_DOWN:   case SDLK_s: navDown_    = true; break;
            case SDLK_RETURN: case SDLK_SPACE: navConfirm_ = true; break;
            default: break;
            }
        }
        if (event.type == SDL_MOUSEMOTION) {
            mouseX_ = static_cast<float>(event.motion.x);
            mouseY_ = static_cast<float>(event.motion.y);
        }
        // Mouse EVENTS are scaled into the 640x480 logical space by
        // SDL_RenderSetLogicalSize (and cover synthesized touch taps);
        // SDL_GetMouseState would return raw window pixels.
        if (event.type == SDL_MOUSEBUTTONDOWN &&
            event.button.button == SDL_BUTTON_LEFT) {
            mouseDown_ = true;
            mouseX_ = static_cast<float>(event.button.x);
            mouseY_ = static_cast<float>(event.button.y);
        }
        if (event.type == SDL_MOUSEBUTTONUP &&
            event.button.button == SDL_BUTTON_LEFT) {
            mouseDown_ = false;
            mouseX_ = static_cast<float>(event.button.x);
            mouseY_ = static_cast<float>(event.button.y);
        }
    }

    GamepadState gp = gamepad_.poll();
    wantResume_ = wantResume_ || gp.pausePressed; // Start toggles back out
    navUp_      = navUp_      || gp.upPressed;
    navDown_    = navDown_    || gp.downPressed;
    navConfirm_ = navConfirm_ || gp.confirmPressed;

}

void PauseState::Activate(int callbackId) {
    if (callbackId == 1) {              // Main menu — abandon the run
        // Pop this overlay first so the PlayState underneath is the one
        // changeState replaces — otherwise it stays buried on the stack.
        machine_.popState();
        machine_.changeState(new MenuState(
            renderer_, windowWidth_, windowHeight_, isDebugging_,
            assetStore_, isRunning_, machine_));
    } else if (callbackId == 2) {       // Resume
        machine_.popState();
    }
}

void PauseState::update() {
    int timeToWait = MILLISECS_PER_FRAME - (SDL_GetTicks() - millisecondsPreviousFrame_);
    if (timeToWait > 0 && timeToWait <= MILLISECS_PER_FRAME)
        SDL_Delay(timeToWait);
    millisecondsPreviousFrame_ = SDL_GetTicks();

    if (wantResume_) {
        machine_.popState(); // PlayState::resume() restarts its clock
        return;
    }

    if (navUp_)   selected_ = WrapSelection(selected_, -1, buttons_.size());
    if (navDown_) selected_ = WrapSelection(selected_,  1, buttons_.size());
    bool confirm = navConfirm_;
    navUp_ = navDown_ = navConfirm_ = false;

    if (confirm && !buttons_.empty()) {
        Activate(buttons_[selected_].callbackId);
        return;
    }

    for (auto &b : buttons_) {
        if (!UpdateButton(b, mouseX_, mouseY_, mouseDown_)) continue;
        Activate(b.callbackId);
        return;
    }
}

void PauseState::render() {
    // Opaque cave-dark backdrop — the machine renders only the top state.
    SDL_SetRenderDrawColor(renderer_, 18, 24, 32, 255);
    SDL_RenderClear(renderer_);

    for (std::size_t i = 0; i < buttons_.size(); ++i)
        DrawButton(renderer_, assetStore_, buttons_[i], mouseX_, mouseY_,
                   mouseDown_, static_cast<int>(i) == selected_);

    SDL_RenderPresent(renderer_);
}
