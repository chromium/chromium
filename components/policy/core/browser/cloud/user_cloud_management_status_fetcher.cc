// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/cloud/user_cloud_management_status_fetcher.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/dmserver_job_configurations.h"
#include "components/policy/core/common/features.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/signin/public/base/oauth_consumer_id.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace policy {

namespace {

UserInterceptionPolicies GetUserInterceptionPoliciesFromProto(
    const enterprise_management::SigninExperiencePolicies& policies_proto) {
  UserInterceptionPolicies policies;
  if (policies_proto.has_sync_disabled()) {
    policies.sync_disabled = policies_proto.sync_disabled();
  }
  std::optional<int> profile_separation_settings;
  if (policies_proto.has_profile_separation_settings()) {
    profile_separation_settings =
        static_cast<int>(policies_proto.profile_separation_settings());
  }
  std::optional<int> profile_separation_data_migration_settings;
  if (policies_proto.has_profile_separation_data_migration_settings()) {
    profile_separation_data_migration_settings = static_cast<int>(
        policies_proto.profile_separation_data_migration_settings());
  }
  std::optional<std::string> managed_accounts_signin_restrictions;
  if (policies_proto.has_managed_accounts_signin_restrictions()) {
    managed_accounts_signin_restrictions =
        policies_proto.managed_accounts_signin_restrictions();
  }

  policies.profile_separation_policies = ProfileSeparationPolicies(
      profile_separation_settings, profile_separation_data_migration_settings,
      std::move(managed_accounts_signin_restrictions));

  if (policies_proto.has_browser_theme_color()) {
    policies.browser_theme_color = policies_proto.browser_theme_color();
  }
  if (policies_proto.has_enterprise_logo_url()) {
    policies.enterprise_logo_url = policies_proto.enterprise_logo_url();
  }
  if (policies_proto.has_enterprise_custom_label()) {
    policies.enterprise_custom_label =
        policies_proto.enterprise_custom_label();
  }
  return policies;
}

}  // namespace

UserManagementStatus::UserManagementStatus() = default;
UserManagementStatus::~UserManagementStatus() = default;
UserManagementStatus::UserManagementStatus(const UserManagementStatus&) =
    default;
UserManagementStatus& UserManagementStatus::operator=(
    const UserManagementStatus&) = default;
UserManagementStatus::UserManagementStatus(UserManagementStatus&&) = default;
UserManagementStatus& UserManagementStatus::operator=(
    UserManagementStatus&&) = default;

UserInterceptionPolicies::UserInterceptionPolicies() = default;
UserInterceptionPolicies::~UserInterceptionPolicies() = default;
UserInterceptionPolicies::UserInterceptionPolicies(
    const UserInterceptionPolicies&) = default;
UserInterceptionPolicies& UserInterceptionPolicies::operator=(
    const UserInterceptionPolicies&) = default;
UserInterceptionPolicies::UserInterceptionPolicies(
    UserInterceptionPolicies&&) = default;
UserInterceptionPolicies& UserInterceptionPolicies::operator=(
    UserInterceptionPolicies&&) = default;

// static
void UserCloudManagementStatusFetcher::FetchStatusAndPolicies(
    DeviceManagementService* service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    const CoreAccountId& account_id,
    bool should_fetch_policies,
    FetchStatusAndPoliciesCallback callback) {
  // Create a new fetcher instance and pass its ownership (`unique_ptr`) into
  // the completion callback closure. The fetcher remains alive for the duration
  // of the asynchronous request (or until timeout) and is automatically
  // deleted when the completion lambda finishes executing.
  auto fetcher = std::make_unique<UserCloudManagementStatusFetcher>(
      service, std::move(url_loader_factory), should_fetch_policies);
  auto* raw_fetcher = fetcher.get();
  auto on_status_received = base::BindOnce(
      [](std::unique_ptr<UserCloudManagementStatusFetcher> fetcher,
         FetchStatusAndPoliciesCallback callback,
         std::optional<UserManagementStatus> status,
         std::optional<UserInterceptionPolicies> interception_policies) {
        std::move(callback).Run(std::move(status),
                                std::move(interception_policies));
      },
      std::move(fetcher), std::move(callback));
  raw_fetcher->Start(
      identity_manager, account_id, std::move(on_status_received));
}

