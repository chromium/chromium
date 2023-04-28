// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/supervised_user/supervised_user_mixin.h"

#include <memory>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"

namespace supervised_user {

namespace {
void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
  // Sets all required testing factories to have control over identity
  // environment during test. Effectively, this substitutes the real identity
  // environment with identity test environment, taking care to fulfill all
  // required dependencies.
  IdentityTestEnvironmentProfileAdaptor::
      SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
}

bool IdentityManagerAlreadyHasPrimaryAccount(
    signin::IdentityManager* identity_manager,
    base::StringPiece email,
    signin::ConsentLevel consent_level) {
  if (!identity_manager->HasPrimaryAccount(consent_level)) {
    return false;
  }
  CoreAccountInfo account_info =
      identity_manager->GetPrimaryAccountInfo(consent_level);
  return account_info.email == email;
}
}  // namespace

SupervisedUserMixin::SupervisedUserMixin(
    InProcessBrowserTestMixinHost& test_mixin_host,
    InProcessBrowserTest* test_base)
    : SupervisedUserMixin(test_mixin_host,
                          test_base,
                          SupervisedUserMixin::Options()) {}

SupervisedUserMixin::SupervisedUserMixin(
    InProcessBrowserTestMixinHost& test_mixin_host,
    InProcessBrowserTest* test_base,
    const Options& options)
    : InProcessBrowserTestMixin(&test_mixin_host),
      test_base_(test_base),
      fake_gaia_mixin_(&test_mixin_host),
      consent_level_(options.consent_level),
      email_(options.email),
      account_type_(options.account_type) {}
SupervisedUserMixin::~SupervisedUserMixin() = default;

void SupervisedUserMixin::SetUpInProcessBrowserTestFixture() {
  subscription_ =
      BrowserContextDependencyManager::GetInstance()
          ->RegisterCreateServicesCallbackForTesting(
              base::BindRepeating(&OnWillCreateBrowserContextServices));
}

void SupervisedUserMixin::SetUpOnMainThread() {
  SetUpIdentityTestEnvironment();
  LogInUser();
  SetUpTestServer();
}

void SupervisedUserMixin::SetUpTestServer() {
  // By default, browser tests block anything that doesn't go to localhost, so
  // account.google.com requests would never reach fake GAIA server without
  // this.
  test_base_->host_resolver()->AddRule("*", "127.0.0.1");
}

void SupervisedUserMixin::SetUpIdentityTestEnvironment() {
  adaptor_ = std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
      test_base_->browser()->profile());
}

void SupervisedUserMixin::LogInUser() {
  if (!IdentityManagerAlreadyHasPrimaryAccount(
          identity_test_environment()->identity_manager(), email_,
          consent_level_)) {
    // PRE_ tests intentionally leave accounts that are picked up by subsequent
    // test runs.
    AccountInfo account_info =
        identity_test_environment()->MakePrimaryAccountAvailable(
            email_, consent_level_);
    CHECK(!account_info.account_id.empty());
  }

  identity_test_environment()->SetRefreshTokenForPrimaryAccount();
  identity_test_environment()->SetAutomaticIssueOfAccessTokens(true);

  SetSupervision(/*is_supervised_profile=*/account_type_ == kSupervised);
}

void SupervisedUserMixin::SetSupervision(bool is_supervised_profile) {
  testing_profile()->SetIsSupervisedProfile(is_supervised_profile);
  CHECK(test_base_->browser()->profile()->IsChild() == is_supervised_profile);
}

TestingProfile* SupervisedUserMixin::testing_profile() {
  return static_cast<TestingProfile*>(test_base_->browser()->profile());
}
signin::IdentityTestEnvironment*
SupervisedUserMixin::identity_test_environment() {
  CHECK(adaptor_->identity_test_env())
      << "Do not use before the environment is set up.";
  return adaptor_->identity_test_env();
}

}  // namespace supervised_user
