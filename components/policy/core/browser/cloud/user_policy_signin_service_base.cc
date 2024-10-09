// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/cloud/user_policy_signin_service_base.h"

#include <utility>

#include "base/dcheck_is_on.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/policy/core/common/cloud/cloud_policy_client_registration_helper.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/profile_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/policy/core/common/policy_types.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"

namespace em = enterprise_management;

namespace policy {

namespace {

#if BUILDFLAG(IS_ANDROID)
const em::DeviceRegisterRequest::Type kCloudPolicyRegistrationType =
    em::DeviceRegisterRequest::ANDROID_BROWSER;
#elif BUILDFLAG(IS_IOS)
const em::DeviceRegisterRequest::Type kCloudPolicyRegistrationType =
    em::DeviceRegisterRequest::IOS_BROWSER;
#else
const em::DeviceRegisterRequest::Type kCloudPolicyRegistrationType =
    em::DeviceRegisterRequest::BROWSER;
#endif

}  // namespace

UserPolicySigninServiceBase::UserPolicySigninServiceBase(
    PrefService* local_state,
    DeviceManagementService* device_management_service,
    absl::variant<UserCloudPolicyManager*, ProfileCloudPolicyManager*>
        policy_manager,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory)
    : user_policy_manager_(
          absl::holds_alternative<UserCloudPolicyManager*>(policy_manager)
              ? absl::get<UserCloudPolicyManager*>(policy_manager)
              : nullptr),
      policy_manager_(
          absl::holds_alternative<UserCloudPolicyManager*>(policy_manager)
              ? static_cast<CloudPolicyManager*>(
                    absl::get<UserCloudPolicyManager*>(policy_manager))
              : static_cast<CloudPolicyManager*>(
                    absl::get<ProfileCloudPolicyManager*>(policy_manager))),
      identity_manager_(identity_manager),
      local_state_(local_state),
      device_management_service_(device_management_service),
      system_url_loader_factory_(system_url_loader_factory) {}

UserPolicySigninServiceBase::~UserPolicySigninServiceBase() = default;

void UserPolicySigninServiceBase::FetchPolicyForSignedInUser(
    const AccountId& account_id,
    const std::string& dm_token,
    const std::string& client_id,
    const std::vector<std::string>& user_affiliation_ids,
    scoped_refptr<network::SharedURLLoaderFactory> profile_url_loader_factory,
    PolicyFetchCallback callback) {
  DVLOG_POLICY(3, POLICY_FETCHING)
      << "Starting policy fetching for signed-in user.";
  CloudPolicyManager* manager = policy_manager();
  DCHECK(manager);

  // Initialize the cloud policy manager there was no prior initialization.
  if (!manager->core()->client()) {
    // TODO(b/301259161): Because user cloud policy fetch and registration are
    // using different CloudPolicyClient instance. We won't be able to get
    // user affiliation ids for fetch request right after registration until
    // browser is relaunched with current implementation.
    // We need to find a way to forward the ids from registration client to
    // here. (Or the caller of this function, if client is created ahead of
    // time).
    std::unique_ptr<CloudPolicyClient> client =
        CreateClientForNonRegistration(profile_url_loader_factory);
    client->SetupRegistration(dm_token, client_id, user_affiliation_ids);
    DCHECK(client->is_registered());
    DCHECK(!manager->core()->client());
    InitializeCloudPolicyManager(account_id, std::move(client));
    // `CloudPolicyManager` will initiate a policy fetch right after
    // initialization. Invoke `callback` after the policy is fetched.
    policy_fetch_callbacks().AddUnsafe(std::move(callback));
    return;
  }

  if (!manager->IsClientRegistered()) {
    // The manager already has a client but it's still registering.
    // `CloudPolicyManager` will initiate a policy fetch when the client
    // registration completes. Invoke `callback` after the policy is fetched.
    policy_fetch_callbacks().AddUnsafe(std::move(callback));
    return;
  }

  // Now initiate a policy fetch.
  manager->core()->service()->RefreshPolicy(std::move(callback),
                                            PolicyFetchReason::kSignin);
}

void UserPolicySigninServiceBase::OnPolicyFetched(CloudPolicyClient* client) {}

void UserPolicySigninServiceBase::OnRegistrationStateChanged(
    CloudPolicyClient* client) {}

void UserPolicySigninServiceBase::OnClientError(CloudPolicyClient* client) {
  if (client->is_registered()) {
    // If the client is already registered, it means this error must have
    // come from a policy fetch.
    if (client->last_dm_status() ==
        DM_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED) {
      // OK, policy fetch failed with MANAGEMENT_NOT_SUPPORTED - this is our
      // trigger to revert to "unmanaged" mode (we will check for management
      // being re-enabled on the next restart and/or login).
      DVLOG_POLICY(1, POLICY_FETCHING)
          << "DMServer returned NOT_SUPPORTED error - removing policy";

      // Can't shutdown now because we're in the middle of a callback from
      // the CloudPolicyClient, so queue up a task to do the shutdown.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &UserPolicySigninServiceBase::ShutdownCloudPolicyManager,
              weak_factory_.GetWeakPtr()));
    } else {
      DVLOG_POLICY(1, POLICY_FETCHING)
          << "Error fetching policy with DM status: "
          << client->last_dm_status();
    }
  }
}

