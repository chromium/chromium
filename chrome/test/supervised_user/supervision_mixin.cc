// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/supervised_user/supervision_mixin.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/supervised_user/child_account_test_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/supervised_user/core/browser/child_account_service.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/supervised_user/test_support/kids_chrome_management_test_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test_utils.h"
#include "google_apis/gaia/fake_gaia.h"

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
    std::string_view email,
    signin::ConsentLevel consent_level) {
  if (!identity_manager->HasPrimaryAccount(consent_level)) {
    return false;
  }
  CoreAccountInfo account_info =
      identity_manager->GetPrimaryAccountInfo(consent_level);
  return account_info.email == email;
}

}  // namespace

SupervisionMixin::SupervisionMixin(
    InProcessBrowserTestMixinHost& test_mixin_host,
    InProcessBrowserTest* test_base,
    raw_ptr<net::EmbeddedTestServer> embedded_test_server,
    const Options& options)
    : InProcessBrowserTestMixin(&test_mixin_host),
      test_base_(test_base),
      fake_gaia_mixin_(&test_mixin_host),
      embedded_test_server_setup_mixin_(std::in_place,
                                        test_mixin_host,
                                        test_base,
                                        embedded_test_server,
                                        options.embedded_test_server_options),
      api_mock_setup_mixin_(test_mixin_host, test_base),
      google_auth_state_waiter_mixin_(
          test_mixin_host,
          test_base,
          GetExpectedAuthState(options.sign_in_mode)),
      consent_level_(options.consent_level),
      email_(options.email),
      sign_in_mode_(options.sign_in_mode) {}

SupervisionMixin::~SupervisionMixin() = default;

void SupervisionMixin::SetUpCommandLine(base::CommandLine* command_line) {
  AddHostResolverRule(command_line, "accounts.google.com",
                      *fake_gaia_mixin_.gaia_server());
}

void SupervisionMixin::SetUpInProcessBrowserTestFixture() {
  subscription_ =
      BrowserContextDependencyManager::GetInstance()
          ->RegisterCreateServicesCallbackForTesting(
              base::BindRepeating(&OnWillCreateBrowserContextServices));
}

void SupervisionMixin::SetUpOnMainThread() {
  SetUpIdentityTestEnvironment();
  ConfigureIdentityTestEnvironment();
}

// static
ChildAccountService::AuthState SupervisionMixin::GetExpectedAuthState(
    SignInMode sign_in_mode) {
  switch (sign_in_mode) {
    case SignInMode::kSignedOut:
      return ChildAccountService::AuthState::NOT_AUTHENTICATED;
    case SignInMode::kRegular:
    case SignInMode::kSupervised:
      return ChildAccountService::AuthState::AUTHENTICATED;
  }
}

void SupervisionMixin::SetUpIdentityTestEnvironment() {
  adaptor_ =
      std::make_unique<IdentityTestEnvironmentProfileAdaptor>(GetProfile());
}

void SupervisionMixin::ConfigureParentalControls(bool is_supervised_profile) {
  if (is_supervised_profile) {
    SetParentalControlsAccountCapability(true);
    EnableParentalControls(*GetProfile()->GetPrefs());
  } else {
    DisableParentalControls(*GetProfile()->GetPrefs());
  }
}

void SupervisionMixin::SetParentalControlsAccountCapability(
    bool is_supervised_profile) {
  auto* identity_manager = GetIdentityTestEnvironment()->identity_manager();
  CoreAccountInfo account_info =
      identity_manager->GetPrimaryAccountInfo(consent_level_);
  CHECK_EQ(account_info.email, email_);
  AccountInfo account = identity_manager->FindExtendedAccountInfo(account_info);

  AccountCapabilitiesTestMutator mutator(&account.capabilities);
  mutator.set_is_subject_to_parental_controls(is_supervised_profile);
  mutator.set_can_fetch_family_member_info(is_supervised_profile);
  signin::UpdateAccountInfoForAccount(identity_manager, account);
}

