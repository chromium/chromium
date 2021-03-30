// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sync/profile_signin_confirmation_dialog_views.h"

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/sync/profile_signin_confirmation_helper.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "content/public/test/browser_test.h"

namespace {

// Test delegate passed to the confirmation dialog to receive (and currently
// ignore) the result from the dialog.
class TestSigninDialogDelegate : public ui::ProfileSigninConfirmationDelegate {
 public:
  TestSigninDialogDelegate() {}

  void OnCancelSignin() override {}
  void OnContinueSignin() override {}
  void OnSigninWithNewProfile() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestSigninDialogDelegate);
};

}  // namespace

class ProfileSigninConfirmationDialogTest : public DialogBrowserTest {
 public:
  ProfileSigninConfirmationDialogTest() {}

  void ShowUi(const std::string& name) override {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    TabDialogs::FromWebContents(web_contents)
        ->ShowProfileSigninConfirmation(
            browser(), "username@example.com",
            /*prompt_for_new_profile=*/true,
            std::make_unique<TestSigninDialogDelegate>());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ProfileSigninConfirmationDialogTest);
};

// Test that calls ShowUi("true").
IN_PROC_BROWSER_TEST_F(ProfileSigninConfirmationDialogTest, InvokeUi_default) {
  ShowAndVerifyUi();
}