void UserPolicySigninServiceBase::Shutdown() {
  PrepareForCloudPolicyManagerShutdown();
}

void UserPolicySigninServiceBase::PrepareForCloudPolicyManagerShutdown() {
  registration_helper_.reset();
  registration_helper_for_temporary_client_.reset();
  // Don't run the callbacks to be consistent with
  // `CloudPolicyService::RefreshPolicy()` behavior during shutdown.
  policy_fetch_callbacks_.reset();
  CloudPolicyManager* manager = policy_manager();
  if (manager && manager->core()->client())
    manager->core()->client()->RemoveObserver(this);
  if (manager && manager->core()->service())
    manager->core()->service()->RemoveObserver(this);
  registration_callback_.Cancel();
}

base::OnceCallbackList<void(bool)>&
UserPolicySigninServiceBase::policy_fetch_callbacks() {
  if (!policy_fetch_callbacks_) {
    policy_fetch_callbacks_ =
        std::make_unique<base::OnceCallbackList<void(bool)>>();
  }
  return *policy_fetch_callbacks_;
}

std::unique_ptr<CloudPolicyClient>
UserPolicySigninServiceBase::CreateClientForRegistrationOnly(
    const std::string& username) {
  DCHECK(!username.empty());
  // We should not be called with a client already registered.
  DCHECK(!policy_manager() || !policy_manager()->IsClientRegistered());

  // If the user should not get policy, just bail out.
  if (!policy_manager() || !ShouldLoadPolicyForUser(username)) {
    DVLOG_POLICY(1, POLICY_FETCHING)
        << "Signed-in user is not in the allowlist";
    return nullptr;
  }

  // If the DeviceManagementService is not yet initialized, start it up now.
  device_management_service_->ScheduleInitialization(0);

  // Create a new CloudPolicyClient for fetching the DMToken.
  return std::make_unique<CloudPolicyClient>(
      GetProfileId(), device_management_service_, system_url_loader_factory_,
      CloudPolicyClient::DeviceDMTokenCallback());
}

std::unique_ptr<CloudPolicyClient>
UserPolicySigninServiceBase::CreateClientForNonRegistration(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  return std::make_unique<CloudPolicyClient>(
      GetProfileId(), device_management_service_, std::move(url_loader_factory),
      GetDeviceDMTokenIfAffiliatedCallback());
}

bool UserPolicySigninServiceBase::ShouldLoadPolicyForUser(
    const std::string& username) {
  if (username.empty())
    return false;  // Not signed in.

  return signin::AccountManagedStatusFinder::MayBeEnterpriseUserBasedOnEmail(
      username);
}

