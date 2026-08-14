// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/safe_browsing/password_reuse_modal_warning_dialog.h"

#include "base/functional/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/safe_browsing/content/browser/password_protection/password_protection_service.h"
#include "content/public/test/browser_test.h"
#include "ui/views/view_tracker.h"

namespace safe_browsing {

class PasswordReuseModalWarningTest : public DialogBrowserTest {
 public:
  PasswordReuseModalWarningTest() = default;

  PasswordReuseModalWarningTest(const PasswordReuseModalWarningTest&) = delete;
  PasswordReuseModalWarningTest& operator=(
      const PasswordReuseModalWarningTest&) = delete;

  ~PasswordReuseModalWarningTest() override = default;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ReusedPasswordAccountType password_type;
    password_type.set_account_type(ReusedPasswordAccountType::GSUITE);
    auto* dialog = new PasswordReuseModalWarningDialog(
        web_contents, nullptr, password_type,
        base::BindOnce(&PasswordReuseModalWarningTest::DialogCallback,
                       base::Unretained(this)));
    dialog_tracker_.SetView(dialog);
    constrained_window::CreateBrowserModalDialogViews(
        dialog, web_contents->GetTopLevelNativeWindow())
        ->Show();
  }

  void CloseActiveWebContents() {
    TabStripModel* tab_strip_model = browser()->tab_strip_model();
    tab_strip_model->CloseWebContents(tab_strip_model->GetActiveWebContents(),
                                      TabCloseTypes::CLOSE_NONE);
  }

  void DialogCallback(WarningAction action) { latest_user_action_ = action; }

  PasswordReuseModalWarningDialog* dialog() {
    return static_cast<PasswordReuseModalWarningDialog*>(
        dialog_tracker_.view());
  }

 protected:
  views::ViewTracker dialog_tracker_;
  WarningAction latest_user_action_ = WarningAction::SHOWN;
};

IN_PROC_BROWSER_TEST_F(PasswordReuseModalWarningTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(PasswordReuseModalWarningTest, TestBasicDialogBehavior) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Simulating a click on ui::mojom::DialogButton::kOk button results in a
  // CHANGE_PASSWORD action.
  ShowUi(std::string());
  ASSERT_TRUE(dialog());
  dialog()->AcceptDialog();
  EXPECT_EQ(WarningAction::CHANGE_PASSWORD, latest_user_action_);

  // Simulating a click on ui::mojom::DialogButton::kCancel button results in an
  // IGNORE_WARNING action.
  ShowUi(std::string());
  ASSERT_TRUE(dialog());
  dialog()->CancelDialog();
  EXPECT_EQ(WarningAction::IGNORE_WARNING, latest_user_action_);
}

IN_PROC_BROWSER_TEST_F(PasswordReuseModalWarningTest,
                       CloseDialogWhenWebContentsDestroyed) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ShowUi(std::string());
  CloseActiveWebContents();
  EXPECT_EQ(WarningAction::CLOSE, latest_user_action_);
}

}  // namespace safe_browsing
