// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/ash/signin_helper.h"

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/account_manager/account_apps_availability.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_upsertion_result.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "components/account_manager_core/chromeos/account_manager_mojo_service.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"

namespace ash {

namespace {
constexpr char kSecondaryGoogleAccountUsageHistogramName[] =
    "Enterprise.SecondaryGoogleAccountUsage.PolicyFetch.Status";
}  // namespace

using SigninRestrictionPolicyFetcher = UserCloudSigninRestrictionPolicyFetcher;

SigninHelper::ArcHelper::ArcHelper(
    bool is_available_in_arc,
    bool is_account_addition,
    AccountAppsAvailability* account_apps_availability)
    : is_available_in_arc_(is_available_in_arc),
      is_account_addition_(is_account_addition),
      account_apps_availability_(account_apps_availability) {}

SigninHelper::ArcHelper::~ArcHelper() = default;

void SigninHelper::ArcHelper::SetIsAvailableInArc(bool is_available_in_arc) {
  is_available_in_arc_ = is_available_in_arc;
}

bool SigninHelper::ArcHelper::IsAvailableInArc() const {
  return is_available_in_arc_;
}

void SigninHelper::ArcHelper::OnAccountAdded(
    const account_manager::Account& account) {
  // Don't change ARC availability after reauthentication.
  if (!is_account_addition_)
    return;

  account_apps_availability_->SetIsAccountAvailableInArc(account,
                                                         is_available_in_arc_);
}

SigninHelper::SigninHelper(
    account_manager::AccountManager* account_manager,
    crosapi::AccountManagerMojoService* account_manager_mojo_service,
    const base::RepeatingClosure& close_dialog_closure,
    const base::RepeatingCallback<void(const std::string&, const std::string&)>&
        show_signin_error,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<ArcHelper> arc_helper,
    const std::string& gaia_id,
    const std::string& email,
    const std::string& auth_code,
    const std::string& signin_scoped_device_id)
    : account_manager_(account_manager),
      account_manager_mojo_service_(account_manager_mojo_service),
      arc_helper_(std::move(arc_helper)),
      close_dialog_closure_(close_dialog_closure),
      show_signin_error_(show_signin_error),
      account_key_(gaia_id, account_manager::AccountType::kGaia),
      email_(email),
      url_loader_factory_(std::move(url_loader_factory)),
      gaia_auth_fetcher_(this, gaia::GaiaSource::kChrome, url_loader_factory_) {
  DCHECK(!signin_scoped_device_id.empty());
  CHECK(show_signin_error_);

  if (AccountAppsAvailability::IsArcAccountRestrictionsEnabled() ||
      AccountAppsAvailability::IsArcManagedAccountRestrictionEnabled()) {
    DCHECK(arc_helper_);
  }

  if (!IsInitialPrimaryAccount()) {
    restriction_fetcher_ =
        std::make_unique<UserCloudSigninRestrictionPolicyFetcher>(
            email_, url_loader_factory_);
  }

  gaia_auth_fetcher_.StartAuthCodeForOAuth2TokenExchangeWithDeviceId(
      auth_code, signin_scoped_device_id);
}

SigninHelper::~SigninHelper() = default;

bool SigninHelper::IsAvailableInArc() const {
  return arc_helper_ && arc_helper_->IsAvailableInArc();
}

void SigninHelper::OnClientOAuthSuccess(const ClientOAuthResult& result) {
  refresh_token_ = result.refresh_token;
  if (!IsInitialPrimaryAccount()) {
    restriction_fetcher_->GetSecondaryGoogleAccountUsage(
        /*access_token_fetcher=*/GaiaAccessTokenFetcher::
            CreateExchangeRefreshTokenForAccessTokenInstance(
                restriction_fetcher_.get(), url_loader_factory_,
                refresh_token_),
        /*callback=*/base::BindOnce(
            &SigninHelper::OnGetSecondaryGoogleAccountUsage,
            weak_factory_.GetWeakPtr()));
    return;
  }
  UpsertAccount(refresh_token_);
  CloseDialogAndExit();
}

void SigninHelper::OnClientOAuthFailure(const GoogleServiceAuthError& error) {
  LOG(ERROR) << "SigninHelper::OnClientOAuthFailure: couldn't fetch OAuth2 "
                "token, the error was "
             << error.ToString();

  show_signin_error_.Run(email_, /*hosted_domain=*/std::string());

  // Notify `AccountManagerMojoService` about account addition failure and send
  // `error`.
  account_manager_mojo_service_->OnAccountUpsertionFinished(
      account_manager::AccountUpsertionResult::FromError(error));
  Exit();
}

void SigninHelper::UpsertAccount(const std::string& refresh_token) {
  // Flow of control after this call:
  // |AccountManager::UpsertAccount| updates / inserts the account and calls
  // its |Observer|s, one of which is
  // |ProfileOAuth2TokenServiceDelegateChromeOS|.
  // |ProfileOAuth2TokenServiceDelegateChromeOS::OnTokenUpserted| seeds the
  // Gaia id and email id for this account in |AccountTrackerService| and
  // invokes |FireRefreshTokenAvailable|. This causes the account to propagate
  // throughout the Identity Service chain, including in
  // |AccountFetcherService|. |AccountFetcherService::OnRefreshTokenAvailable|
  // invokes |AccountTrackerService::StartTrackingAccount|, triggers a fetch
  // for the account information from Gaia and updates this information into
  // |AccountTrackerService|. At this point the account will be fully added to
  // the system.
  account_manager_->UpsertAccount(account_key_, email_, refresh_token);

  auto new_account = account_manager::Account{account_key_, email_};
  if (AccountAppsAvailability::IsArcAccountRestrictionsEnabled() ||
      AccountAppsAvailability::IsArcManagedAccountRestrictionEnabled()) {
    arc_helper_->OnAccountAdded(new_account);
  }
  // Notify `AccountManagerMojoService` about successful account addition and
  // send the account.
  account_manager_mojo_service_->OnAccountUpsertionFinished(
      account_manager::AccountUpsertionResult::FromAccount(
          account_manager::Account{account_key_, email_}));
}

void SigninHelper::CloseDialogAndExit() {
  close_dialog_closure_.Run();
  Exit();
}

void SigninHelper::Exit() {
  base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE, this);
}