void UserPolicySigninServiceBase::InitializeForSignedInUser(
    const AccountId& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> profile_url_loader_factory) {
  if (user_policy_manager_) {
    DCHECK(account_id.is_valid());
    bool should_load_policies =
        ShouldLoadPolicyForUser(account_id.GetUserEmail());
    user_policy_manager_->SetPoliciesRequired(should_load_policies,
                                              PolicyFetchReason::kSignin);
    if (!should_load_policies) {
      DVLOG_POLICY(1, POLICY_FETCHING)
          << "Policy load not enabled for user: " << account_id.GetUserEmail();
      return;
    }
  }

  CloudPolicyManager* manager = policy_manager();
  // Initialize the UCPM if it is not already initialized.
  if (!manager->core()->service()) {
    // If there is no cached DMToken then we can detect this when the
    // OnCloudPolicyServiceInitializationCompleted() callback is invoked and
    // this will initiate a policy fetch.
    InitializeCloudPolicyManager(
        account_id, CreateClientForNonRegistration(profile_url_loader_factory));
  } else if (user_policy_manager_) {
    user_policy_manager_->SetSigninAccountId(account_id);
  }

  // If the CloudPolicyService is initialized, kick off registration.
  // Otherwise OnCloudPolicyServiceInitializationCompleted is invoked as soon as
  // the service finishes its initialization.
  if (manager->core()->service()->IsInitializationComplete())
    OnCloudPolicyServiceInitializationCompleted();
}

void UserPolicySigninServiceBase::InitializeCloudPolicyManager(
    const AccountId& account_id,
    std::unique_ptr<CloudPolicyClient> client) {
  DCHECK(client);
  CloudPolicyManager* manager = policy_manager();
  if (user_policy_manager_) {
    user_policy_manager_->SetSigninAccountId(account_id);
  }
  DCHECK(!manager->core()->client());
  manager->Connect(local_state_, std::move(client));
  DCHECK(manager->core()->service());

  // Observe the client to detect errors fetching policy.
  manager->core()->client()->AddObserver(this);
  // Observe the service to determine when it's initialized.
  manager->core()->service()->AddObserver(this);
}

void UserPolicySigninServiceBase::ShutdownCloudPolicyManager() {
  PrepareForCloudPolicyManagerShutdown();
  CloudPolicyManager* manager = policy_manager();
  if (manager)
    manager->DisconnectAndRemovePolicy();
}

void UserPolicySigninServiceBase::CancelPendingRegistration() {
  weak_factory_for_registration_.InvalidateWeakPtrs();
  registration_helper_.reset();
  registration_helper_for_temporary_client_.reset();
}

void UserPolicySigninServiceBase::
    CallPolicyRegistrationCallbackForTemporaryClient(
        std::unique_ptr<CloudPolicyClient> client,
        PolicyRegistrationCallback callback) {
  registration_helper_for_temporary_client_.reset();
  std::move(callback).Run(client->dm_token(), client->client_id(),
                          client->user_affiliation_ids());
}

void UserPolicySigninServiceBase::RegisterForPolicyWithAccountId(
    const std::string& username,
    const CoreAccountId& account_id,
    PolicyRegistrationCallback callback) {
  DCHECK(!account_id.empty());

  if (policy_manager() && policy_manager()->IsClientRegistered()) {
    // Reuse the already fetched DM token if the client of the manager is
    // already registered.
    std::move(callback).Run(
        policy_manager()->core()->client()->dm_token(),
        policy_manager()->core()->client()->client_id(),
        policy_manager()->core()->client()->user_affiliation_ids());
    return;
  }

  // Create a new CloudPolicyClient for fetching the DMToken. This is a
  // different client from the one used by the manager.
  std::unique_ptr<CloudPolicyClient> policy_client =
      CreateClientForRegistrationOnly(username);
  if (!policy_client) {
    std::move(callback).Run(std::string(), std::string(),
                            std::vector<std::string>());
    return;
  }

  // Fire off the registration process. Callback owns and keeps the
  // CloudPolicyClient alive for the length of the registration process.
  // Cancels in-progress registration triggered previously via
  // `RegisterForPolicyWithAccountId()`, if any.
  registration_helper_for_temporary_client_ =
      std::make_unique<CloudPolicyClientRegistrationHelper>(
          policy_client.get(), kCloudPolicyRegistrationType);

  // Using a raw pointer to |this| is okay, because the service owns
  // |registration_helper_for_temporary_client_|.
  auto registration_callback = base::BindOnce(
      &UserPolicySigninServiceBase::
          CallPolicyRegistrationCallbackForTemporaryClient,
      base::Unretained(this), std::move(policy_client), std::move(callback));
  registration_helper_for_temporary_client_->StartRegistration(
      identity_manager(), account_id, std::move(registration_callback));
}

