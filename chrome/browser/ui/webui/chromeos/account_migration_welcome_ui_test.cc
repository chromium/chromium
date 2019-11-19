// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/account_migration_welcome_ui_test.h"

#include "base/macros.h"
#include "chrome/browser/ui/webui/chromeos/account_migration_welcome_dialog.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/web_dialogs/web_dialog_ui.h"

AccountMigrationWelcomeUITest::AccountMigrationWelcomeUITest() = default;
AccountMigrationWelcomeUITest::~AccountMigrationWelcomeUITest() = default;

void AccountMigrationWelcomeUITest::ShowDialog() {
  auto* account_email = "test@example.com";
  auto* dialog = chromeos::AccountMigrationWelcomeDialog::Show(account_email);
  auto* webui = dialog->GetWebUIForTest();
  auto* web_contents = webui->GetWebContents();
  content::WaitForLoadStop(web_contents);
  web_contents->GetMainFrame()->SetWebUIProperty(
      "expectedUrl", chrome::kChromeUIAccountMigrationWelcomeURL);
  web_contents->GetMainFrame()->SetWebUIProperty("expectedEmail",
                                                 account_email);
  SetWebUIInstance(webui);
}
