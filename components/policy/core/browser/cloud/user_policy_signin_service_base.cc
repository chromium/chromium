// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/cloud/user_policy_signin_service_base.h"

#include <utility>

#include "base/bind.h"
#include "base/dcheck_is_on.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/cloud/cloud_policy_client_registration_helper.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"

namespace em = enterprise_management;

namespace {

#if BUILDFLAG(IS_ANDROID)
const em::DeviceRegisterRequest::Type kCloudPolicyRegistrationType =
    em::DeviceRegisterRequest::ANDROID_BROWSER;
#elif BUILDFLAG(IS_IOS)
// TODO(crbug.com/1312263): Use em::DeviceRegisterRequest::IOS_BROWSER when
// supported in the dmserver. The type for Desktop is temporarily used on iOS
// to allow early testing of the feature before the DMServer can support iOS
// User Policy.
const em::DeviceRegisterRequest::Type kCloudPolicyRegistrationType =
    em::DeviceRegisterRequest::BROWSER;
#else
const em::DeviceRegisterRequest::Type kCloudPolicyRegistrationType =
    em::DeviceRegisterRequest::BROWSER;
#endif

}  // namespace

namespace policy {

UserPolicySigninServiceBase::UserPolicySigninServiceBase(
    PrefService* local_state,
    DeviceManagementService* device_management_service,
    UserCloudPolicyManager* policy_manager,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory)
    : policy_manager_(policy_manager),
      identity_manager_(identity_manager),
      local_state_(local_state),
      device_management_service_(device_management_service),
      system_url_loader_factory_(system_url_loader_factory) {}

UserPolicySigninServiceBase::~UserPolicySigninServiceBase() {}

void UserPolicySigninServiceBase::FetchPolicyForSignedInUser(
    const AccountId& account_id,
    const std::string& dm_token,
    const std::string& client_id,
    scoped_refptr<network::SharedURLLoaderFactory> profile_url_loader_factory,
    PolicyFetchCallback callback) {
  UserCloudPolicyManager* manager = policy_manager();
  DCHECK(manager);

  // Initialize the cloud policy manager there was no prior initialization.
  if (!manager->core()->client()) {
    std::unique_ptr<CloudPolicyClient> client =
        UserCloudPolicyManager::CreateCloudPolicyClient(
            device_management_service_, profile_url_loader_factory);
    client->SetupRegistration(
        dm_token, client_id,
        std::vector<std::string>() /* user_affiliation_ids */);
    DCHECK(client->is_registered());
    DCHECK(!manager->core()->client());
    InitializeUserCloudPolicyManager(account_id, std::move(client));
    // `UserCloudPolicyManager` will initiate a policy fetch right after
    // initialization. Invoke `callback` after the policy is fetched.
    policy_fetch_callbacks().AddUnsafe(std::move(callback));
    return;
  }

  if (!manager->IsClientRegistered()) {
    // The manager already has a client but it's still registering.
    // `UserCloudPolicyManager` will initiate a policy fetch when the client
    // registration completes. Invoke `callback` after the policy is fetched.
    policy_fetch_callbacks().AddUnsafe(std::move(callback));
    return;
  }

  // Now initiate a policy fetch.
  manager->core()->service()->RefreshPolicy(std::move(callback));
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
      DVLOG(1) << "DMServer returned NOT_SUPPORTED error - removing policy";

      // Can't shutdown now because we're in the middle of a callback from
      // the CloudPolicyClient, so queue up a task to do the shutdown.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &UserPolicySigninServiceBase::ShutdownUserCloudPolicyManager,
              weak_factory_.GetWeakPtr()));
    } else {
      DVLOG(1) << "Error fetching policy: " << client->last_dm_status();
    }
  }
}

void UserPolicySigninServiceBase::Shutdown() {
  PrepareForUserCloudPolicyManagerShutdown();
}

void UserPolicySigninServiceBase::PrepareForUserCloudPolicyManagerShutdown() {
  registration_helper_.reset();
  registration_helper_for_temporary_client_.reset();
  // Don't run the callbacks to be consistent with
  // `CloudPolicyService::RefreshPolicy()` behavior during shutdown.
  policy_fetch_callbacks_.reset();
  UserCloudPolicyManager* manager = policy_manager();
  if (manager && manager->core()->client())
    manager->core()->client()->RemoveObserver(this);
  if (manager && manager->core()->service())
    manager->core()->service()->RemoveObserver(this);
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
    DVLOG(1) << "Signed in user is not in the allowlist";
    return nullptr;
  }

  // If the DeviceManagementService is not yet initialized, start it up now.
  device_management_service_->ScheduleInitialization(0);

  // Create a new CloudPolicyClient for fetching the DMToken.
  return UserCloudPolicyManager::CreateCloudPolicyClient(
      device_management_service_, system_url_loader_factory_);
}

bool UserPolicySigninServiceBase::ShouldLoadPolicyForUser(
    const std::string& username) {
  if (username.empty())
    return false;  // Not signed in.

  return !BrowserPolicyConnector::IsNonEnterpriseUser(username);
}

