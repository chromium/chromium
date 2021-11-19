// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sync/profile_signin_confirmation_dialog_views.h"

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/sync/profile_signin_confirmation_helper.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/ui_features.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "content/public/test/browser_test.h"

namespace {

// Test delegate passed to the confirmation dialog to receive (and currently
// ignore) the result from the dialog.
class TestSigninDialogDelegate : public ui::ProfileSigninConfirmationDelegate {
 public:
  TestSigninDialogDelegate() {}

  TestSigninDialogDelegate(const TestSigninDialogDelegate&) = delete;
  TestSigninDialogDelegate& operator=(const TestSigninDialogDelegate&) = delete;

  void OnCancelSignin() override {}
  void OnContinueSignin() override {}
  void OnSigninWithNewProfile() override {}
};

}  // namespace

class ProfileSigninConfirmationDialogTest : public DialogBrowserTest {
 public:
  ProfileSigninConfirmationDialogTest() {}

  ProfileSigninConfirmationDialogTest(
      const ProfileSigninConfirmationDialogTest&) = delete;
  ProfileSigninConfirmationDialogTest& operator=(
      const ProfileSigninConfirmationDialogTest&) = delete;

  void ShowUi(const std::string& name) override {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    TabDialogs::FromWebContents(web_contents)
        ->ShowProfileSigninConfirmation(
            browser(), "username@example.com",
            /*prompt_for_new_profile=*/true,
            std::make_unique<TestSigninDialogDelegate>());
  }
};

class WorkProfileSigninConfirmationDialogTest
    : public ProfileSigninConfirmationDialogTest {
 public:
  WorkProfileSigninConfirmationDialogTest() {
    features_.InitAndEnableFeature(features::kSyncConfirmationUpdatedText);
  }

 private:
  base::test::ScopedFeatureList features_;
};

// Test that calls ShowUi("default").
IN_PROC_BROWSER_TEST_F(ProfileSigninConfirmationDialogTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(WorkProfileSigninConfirmationDialogTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}
