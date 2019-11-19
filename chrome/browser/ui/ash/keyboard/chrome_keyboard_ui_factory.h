// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_KEYBOARD_CHROME_KEYBOARD_UI_FACTORY_H_
#define CHROME_BROWSER_UI_ASH_KEYBOARD_CHROME_KEYBOARD_UI_FACTORY_H_

#include "ash/keyboard/ui/keyboard_ui_factory.h"
#include "base/macros.h"

namespace keyboard {
class KeyboardUI;
}

// ChromeKeyboardUIFactory is the factory class for ChromeKeyboardUI.
class ChromeKeyboardUIFactory : public keyboard::KeyboardUIFactory {
 public:
  ChromeKeyboardUIFactory();
  ~ChromeKeyboardUIFactory() override;

 private:
  // keyboard::KeyboardUIFactory:
  std::unique_ptr<keyboard::KeyboardUI> CreateKeyboardUI() override;

  DISALLOW_COPY_AND_ASSIGN(ChromeKeyboardUIFactory);
};

#endif  // CHROME_BROWSER_UI_ASH_KEYBOARD_CHROME_KEYBOARD_UI_FACTORY_H_
