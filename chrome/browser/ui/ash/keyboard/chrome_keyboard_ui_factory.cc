// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_ui_factory.h"

#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_ui.h"

ChromeKeyboardUIFactory::ChromeKeyboardUIFactory() = default;
ChromeKeyboardUIFactory::~ChromeKeyboardUIFactory() = default;

std::unique_ptr<keyboard::KeyboardUI>
ChromeKeyboardUIFactory::CreateKeyboardUI() {
  return std::make_unique<ChromeKeyboardUI>(
      ProfileManager::GetActiveUserProfile());
}
