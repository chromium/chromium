// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/parent_access/parent_access_browsertest_base.h"

#include <string>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_dialog.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

namespace ash {

ParentAccessBrowserTestBase::ParentAccessBrowserTestBase() = default;
ParentAccessBrowserTestBase::~ParentAccessBrowserTestBase() = default;

void ParentAccessBrowserTestBase::SetUp() {
  logged_in_user_mixin_ = std::make_unique<LoggedInUserMixin>(
      &mixin_host_, /*test_base=*/this, embedded_test_server(), GetLogInType());
  // Setup() must be called after the mixin is instantiated because it is what
  // actually causes the tests to be run.
  MixinBasedInProcessBrowserTest::SetUp();
}

content::WebUI* ParentAccessBrowserTestBase::GetWebUI() {
  return ParentAccessDialog::GetInstance()->GetWebUIForTest();
}

ParentAccessUI* ParentAccessBrowserTestBase::GetParentAccessUI() {
  return static_cast<ParentAccessUI*>(GetWebUI()->GetController());
}

content::WebContents* ParentAccessBrowserTestBase::GetContents() {
  return GetWebUI()->GetWebContents();
}

// InProcessBrowserTest methods
void ParentAccessBrowserTestBase::SetUpOnMainThread() {
  MixinBasedInProcessBrowserTest::SetUpOnMainThread();
  // Login must occur here because it relies on additional setup
  // that takes place after Setup()
  logged_in_user_mixin_->LogInUser();

  // The identity test environment is independent of the logged in user  mixin
  // and is used to configure the OAuth token fetcher for test.
  identity_test_env_ = std::make_unique<signin::IdentityTestEnvironment>();
  identity_test_env_->MakePrimaryAccountAvailable(
      logged_in_user_mixin_->GetAccountId().GetUserEmail(),
      signin::ConsentLevel::kSync);
  // This makes the identity manager return the string "access_token" for the
  // access token.
  identity_test_env_->SetAutomaticIssueOfAccessTokens(true);
  ParentAccessUI::SetUpForTest(identity_test_env_->identity_manager());
}

//  ParentAccessRegularUserBrowserTestBase
ParentAccessRegularUserBrowserTestBase::
    ParentAccessRegularUserBrowserTestBase() = default;
ParentAccessRegularUserBrowserTestBase::
    ~ParentAccessRegularUserBrowserTestBase() = default;

// ParentAccessBrowserTestBase methods
LoggedInUserMixin::LogInType
ParentAccessRegularUserBrowserTestBase::GetLogInType() {
  return LoggedInUserMixin::LogInType::kConsumer;
}

//  ParentAccessChildUserBrowserTestBase
ParentAccessChildUserBrowserTestBase::ParentAccessChildUserBrowserTestBase() =
    default;
ParentAccessChildUserBrowserTestBase::~ParentAccessChildUserBrowserTestBase() =
    default;

// ParentAccessBrowserTestBase methods
LoggedInUserMixin::LogInType
ParentAccessChildUserBrowserTestBase::GetLogInType() {
  return LoggedInUserMixin::LogInType::kChild;
}

}  // namespace ash