void SupervisionMixin::SetPendingStateForPrimaryAccount() {
  CHECK_NE(sign_in_mode_, SignInMode::kSignedOut);
  // Getting into pending state pre-Uno requires the user to sync.
  CHECK(consent_level_ == signin::ConsentLevel::kSync ||
        base::FeatureList::IsEnabled(
            switches::kExplicitBrowserSigninUIOnDesktop));

  auto* identity_manager = GetIdentityTestEnvironment()->identity_manager();

  // Invalidate refresh token and google auth cookie so that the supervised
  // user is in pending state.
  CoreAccountInfo account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  GetIdentityTestEnvironment()->SetInvalidRefreshTokenForAccount(
      account_info.account_id);
  signin::SetFreshnessOfAccountsInGaiaCookie(identity_manager,
                                             /*accounts_are_fresh=*/false);

  signin::AccountsInCookieJarInfo cookie_jar =
      identity_manager->GetAccountsInCookieJar();
  CHECK(!identity_manager->GetAccountsInCookieJar().AreAccountsFresh());
  CHECK(identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin)));
}

void SupervisionMixin::ConfigureIdentityTestEnvironment() {
  switch (sign_in_mode_) {
    case SignInMode::kSignedOut:
      GetIdentityTestEnvironment()->ClearPrimaryAccount();
      return;
    case SignInMode::kRegular:
      fake_gaia_mixin_.SetupFakeGaiaForLogin(
          email_, signin::GetTestGaiaIdForEmail(email_),
          FakeGaiaMixin::kFakeRefreshToken);
      break;
    case SignInMode::kSupervised:
      fake_gaia_mixin_.SetupFakeGaiaForChildUser(
          email_, signin::GetTestGaiaIdForEmail(email_),
          FakeGaiaMixin::kFakeRefreshToken, true);
      break;
  }

  if (!IdentityManagerAlreadyHasPrimaryAccount(
          GetIdentityTestEnvironment()->identity_manager(), email_,
          consent_level_)) {
    // PRE_ tests intentionally leave accounts that are picked up by subsequent
    // test runs.

    // Use the same Gaia id as the one used in the /ListAccounts response from
    // `FakeGaia`.
    // Otherwise, the `SigninManager` will find:
    // - cookie accounts not empty and
    // - the first account in the cookie doesn't have an equivalent extended
    //   account info with the same gaia id.
    // This will lead to clear primary account and removing all accounts.
    AccountInfo account_info =
        GetIdentityTestEnvironment()->MakeAccountAvailable(
            signin::AccountAvailabilityOptionsBuilder()
                .AsPrimary(consent_level_)
                .WithGaiaId(FakeGaia::kDefaultGaiaId)
                .WithRefreshToken(FakeGaiaMixin::kFakeRefreshToken)
                .Build(email_));
    CHECK(!account_info.account_id.empty());
  } else {
    GetIdentityTestEnvironment()->SetRefreshTokenForPrimaryAccount();
  }

  GetIdentityTestEnvironment()->SetAutomaticIssueOfAccessTokens(true);
  GetIdentityTestEnvironment()->SetFreshnessOfAccountsInGaiaCookie(true);
  ConfigureParentalControls(
      /*is_supervised_profile=*/sign_in_mode_ == SignInMode::kSupervised);
}

Profile* SupervisionMixin::GetProfile() const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return ProfileManager::GetActiveUserProfile();
#else
  return test_base_->browser()->profile();
#endif
}

signin::IdentityTestEnvironment* SupervisionMixin::GetIdentityTestEnvironment()
    const {
  CHECK(adaptor_->identity_test_env())
      << "Do not use before the environment is set up.";
  return adaptor_->identity_test_env();
}

void SupervisionMixin::SetNextReAuthStatus(
    GaiaAuthConsumer::ReAuthProofTokenStatus status) {
  fake_gaia_mixin_.fake_gaia()->SetNextReAuthStatus(status);
}

void SupervisionMixin::SignIn(SignInMode mode) {
  CHECK_NE(mode, SignInMode::kSignedOut);

  sign_in_mode_ = mode;
  ConfigureIdentityTestEnvironment();
}

std::ostream& operator<<(std::ostream& stream,
                         SupervisionMixin::SignInMode sign_in_mode) {
  stream << SignInModeAsString(sign_in_mode);
  return stream;
}
std::string SignInModeAsString(SupervisionMixin::SignInMode sign_in_mode) {
  switch (sign_in_mode) {
    case SupervisionMixin::SignInMode::kSignedOut:
      return "SignedOut";
    case SupervisionMixin::SignInMode::kRegular:
      return "Regular";
    case SupervisionMixin::SignInMode::kSupervised:
      return "Supervised";
    default:
      NOTREACHED();
  }
}

}  // namespace supervised_user
