// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_KEYBOARD_CHROME_KEYBOARD_UI_H_
#define CHROME_BROWSER_UI_ASH_KEYBOARD_CHROME_KEYBOARD_UI_H_

#include <memory>

#include "ash/keyboard/ui/keyboard_ui.h"
#include "base/macros.h"
#include "ui/aura/window_observer.h"

class ChromeKeyboardWebContents;

namespace aura {
class Window;
}

namespace content {
class BrowserContext;
}

namespace ui {
class Shadow;
}

// Subclass of KeyboardUI. It is used by KeyboardController to get
// access to the virtual keyboard window and setup Chrome extension functions.
// Used in classic ash, not in mash.
class ChromeKeyboardUI : public keyboard::KeyboardUI,
                         public aura::WindowObserver {
 public:
  explicit ChromeKeyboardUI(content::BrowserContext* context);
  ~ChromeKeyboardUI() override;

  // keyboard::KeyboardUI:
  aura::Window* LoadKeyboardWindow(LoadCallback callback) override;
  aura::Window* GetKeyboardWindow() const override;
  ui::InputMethod* GetInputMethod() override;
  void ReloadKeyboardIfNeeded() override;

  // aura::WindowObserver:
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowDestroyed(aura::Window* window) override;
  void OnWindowParentChanged(aura::Window* window,
                             aura::Window* parent) override;

 private:
  // Sets shadow around the keyboard. If shadow has not been created yet,
  // this method creates it.
  void SetShadowAroundKeyboard();

  // The BrowserContext to use for creating the WebContents hosting the
  // keyboard.
  content::BrowserContext* const browser_context_;

  std::unique_ptr<ChromeKeyboardWebContents> keyboard_contents_;
  std::unique_ptr<ui::Shadow> shadow_;

  DISALLOW_COPY_AND_ASSIGN(ChromeKeyboardUI);
};

#endif  // CHROME_BROWSER_UI_ASH_KEYBOARD_CHROME_KEYBOARD_UI_H_
