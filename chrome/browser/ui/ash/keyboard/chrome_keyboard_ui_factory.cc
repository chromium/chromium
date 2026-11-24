// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_ui_factory.h"

#include "base/check.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_ui.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/browser_context.h"

ChromeKeyboardUIFactory::ChromeKeyboardUIFactory() = default;
ChromeKeyboardUIFactory::~ChromeKeyboardUIFactory() = default;

std::unique_ptr<keyboard::KeyboardUI>
ChromeKeyboardUIFactory::CreateKeyboardUI() {
  auto* browser_context_helper = ash::BrowserContextHelper::Get();
  content::BrowserContext* browser_context = nullptr;
  if (const session_manager::Session* active_session =
          session_manager::SessionManager::Get()->GetActiveSession()) {
    browser_context = browser_context_helper->GetBrowserContextByAccountId(
        active_session->account_id());
  }
  // The keyboard runs on the sign-in profile on the login screen, and keeps
  // using it after login until the user's profile is created.
  if (!browser_context) {
    browser_context = browser_context_helper->GetSigninBrowserContext();
  }
  CHECK(browser_context);
  return std::make_unique<ChromeKeyboardUI>(browser_context);
}
