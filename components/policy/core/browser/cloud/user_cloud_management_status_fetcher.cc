// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/cloud/user_cloud_management_status_fetcher.h"

#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/policy/core/common/cloud/dmserver_job_configurations.h"
#include "components/policy/core/common/features.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_proto_decoders.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/signin/public/base/oauth_consumer_id.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/core_account_id.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace policy {

namespace {

const base::Value* GetMandatoryPolicyValue(const PolicyMap& policy_map,
                                           const char* policy_name,
                                           base::Value::Type value_type) {
  const PolicyMap::Entry* entry = policy_map.Get(policy_name);
  if (!entry || entry->level != POLICY_LEVEL_MANDATORY) {
    return nullptr;
  }
  return entry->value(value_type);
}

std::optional<ProfileSeparationPolicies> ExtractProfileSeparationPolicies(
    const enterprise_management::PolicyFetchResponse& policy_fetch_response) {
  if ((policy_fetch_response.has_error_code() &&
       policy_fetch_response.error_code() != net::HTTP_OK) ||
      !policy_fetch_response.error_message().empty()) {
    LOG_POLICY(WARNING, POLICY_FETCHING)
        << "UserManagementStatusAndPolicies policy_fetch_response contained "
        << "error code: " << policy_fetch_response.error_code()
        << ", message: " << policy_fetch_response.error_message();
    return std::nullopt;
  }

  enterprise_management::PolicyData policy_data;
  if (!policy_data.ParseFromString(policy_fetch_response.policy_data()) ||
      !policy_data.has_policy_value()) {
    LOG_POLICY(WARNING, POLICY_FETCHING)
        << "Failed to parse PolicyData or missing policy_value in "
        << "UserManagementStatusAndPolicies.";
    return std::nullopt;
  }

  // TODO(crbug.com/543381833): Use PolicyValidator to validate this policy.
  enterprise_management::CloudPolicySettings cloud_policy_settings;
  if (!cloud_policy_settings.ParseFromString(policy_data.policy_value())) {
    LOG_POLICY(WARNING, POLICY_FETCHING)
        << "Failed to parse CloudPolicySettings from PolicyData.";
    return std::nullopt;
  }

  PolicyMap policy_map;
  DecodeProtoFields(cloud_policy_settings, /*external_data_manager=*/nullptr,
                    POLICY_SOURCE_CLOUD, POLICY_SCOPE_USER, &policy_map,
                    PolicyPerProfileFilter::kAny);

  std::optional<int> profile_separation_settings;
#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_IOS)
  if (const base::Value* v =
          GetMandatoryPolicyValue(policy_map, key::kProfileSeparationSettings,
                                  base::Value::Type::INTEGER)) {
    profile_separation_settings = v->GetInt();
  }
#endif

  std::optional<int> profile_separation_data_migration_settings;
#if !BUILDFLAG(IS_CHROMEOS)
  if (const base::Value* v = GetMandatoryPolicyValue(
          policy_map, key::kProfileSeparationDataMigrationSettings,
          base::Value::Type::INTEGER)) {
    profile_separation_data_migration_settings = v->GetInt();
  }
#endif

  std::optional<std::string> managed_accounts_signin_restrictions;
#if !BUILDFLAG(IS_IOS)
  if (const base::Value* v = GetMandatoryPolicyValue(
          policy_map, key::kManagedAccountsSigninRestriction,
          base::Value::Type::STRING)) {
    managed_accounts_signin_restrictions = v->GetString();
  }
#endif

  return ProfileSeparationPolicies(
      profile_separation_settings, profile_separation_data_migration_settings,
      std::move(managed_accounts_signin_restrictions));
}

}  // namespace

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
         ProfileSeparationPolicies profile_separation_policies) {
        std::move(callback).Run(std::move(status),
                                std::move(profile_separation_policies));
      },
      std::move(fetcher), std::move(callback));
  raw_fetcher->Start(identity_manager, account_id,
                     std::move(on_status_received));
}

UserCloudManagementStatusFetcher::UserCloudManagementStatusFetcher(
    DeviceManagementService* service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    bool should_fetch_policies)
    : service_(CHECK_DEREF(service)),
      url_loader_factory_(std::move(url_loader_factory)),
      should_fetch_policies_(should_fetch_policies) {
  CHECK(features::IsMigrateSecureConnectApiToDmServerEnabled());
  CHECK(url_loader_factory_);
}

UserCloudManagementStatusFetcher::~UserCloudManagementStatusFetcher() = default;

void UserCloudManagementStatusFetcher::Start(
    signin::IdentityManager* identity_manager,
    const CoreAccountId& account_id,
    FetchStatusAndPoliciesCallback callback) {
  CHECK(!callback_);
  CHECK(!token_fetcher_);
  CHECK(identity_manager);
  CHECK(!account_id.empty());
  callback_ = std::move(callback);

  // Start the fetch timeout timer.
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
    Finish(std::nullopt, ProfileSeparationPolicies());
    return;
  }

  SendDeviceManagementRequest(std::move(access_token_info.token));
}

void UserCloudManagementStatusFetcher::SendDeviceManagementRequest(
    std::string access_token) {
  DMServerJobConfiguration::CreateParams params =
      DMServerJobConfiguration::CreateParams::WithoutClient(
          DeviceManagementService::JobConfiguration::
              TYPE_USER_MANAGEMENT_STATUS_AND_POLICIES,
          &service_.get(), /*client_id=*/"", url_loader_factory_);
  params.oauth_token = std::move(access_token);
  params.callback = base::BindOnce(&UserCloudManagementStatusFetcher::OnJobDone,
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
    Finish(std::nullopt, ProfileSeparationPolicies());
    return;
  }

  const auto& status_response =
      result.response.user_management_status_and_policies_response();

  UserManagementStatus status;
  status.is_account_managed = status_response.is_account_managed();
  status.is_chrome_profile_management_enabled =
      status_response.is_chrome_profile_management_enabled();

  ProfileSeparationPolicies profile_separation_policies;
  if (status_response.has_policy_fetch_response()) {
    profile_separation_policies = ExtractProfileSeparationPolicies(
                                      status_response.policy_fetch_response())
                                      .value_or(ProfileSeparationPolicies());
  }

  Finish(status, std::move(profile_separation_policies));
}

void UserCloudManagementStatusFetcher::OnTimeout() {
  LOG_POLICY(WARNING, POLICY_FETCHING)
      << "UserCloudManagementStatusFetcher timed out.";
  // Fail open on timeout.
  Finish(std::nullopt, ProfileSeparationPolicies());
}

void UserCloudManagementStatusFetcher::Finish(
    std::optional<UserManagementStatus> status,
    ProfileSeparationPolicies profile_separation_policies) {
  timeout_timer_.Stop();
  token_fetcher_.reset();
  fetch_job_.reset();

  if (callback_) {
    std::move(callback_).Run(std::move(status),
                             std::move(profile_separation_policies));
  }
}

}  // namespace policy
