// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_ui.h"

#include <string>
#include <utility>

#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_bounds_observer.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_web_contents.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/compositor/layer.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/wm/core/shadow_types.h"

namespace {

const int kShadowElevationVirtualKeyboard = 2;

}  // namespace

ChromeKeyboardUI::ChromeKeyboardUI(content::BrowserContext* context)
    : browser_context_(context) {
}

ChromeKeyboardUI::~ChromeKeyboardUI() {
  DCHECK(!keyboard_controller());
}

// keyboard::KeyboardUI:

aura::Window* ChromeKeyboardUI::LoadKeyboardWindow(LoadCallback callback) {
  DCHECK(!keyboard_contents_);

  keyboard_contents_ = std::make_unique<ChromeKeyboardWebContents>(
      browser_context_,
      ChromeKeyboardControllerClient::Get()->GetVirtualKeyboardUrl(),
      base::BindOnce(
          [](LoadCallback callback) {
            ChromeKeyboardControllerClient::Get()->NotifyKeyboardLoaded();
            std::move(callback).Run();
          },
          std::move(callback)),
      base::NullCallback());

  aura::Window* keyboard_window =
      keyboard_contents_->web_contents()->GetNativeView();
  keyboard_window->AddObserver(this);

  return keyboard_window;
}

aura::Window* ChromeKeyboardUI::GetKeyboardWindow() const {
  return keyboard_contents_
             ? keyboard_contents_->web_contents()->GetNativeView()
             : nullptr;
}

ui::GestureConsumer* ChromeKeyboardUI::GetGestureConsumer() const {
  return keyboard_contents_
             ? keyboard_contents_->web_contents()->GetContentNativeView()
             : nullptr;
}

ui::InputMethod* ChromeKeyboardUI::GetInputMethod() {
  ash::IMEBridge* bridge = ash::IMEBridge::Get();
  if (!bridge || !bridge->GetInputContextHandler()) {
    // Needed by a handful of browser tests that use MockInputMethod.
    return ash::Shell::GetRootWindowForNewWindows()
        ->GetHost()
        ->GetInputMethod();
  }

  return bridge->GetInputContextHandler()->GetInputMethod();
}

void ChromeKeyboardUI::ReloadKeyboardIfNeeded() {
  VLOG(1) << "ReloadKeyboardIfNeeded";
  DCHECK(keyboard_contents_);
  keyboard_contents_->SetKeyboardUrl(
      ChromeKeyboardControllerClient::Get()->GetVirtualKeyboardUrl());
}

// aura::WindowObserver:

void ChromeKeyboardUI::OnWindowBoundsChanged(aura::Window* window,
                                             const gfx::Rect& old_bounds,
                                             const gfx::Rect& new_bounds,
                                             ui::PropertyChangeReason reason) {
  SetShadowAroundKeyboard();
}

void ChromeKeyboardUI::OnWindowDestroyed(aura::Window* window) {
  window->RemoveObserver(this);
}

void ChromeKeyboardUI::OnWindowParentChanged(aura::Window* window,
                                             aura::Window* parent) {
  SetShadowAroundKeyboard();
}

// private methods:

void ChromeKeyboardUI::SetShadowAroundKeyboard() {
  aura::Window* contents_window = GetKeyboardWindow();
  DCHECK(contents_window);

  if (!shadow_) {
    shadow_ = std::make_unique<ui::Shadow>();
    shadow_->Init(kShadowElevationVirtualKeyboard);
    shadow_->layer()->SetVisible(true);
    contents_window->layer()->Add(shadow_->layer());
  }

  shadow_->SetContentBounds(gfx::Rect(contents_window->bounds().size()));

  // In floating mode, make the shadow layer invisible because the shadows are
  // drawn manually by the IME extension.
  // TODO(https://crbug.com/856195): Remove this when we figure out how ChromeOS
  // can draw custom shaped shadows, or how overscrolling can account for
  // shadows drawn by IME.
  shadow_->layer()->SetVisible(
      keyboard_controller()->GetActiveContainerType() ==
      keyboard::ContainerType::kFullWidth);
}
