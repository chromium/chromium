// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/forced_reauthentication_dialog_view.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/identity/public/cpp/identity_test_utils.h"
#include "ui/base/ui_base_types.h"

class ForcedReauthenticationDialogViewBrowserTest : public DialogBrowserTest {
 public:
  ForcedReauthenticationDialogViewBrowserTest() {}

  // override DialogBrowserTest
  void ShowUi(const std::string& name) override {
    Profile* profile = browser()->profile();
    identity::IdentityManager* manager =
        IdentityManagerFactory::GetForProfile(profile);

    identity::MakePrimaryAccountAvailable(
        SigninManagerFactory::GetForProfile(profile),
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile), manager,
        "test@xyz.com");

    ForcedReauthenticationDialogView::ShowDialog(
        profile, manager, base::TimeDelta::FromSeconds(60));
  }

  // An integer represents the buttons of dialog.

 private:
  DISALLOW_COPY_AND_ASSIGN(ForcedReauthenticationDialogViewBrowserTest);
};

IN_PROC_BROWSER_TEST_F(ForcedReauthenticationDialogViewBrowserTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}

// Dialog will not be display if there is no valid browser window.
IN_PROC_BROWSER_TEST_F(ForcedReauthenticationDialogViewBrowserTest,
                       NotOpenDialogDueToNoBrowser) {
  Profile* profile = browser()->profile();
  CloseBrowserSynchronously(browser());
  EXPECT_EQ(nullptr,
            ForcedReauthenticationDialogView::ShowDialog(
                profile, IdentityManagerFactory::GetForProfile(profile),
                base::TimeDelta::FromSeconds(60)));
}

IN_PROC_BROWSER_TEST_F(ForcedReauthenticationDialogViewBrowserTest,
                       NotOpenDialogDueToNoTabs) {
  Profile* profile = browser()->profile();
  TabStripModel* model = browser()->tab_strip_model();
  ASSERT_EQ(1, model->count());
  model->CloseWebContentsAt(0, TabStripModel::CLOSE_NONE);
  ASSERT_TRUE(model->empty());
  EXPECT_EQ(nullptr,
            ForcedReauthenticationDialogView::ShowDialog(
                profile, IdentityManagerFactory::GetForProfile(profile),
                base::TimeDelta::FromSeconds(60)));
}
