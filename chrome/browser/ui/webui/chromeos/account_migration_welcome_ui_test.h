// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_ACCOUNT_MIGRATION_WELCOME_UI_TEST_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_ACCOUNT_MIGRATION_WELCOME_UI_TEST_H_

#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/web_ui_browser_test.h"
// Test framework for
// chrome/browser/ui/webui/chromeos/account_migration_welcome_test.js
class AccountMigrationWelcomeUITest : public WebUIBrowserTest {
 public:
  AccountMigrationWelcomeUITest();
  ~AccountMigrationWelcomeUITest() override;

 protected:
  void ShowDialog();
};

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_ACCOUNT_MIGRATION_WELCOME_UI_TEST_H_
