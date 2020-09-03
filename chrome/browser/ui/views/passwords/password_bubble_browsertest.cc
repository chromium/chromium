// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"

#include <memory>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/passwords/manage_passwords_test.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/passwords/password_auto_sign_in_view.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/test/browser_test.h"
#include "ui/views/test/ax_event_counter.h"

using base::StartsWith;

// Test params:
//  - bool : whether to enable account storage feature or not.
class PasswordBubbleBrowserTest
    : public SupportsTestDialog<ManagePasswordsTest>,
      public testing::WithParamInterface<bool> {
 public:
  PasswordBubbleBrowserTest() {
    if (GetParam()) {
      // |kEnablePasswordsAccountStorage|, |kCompromisedPasswordsReengagement|
      // are both enabled.
      scoped_feature_list_.InitWithFeatures(
          {password_manager::features::kEnablePasswordsAccountStorage,
           password_manager::features::kCompromisedPasswordsReengagement},
          {});
    } else {
      // |kCompromisedPasswordsReengagement| enabled,
      // |kEnablePasswordsAccountStorage| disabled.
      scoped_feature_list_.InitWithFeatures(
          {password_manager::features::kCompromisedPasswordsReengagement},
          {password_manager::features::kEnablePasswordsAccountStorage});
    }
  }

  ~PasswordBubbleBrowserTest() override = default;

  void ShowUi(const std::string& name) override {
    if (StartsWith(name, "PendingPasswordBubble",
                   base::CompareCase::SENSITIVE)) {
      SetupPendingPassword();
    } else if (StartsWith(name, "AutomaticPasswordBubble",
                          base::CompareCase::SENSITIVE)) {
      SetupAutomaticPassword();
    } else if (StartsWith(name, "ManagePasswordBubble",
                          base::CompareCase::SENSITIVE)) {
      // Set test form to be account-stored. Otherwise, there is no indicator.
      test_form()->in_store =
          GetParam() ? autofill::PasswordForm::Store::kAccountStore
                     : autofill::PasswordForm::Store::kProfileStore;
      SetupManagingPasswords();
      ExecuteManagePasswordsCommand();
    } else if (StartsWith(name, "AutoSignin", base::CompareCase::SENSITIVE)) {
      test_form()->url = GURL("https://example.com");
      test_form()->display_name = base::ASCIIToUTF16("Peter");
      test_form()->username_value = base::ASCIIToUTF16("pet12@gmail.com");
      std::vector<std::unique_ptr<autofill::PasswordForm>> local_credentials;
      local_credentials.push_back(
          std::make_unique<autofill::PasswordForm>(*test_form()));

      PasswordAutoSignInView::set_auto_signin_toast_timeout(10);
      SetupAutoSignin(std::move(local_credentials));
    } else if (StartsWith(name, "MoveToAccountStoreBubble",
                          base::CompareCase::SENSITIVE)) {
      SetupMovingPasswords();
    } else if (StartsWith(name, "SafeState", base::CompareCase::SENSITIVE)) {
      SetupSafeState();
    } else if (StartsWith(name, "MoreToFixState",
                          base::CompareCase::SENSITIVE)) {
      SetupMoreToFixState();
    } else if (StartsWith(name, "UnsafeState", base::CompareCase::SENSITIVE)) {
      SetupUnsafeState();
    } else {
      ADD_FAILURE() << "Unknown dialog type";
      return;
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  DISALLOW_COPY_AND_ASSIGN(PasswordBubbleBrowserTest);
};

INSTANTIATE_TEST_SUITE_P(All, PasswordBubbleBrowserTest, ::testing::Bool());

IN_PROC_BROWSER_TEST_P(PasswordBubbleBrowserTest,
                       InvokeUi_PendingPasswordBubble) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PasswordBubbleBrowserTest,
                       InvokeUi_AutomaticPasswordBubble) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PasswordBubbleBrowserTest,
                       InvokeUi_ManagePasswordBubble) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PasswordBubbleBrowserTest, InvokeUi_AutoSignin) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PasswordBubbleBrowserTest, InvokeUi_SafeState) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PasswordBubbleBrowserTest, InvokeUi_MoreToFixState) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PasswordBubbleBrowserTest, InvokeUi_UnsafeState) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PasswordBubbleBrowserTest,
                       InvokeUi_MoveToAccountStoreBubble) {
  if (!GetParam()) {
    return;  // No moving bubble available without the flag.
  }
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PasswordBubbleBrowserTest, AlertAccessibleEvent) {
  views::test::AXEventCounter counter(views::AXEventManager::Get());
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kAlert));
  ShowUi("ManagePasswordBubble");
  // TODO(crbug.com/1082217): This should only produce one event
  EXPECT_LT(0, counter.GetCount(ax::mojom::Event::kAlert));
}
