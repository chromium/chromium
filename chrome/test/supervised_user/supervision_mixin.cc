// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/supervised_user/supervision_mixin.h"

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
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

void SetIsSupervisedProfile(Profile* profile, bool is_supervised_profile) {
  if (is_supervised_profile) {
    EnableParentalControls(*profile->GetPrefs());
  } else {
    DisableParentalControls(*profile->GetPrefs());
  }
}

}  // namespace

SupervisionMixin::SupervisionMixin(
    InProcessBrowserTestMixinHost& test_mixin_host,
    InProcessBrowserTest* test_base,
    const Options& options)
    : InProcessBrowserTestMixin(&test_mixin_host),
      test_base_(test_base),
      fake_gaia_mixin_(&test_mixin_host),
      consent_level_(options.consent_level),
      email_(options.email),
      account_type_(options.account_type) {}

SupervisionMixin::SupervisionMixin(
    InProcessBrowserTestMixinHost& test_mixin_host,
    InProcessBrowserTest* test_base,
    base::raw_ptr<net::EmbeddedTestServer> embedded_test_server,
    const Options& options)
    : InProcessBrowserTestMixin(&test_mixin_host),
      test_base_(test_base),
      fake_gaia_mixin_(&test_mixin_host),
      embedded_test_server_setup_mixin_(absl::in_place,
                                        test_mixin_host,
                                        embedded_test_server,
                                        options.embedded_test_server_options),
      consent_level_(options.consent_level),
      email_(options.email),
      account_type_(options.account_type) {}

SupervisionMixin::~SupervisionMixin() = default;

void SupervisionMixin::SetUpInProcessBrowserTestFixture() {
  subscription_ =
      BrowserContextDependencyManager::GetInstance()
          ->RegisterCreateServicesCallbackForTesting(
              base::BindRepeating(&OnWillCreateBrowserContextServices));
}

void SupervisionMixin::SetUpOnMainThread() {
  SetUpIdentityTestEnvironment();
  LogInUser();
  SetUpTestServer();
}

void SupervisionMixin::SetUpTestServer() {
  // By default, browser tests block anything that doesn't go to localhost, so
  // account.google.com requests would never reach fake GAIA server without
  // this.
  test_base_->host_resolver()->AddRule("*", "127.0.0.1");
}

void SupervisionMixin::SetUpIdentityTestEnvironment() {
  adaptor_ =
      std::make_unique<IdentityTestEnvironmentProfileAdaptor>(GetProfile());
}

void SupervisionMixin::LogInUser() {
  if (!IdentityManagerAlreadyHasPrimaryAccount(
          GetIdentityTestEnvironment()->identity_manager(), email_,
          consent_level_)) {
    // PRE_ tests intentionally leave accounts that are picked up by subsequent
    // test runs.
    AccountInfo account_info =
        GetIdentityTestEnvironment()->MakePrimaryAccountAvailable(
            email_, consent_level_);
    CHECK(!account_info.account_id.empty());
  }

  GetIdentityTestEnvironment()->SetRefreshTokenForPrimaryAccount();
  GetIdentityTestEnvironment()->SetAutomaticIssueOfAccessTokens(true);

  SetIsSupervisedProfile(
      GetProfile(),
      /*is_supervised_profile=*/account_type_ == AccountType::kSupervised);
}

Profile* SupervisionMixin::GetProfile() const {
  return test_base_->browser()->profile();
}

signin::IdentityTestEnvironment* SupervisionMixin::GetIdentityTestEnvironment()
    const {
  CHECK(adaptor_->identity_test_env())
      << "Do not use before the environment is set up.";
  return adaptor_->identity_test_env();
}

}  // namespace supervised_user
