// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/email_verifier/email_verified_toast_menu_model.h"

#include "base/test/metrics/user_action_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {
namespace {

using EmailVerifiedToastMenuModelBrowserTest = InProcessBrowserTest;

// Verifies the menu structure (single 'Manage' item with correct label).
IN_PROC_BROWSER_TEST_F(EmailVerifiedToastMenuModelBrowserTest, MenuStructure) {
  EmailVerifiedToastMenuModel model(browser());

  ASSERT_EQ(model.GetItemCount(), 1u);
  EXPECT_EQ(model.GetCommandIdAt(0),
            EmailVerifiedToastMenuModel::CommandId::kManage);
  EXPECT_EQ(model.GetLabelAt(0), l10n_util::GetStringUTF16(IDS_MANAGE));
}

// Verifies that executing the command records the user action and opens
// Settings.
IN_PROC_BROWSER_TEST_F(EmailVerifiedToastMenuModelBrowserTest,
                       ExecuteCommandRecordsActionAndNavigates) {
  EmailVerifiedToastMenuModel model(browser());
  base::UserActionTester user_action_tester;

  EXPECT_EQ(
      user_action_tester.GetActionCount("Toast.EmailVerified.ManageClicked"),
      0);

  // Execute the "Manage" command.
  model.ExecuteCommand(EmailVerifiedToastMenuModel::CommandId::kManage,
                       /*event_flags=*/0);

  // Verify the user action was recorded.
  EXPECT_EQ(
      user_action_tester.GetActionCount("Toast.EmailVerified.ManageClicked"),
      1);

  // Verify that the settings subpage was opened in a tab.
  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);
  EXPECT_EQ(
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      chrome::GetSettingsUrl(chrome::kContactInfoSubPage));
}

// Verifies that invalid command IDs are ignored and do not record metrics.
IN_PROC_BROWSER_TEST_F(EmailVerifiedToastMenuModelBrowserTest,
                       UnknownCommandIgnored) {
  EmailVerifiedToastMenuModel model(browser());
  base::UserActionTester user_action_tester;

  model.ExecuteCommand(/*command_id=*/999, /*event_flags=*/0);

  EXPECT_EQ(
      user_action_tester.GetActionCount("Toast.EmailVerified.ManageClicked"),
      0);
  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);
}

}  // namespace
}  // namespace autofill
