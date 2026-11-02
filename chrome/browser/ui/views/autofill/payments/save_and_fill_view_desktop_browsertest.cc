// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/save_and_fill_view_desktop.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/autofill/payments/payments_view_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/extensions/security_dialog_tracker.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/payments/test_legal_message_line.h"
#include "components/autofill/core/browser/ui/payments/save_and_fill_dialog_controller_impl.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class SaveAndFillViewDesktopBrowserTest : public DialogBrowserTest {
 public:
  SaveAndFillViewDesktopBrowserTest() = default;
  ~SaveAndFillViewDesktopBrowserTest() override = default;
  SaveAndFillViewDesktopBrowserTest(const SaveAndFillViewDesktopBrowserTest&) =
      delete;
  SaveAndFillViewDesktopBrowserTest& operator=(
      const SaveAndFillViewDesktopBrowserTest&) = delete;

  void ShowUi(const std::string& name) override {
    controller_ = std::make_unique<SaveAndFillDialogControllerImpl>();
    if (name == "Pending") {
      controller_->ShowPendingDialog(
          base::BindOnce(&CreateAndShowSaveAndFillDialog,
                         controller_->GetWeakPtr(),
                         base::Unretained(web_contents())),
          base::DoNothing());
    } else {
      controller_->ShowLocalDialog(
          base::BindOnce(&CreateAndShowSaveAndFillDialog,
                         controller_->GetWeakPtr(),
                         base::Unretained(web_contents())),
          base::DoNothing());
    }
  }

  void DismissUi() override { controller_.reset(); }

  content::WebContents* web_contents() const {
    return browser()->GetActiveTabInterface()->GetContents();
  }

 protected:
  std::unique_ptr<SaveAndFillDialogControllerImpl> controller_;
};

IN_PROC_BROWSER_TEST_F(SaveAndFillViewDesktopBrowserTest, ShowAndVerifyUi) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(SaveAndFillViewDesktopBrowserTest,
                       TracksVisibleSecurityDialog) {
  extensions::SecurityDialogTracker* tracker =
      extensions::SecurityDialogTracker::GetInstance();
  EXPECT_FALSE(tracker->BrowserHasVisibleSecurityDialogs(browser()));

  ShowUi("Local");
  ASSERT_TRUE(VerifyUi());
  EXPECT_TRUE(tracker->BrowserHasVisibleSecurityDialogs(browser()));

  BrowserWindowInterface* other_browser =
      CreateBrowser(browser()->GetProfile());
  EXPECT_FALSE(tracker->BrowserHasVisibleSecurityDialogs(other_browser));

  DismissUi();
  EXPECT_FALSE(tracker->BrowserHasVisibleSecurityDialogs(browser()));
}

IN_PROC_BROWSER_TEST_F(SaveAndFillViewDesktopBrowserTest,
                       TracksVisibleSecurityDialogAcrossStateTransitions) {
  extensions::SecurityDialogTracker* tracker =
      extensions::SecurityDialogTracker::GetInstance();
  EXPECT_FALSE(tracker->BrowserHasVisibleSecurityDialogs(browser()));

  ShowUi("Pending");
  ASSERT_TRUE(VerifyUi());
  EXPECT_TRUE(tracker->BrowserHasVisibleSecurityDialogs(browser()));

  controller_->ShowUploadDialog({TestLegalMessageLine("Test legal message")},
                                base::DoNothing());
  EXPECT_TRUE(tracker->BrowserHasVisibleSecurityDialogs(browser()));

  controller_->ShowLocalDialog(base::NullCallback(), base::DoNothing());
  EXPECT_TRUE(tracker->BrowserHasVisibleSecurityDialogs(browser()));

  DismissUi();
  EXPECT_FALSE(tracker->BrowserHasVisibleSecurityDialogs(browser()));
}

}  // namespace autofill