void UserPolicySigninServiceBase::InitializeForSignedInUser(
    const AccountId& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> profile_url_loader_factory) {
  DCHECK(account_id.is_valid());
  UserCloudPolicyManager* manager = policy_manager();
  bool should_load_policies =
      ShouldLoadPolicyForUser(account_id.GetUserEmail());
  manager->SetPoliciesRequired(should_load_policies);
  if (!should_load_policies) {
    DVLOG(1) << "Policy load not enabled for user: "
             << account_id.GetUserEmail();
    return;
  }

  // Initialize the UCPM if it is not already initialized.
  if (!manager->core()->service()) {
    // If there is no cached DMToken then we can detect this when the
    // OnCloudPolicyServiceInitializationCompleted() callback is invoked and
    // this will initiate a policy fetch.
    InitializeUserCloudPolicyManager(
        account_id,
        UserCloudPolicyManager::CreateCloudPolicyClient(
            device_management_service_, profile_url_loader_factory));
  } else {
    manager->SetSigninAccountId(account_id);
  }

  // If the CloudPolicyService is initialized, kick off registration.
  // Otherwise OnCloudPolicyServiceInitializationCompleted is invoked as soon as
  // the service finishes its initialization.
  if (manager->core()->service()->IsInitializationComplete())
    OnCloudPolicyServiceInitializationCompleted();
}

void UserPolicySigninServiceBase::InitializeUserCloudPolicyManager(
    const AccountId& account_id,
    std::unique_ptr<CloudPolicyClient> client) {
  DCHECK(client);
  UserCloudPolicyManager* manager = policy_manager();
  manager->SetSigninAccountId(account_id);
  DCHECK(!manager->core()->client());
  manager->Connect(local_state_, std::move(client));
  DCHECK(manager->core()->service());

  // Observe the client to detect errors fetching policy.
  manager->core()->client()->AddObserver(this);
  // Observe the service to determine when it's initialized.
  manager->core()->service()->AddObserver(this);
}

void UserPolicySigninServiceBase::ShutdownUserCloudPolicyManager() {
  PrepareForUserCloudPolicyManagerShutdown();
  UserCloudPolicyManager* manager = policy_manager();
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
  std::move(callback).Run(client->dm_token(), client->client_id());
}

void UserPolicySigninServiceBase::RegisterForPolicyWithAccountId(
    const std::string& username,
    const CoreAccountId& account_id,
    PolicyRegistrationCallback callback) {
  DCHECK(!account_id.empty());

  if (policy_manager() && policy_manager()->IsClientRegistered()) {
    // Reuse the already fetched DM token if the client of the manager is
    // already registered.
    std::move(callback).Run(policy_manager()->core()->client()->dm_token(),
                            policy_manager()->core()->client()->client_id());
    return;
  }

  // Create a new CloudPolicyClient for fetching the DMToken. This is a
  // different client from the one used by the manager.
  std::unique_ptr<CloudPolicyClient> policy_client =
      CreateClientForRegistrationOnly(username);
  if (!policy_client) {
    std::move(callback).Run(std::string(), std::string());
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

void UserPolicySigninServiceBase::RegisterCloudPolicyService() {
  DCHECK(
      identity_manager()->HasPrimaryAccount(GetConsentLevelForRegistration()));
  DCHECK(policy_manager()->core()->client());
  DCHECK(!policy_manager()->IsClientRegistered());

  DVLOG(1) << "Fetching new DM Token";

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

void UserPolicySigninServiceBase::OnRegistrationComplete() {
  ProhibitSignoutIfNeeded();
  registration_helper_.reset();
}

base::TimeDelta UserPolicySigninServiceBase::GetTryRegistrationDelay() {
  return base::TimeDelta();
}

void UserPolicySigninServiceBase::
    OnCloudPolicyServiceInitializationCompleted() {
  UserCloudPolicyManager* manager = policy_manager();
  DCHECK(manager->core()->service()->IsInitializationComplete());
  // The service is now initialized - if the client is not yet registered, then
  // it means that there is no cached policy and so we need to initiate a new
  // client registration.
  if (manager->IsClientRegistered()) {
    DVLOG(1) << "Client already registered - not fetching DMToken";
    ProhibitSignoutIfNeeded();
    return;
  }

  if (!CanApplyPolicies(/*check_for_refresh_token=*/true)) {
    // No token yet. This can only happen on Desktop platforms which should
    // listen to OnRefreshTokenUpdatedForAccount() and will re-attempt
    // registration once the token is available.
    DLOG(WARNING) << "No OAuth Refresh Token - delaying policy download";
    return;
  }

  base::TimeDelta try_registration_delay = GetTryRegistrationDelay();
  if (try_registration_delay.is_zero()) {
    // If the try registration delay is 0, register the cloud policy service
    // immediately without queueing a task. This is the case for Desktop.
    RegisterCloudPolicyService();
  } else {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&UserPolicySigninServiceBase::RegisterCloudPolicyService,
                       weak_factory_for_registration_.GetWeakPtr()),
        try_registration_delay);
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

}  // namespace policy