void UserPolicySigninServiceBase::SetDeviceDMTokenCallbackForTesting(
    CloudPolicyClient::DeviceDMTokenCallback callback) {
  device_dm_token_callback_for_testing_ = std::move(callback);
}

void UserPolicySigninServiceBase::RegisterCloudPolicyService() {
  DCHECK(
      identity_manager()->HasPrimaryAccount(GetConsentLevelForRegistration()));
  DCHECK(policy_manager()->core()->client());
  DCHECK(!policy_manager()->IsClientRegistered());

  DVLOG_POLICY(1, POLICY_FETCHING) << "Fetching new DM Token";

  // Do nothing if already starting the registration process in which case there
  // will be an instance of |registration_helper_|.
  if (registration_helper_)
    return;

  UpdateLastPolicyCheckTime();

  // Start the process of registering the CloudPolicyClient. Once it completes,
  // policy fetch will automatically happen.
  registration_helper_ = std::make_unique<CloudPolicyClientRegistrationHelper>(
      policy_manager()->core()->client(), kCloudPolicyRegistrationType);
  registration_helper_->StartRegistration(
      identity_manager(),
      identity_manager()->GetPrimaryAccountId(GetConsentLevelForRegistration()),
      base::BindOnce(&UserPolicySigninServiceBase::OnRegistrationComplete,
                     base::Unretained(this)));
}

CloudPolicyClient::DeviceDMTokenCallback
UserPolicySigninServiceBase::GetDeviceDMTokenIfAffiliatedCallback() {
  return CloudPolicyClient::DeviceDMTokenCallback();
}

void UserPolicySigninServiceBase::OnRegistrationComplete() {
  ProhibitSignoutIfNeeded();
  registration_helper_.reset();
}

base::TimeDelta UserPolicySigninServiceBase::GetTryRegistrationDelay() {
  return base::TimeDelta();
}

void UserPolicySigninServiceBase::
    OnCloudPolicyServiceInitializationCompleted() {
  CloudPolicyManager* manager = policy_manager();
  DCHECK(manager->core()->service()->IsInitializationComplete());
  // The service is now initialized - if the client is not yet registered, then
  // it means that there is no cached policy and so we need to initiate a new
  // client registration.
  if (manager->IsClientRegistered()) {
    DVLOG_POLICY(1, POLICY_FETCHING)
        << "Client already registered - not fetching DMToken";
    ProhibitSignoutIfNeeded();
    return;
  }

  if (!CanApplyPolicies(/*check_for_refresh_token=*/true)) {
    // No token yet. This can only happen on Desktop platforms which should
    // listen to OnRefreshTokenUpdatedForAccount() and will re-attempt
    // registration once the token is available.
    DLOG_POLICY(WARNING, POLICY_AUTH)
        << "No OAuth Refresh Token - delaying policy download";
    return;
  }

  base::TimeDelta try_registration_delay = GetTryRegistrationDelay();
  if (try_registration_delay.is_zero()) {
    // If the try registration delay is 0, register the cloud policy service
    // immediately without queueing a task. This is the case for Desktop.
    RegisterCloudPolicyService();
  } else {
    registration_callback_.Reset(
        base::BindOnce(&UserPolicySigninServiceBase::RegisterCloudPolicyService,
                       weak_factory_for_registration_.GetWeakPtr()));
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, registration_callback_.callback(), try_registration_delay);
  }

  ProhibitSignoutIfNeeded();
}

void UserPolicySigninServiceBase::OnPolicyRefreshed(bool success) {
  policy_fetch_callbacks().Notify(success);
}

void UserPolicySigninServiceBase::ProhibitSignoutIfNeeded() {}

bool UserPolicySigninServiceBase::CanApplyPolicies(
    bool check_for_refresh_token) {
  return false;
}

void UserPolicySigninServiceBase::UpdateLastPolicyCheckTime() {}

signin::ConsentLevel
UserPolicySigninServiceBase::GetConsentLevelForRegistration() {
  return signin::ConsentLevel::kSignin;
}

std::string_view UserPolicySigninServiceBase::name() const {
  return "UserPolicySigninServiceBase";
}

}  // namespace policy
