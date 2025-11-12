// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_ui_factory.h"

#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_ui.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/user_manager/user_manager.h"

ChromeKeyboardUIFactory::ChromeKeyboardUIFactory() = default;
ChromeKeyboardUIFactory::~ChromeKeyboardUIFactory() = default;

std::unique_ptr<keyboard::KeyboardUI>
ChromeKeyboardUIFactory::CreateKeyboardUI() {
  auto* user = user_manager::UserManager::Get()->GetActiveUser();
  auto* browser_context =
      user != nullptr
          ? ash::BrowserContextHelper::Get()->GetBrowserContextByUser(user)
          : ash::BrowserContextHelper::Get()->GetSigninBrowserContext();
  return std::make_unique<ChromeKeyboardUI>(browser_context);
}