// Check if the account being added is allowed to sign-in. If the account is
// allowed, add it to crOS Account Manager. Otherwise stop the sign-in flow and
// show an error page.
void SigninHelper::OnGetSecondaryGoogleAccountUsage(
    SigninRestrictionPolicyFetcher::Status status,
    std::optional<std::string> policy_result,
    const std::string& hosted_domain) {
  base::UmaHistogramEnumeration(kSecondaryGoogleAccountUsageHistogramName,
                                status);

  if (status ==
      SigninRestrictionPolicyFetcher::Status::kUnsupportedAccountTypeError) {
    // SecondaryGoogleAccountUsage policy does not apply to non enterprise
    // accounts.
    UpsertAccount(refresh_token_);
    CloseDialogAndExit();
    return;
  }

  if (status != SigninRestrictionPolicyFetcher::Status::kSuccess) {
    ShowSigninBlockedErrorPageAndExit(/*hosted_domain=*/std::string());
    return;
  }

  if (policy_result.has_value() &&
      policy_result.value() ==
          SigninRestrictionPolicyFetcher::
              kSecondaryGoogleAccountUsagePolicyValuePrimaryAccountSignin) {
    // The sign-in is blocked by SecondaryGoogleAccountUsage policy.
    // Notify `AccountManagerMojoService` about account addition failure and
    // send `error`.
    account_manager_mojo_service_->OnAccountUpsertionFinished(
        account_manager::AccountUpsertionResult::FromStatus(
            account_manager::AccountUpsertionResult::Status::kBlockedByPolicy));
    ShowSigninBlockedErrorPageAndExit(hosted_domain);
    return;
  }

  if (AccountAppsAvailability::IsArcManagedAccountRestrictionEnabled()) {
    restriction_fetcher_->GetSecondaryAccountAllowedInArcPolicy(
        /*access_token_fetcher=*/GaiaAccessTokenFetcher::
            CreateExchangeRefreshTokenForAccessTokenInstance(
                restriction_fetcher_.get(), url_loader_factory_,
                refresh_token_),
        /*callback=*/base::BindOnce(
            &SigninHelper::OnGetSecondaryAccountAllowedInArcPolicy,
            weak_factory_.GetWeakPtr()));
    return;
  }

  // Enterprise accounts with no restrictions are allow to sign-in.
  UpsertAccount(refresh_token_);
  CloseDialogAndExit();
}

void SigninHelper::OnGetSecondaryAccountAllowedInArcPolicy(
    SigninRestrictionPolicyFetcher::Status status,
    std::optional<bool> policy_result) {
  if (status != SigninRestrictionPolicyFetcher::Status::kSuccess) {
    // If we can't fetch the policy, we don't know if we can sync to ARC.
    // Block adding the account as a safety measure.
    ShowSigninBlockedErrorPageAndExit(/*hosted_domain=*/std::string());
    return;
  }

  DCHECK(policy_result);
  arc_helper_->SetIsAvailableInArc(policy_result.value());

  UpsertAccount(refresh_token_);
  CloseDialogAndExit();
}

void SigninHelper::ShowSigninBlockedErrorPageAndExit(
    const std::string& hosted_domain) {
  show_signin_error_.Run(email_, hosted_domain);
  RevokeGaiaTokenOnServer();
}

void SigninHelper::RevokeGaiaTokenOnServer() {
  // Revokes refresh token due to the account is not allowed to sign in.
  gaia_auth_fetcher_.StartRevokeOAuth2Token(refresh_token_);
}

// Notifies about GAIA token revocation status and call `Exit()` to delete
// `this` object.
void SigninHelper::OnOAuth2RevokeTokenCompleted(
    GaiaAuthConsumer::TokenRevocationStatus status) {
  if (status == GaiaAuthConsumer::TokenRevocationStatus::kSuccess) {
    DVLOG(1) << "GaiaTokenRevocationRequest::OnOAuth2RevokeTokenCompleted";
  } else {
    LOG(ERROR) << "GaiaTokenRevocationRequest::OnOAuth2RevokeTokenCompleted "
                  "returned with an error";
  }
  Exit();
}

bool SigninHelper::IsInitialPrimaryAccount() {
  return user_manager::UserManager::Get()
             ->GetPrimaryUser()
             ->GetAccountId()
             .GetGaiaId() == account_key_.id();
}

account_manager::AccountManager* SigninHelper::GetAccountManager() {
  return account_manager_;
}

std::string SigninHelper::GetEmail() {
  return email_;
}

scoped_refptr<network::SharedURLLoaderFactory>
SigninHelper::GetUrlLoaderFactory() {
  return url_loader_factory_;
}
}  // namespace ash