UserCloudManagementStatusFetcher::UserCloudManagementStatusFetcher(
    DeviceManagementService* service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    bool should_fetch_policies)
    : service_(CHECK_DEREF(service)),
      url_loader_factory_(std::move(url_loader_factory)),
      should_fetch_policies_(should_fetch_policies) {
  CHECK(base::FeatureList::IsEnabled(
      features::kMigrateSecureConnectApiToDmServer));
  CHECK(url_loader_factory_);
}

UserCloudManagementStatusFetcher::~UserCloudManagementStatusFetcher() = default;

void UserCloudManagementStatusFetcher::Start(
    signin::IdentityManager* identity_manager,
    const CoreAccountId& account_id,
    InternalFetchCallback callback) {
  callback_ = std::move(callback);

  // Set 10.0s timeout timer.
  timeout_timer_.Start(
      FROM_HERE, features::kMigrateSecureConnectApiToDmServerFetchTimeout.Get(),
      base::BindOnce(&UserCloudManagementStatusFetcher::OnTimeout,
                     weak_factory_.GetWeakPtr()));

  token_fetcher_ = identity_manager->CreateAccessTokenFetcherForAccount(
      account_id, signin::OAuthConsumerId::kCloudPolicyClientRegistration,
      base::BindOnce(
          &UserCloudManagementStatusFetcher::OnAccessTokenFetchComplete,
          weak_factory_.GetWeakPtr()),
      signin::AccessTokenFetcher::Mode::kWaitUntilRefreshTokenAvailable);
}

void UserCloudManagementStatusFetcher::OnAccessTokenFetchComplete(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  token_fetcher_.reset();

  if (error.state() != GoogleServiceAuthError::NONE) {
    LOG_POLICY(WARNING, POLICY_FETCHING)
        << "Failed to fetch access token for management status check: "
        << error.ToString();
    Finish(std::nullopt, std::nullopt);
    return;
  }

  SendDeviceManagementRequest(access_token_info.token);
}

void UserCloudManagementStatusFetcher::SendDeviceManagementRequest(
    const std::string& access_token) {
  DMServerJobConfiguration::CreateParams params =
      DMServerJobConfiguration::CreateParams::WithoutClient(
          DeviceManagementService::JobConfiguration::
              TYPE_USER_MANAGEMENT_STATUS_AND_POLICIES,
          &service_.get(), /*client_id=*/"", url_loader_factory_);
  params.oauth_token = access_token;
  params.callback =
      base::BindOnce(&UserCloudManagementStatusFetcher::OnJobDone,
                     weak_factory_.GetWeakPtr());

  auto config = std::make_unique<DMServerJobConfiguration>(std::move(params));

  auto* status_request =
      config->request()->mutable_user_management_status_and_policies_request();
  status_request->set_should_fetch_policies(should_fetch_policies_);

  fetch_job_ = service_->CreateJob(std::move(config));
}

void UserCloudManagementStatusFetcher::OnJobDone(DMServerJobResult result) {
  fetch_job_.reset();

  if (result.dm_status != DM_STATUS_SUCCESS ||
      !result.response.has_user_management_status_and_policies_response()) {
    LOG_POLICY(WARNING, POLICY_FETCHING)
        << "UserManagementStatusAndPolicies failed with status: "
        << result.dm_status;
    // Fail open on error.
    Finish(std::nullopt, std::nullopt);
    return;
  }

  const auto& status_response =
      result.response.user_management_status_and_policies_response();

  UserManagementStatus status;
  status.is_account_managed = status_response.is_account_managed();
  status.is_chrome_profile_management_enabled =
      status_response.is_chrome_profile_management_enabled();

  std::optional<UserInterceptionPolicies> interception_policies;
  if (status_response.has_signin_experience_policies()) {
    interception_policies = GetUserInterceptionPoliciesFromProto(
        status_response.signin_experience_policies());
  }

  Finish(status, std::move(interception_policies));
}

void UserCloudManagementStatusFetcher::OnTimeout() {
  LOG_POLICY(WARNING, POLICY_FETCHING)
      << "UserCloudManagementStatusFetcher timed out.";
  // Fail open on timeout.
  Finish(std::nullopt, std::nullopt);
}

void UserCloudManagementStatusFetcher::Finish(
    std::optional<UserManagementStatus> status,
    std::optional<UserInterceptionPolicies> interception_policies) {
  timeout_timer_.Stop();
  token_fetcher_.reset();
  fetch_job_.reset();

  if (callback_) {
    std::move(callback_).Run(std::move(status),
                            std::move(interception_policies));
  }
}

}  // namespace policy
