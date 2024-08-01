// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_CLOUD_USER_POLICY_SIGNIN_SERVICE_BASE_H_
#define COMPONENTS_POLICY_CORE_BROWSER_CLOUD_USER_POLICY_SIGNIN_SERVICE_BASE_H_

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/cancelable_callback.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/core_account_id.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

class AccountId;
class PrefService;

namespace policy {

class DeviceManagementService;
class UserCloudPolicyManager;
class CloudPolicyManager;
class CloudPolicyClientRegistrationHelper;
class CloudPolicyClient;
class ProfileCloudPolicyManager;

// The UserPolicySigninService is responsible for interacting with the policy
// infrastructure (mainly CloudPolicyManager) to load policy for the signed
// in user. This is the base class that contains shared behavior.
//
// At signin time, this class initializes the CloudPolicyManager and loads
// policy before any other signed in services are initialized. After each
// restart, this class ensures that the CloudPolicyClient is registered (in case
// the policy server was offline during the initial policy fetch) and if not it
// initiates a fresh registration process.
//
// Finally, if the user signs out, this class is responsible for shutting down
// the policy infrastructure to ensure that any cached policy is cleared.
class POLICY_EXPORT UserPolicySigninServiceBase
    : public KeyedService,
      public CloudPolicyClient::Observer,
      public CloudPolicyService::Observer {
 public:
  // The callback invoked once policy registration is complete. Passed
  // |dm_token| and |client_id| parameters are empty if policy registration
  // failed.
  typedef base::OnceCallback<void(
      const std::string& dm_token,
      const std::string& client_id,
      const std::vector<std::string>& user_affiliation_ids)>
      PolicyRegistrationCallback;

  // The callback invoked once policy fetch is complete. Passed boolean
  // parameter is set to true if the policy fetch succeeded.
  typedef base::OnceCallback<void(bool)> PolicyFetchCallback;

  // Creates a UserPolicySigninServiceBase associated with the passed |profile|.
  UserPolicySigninServiceBase(
      PrefService* local_state,
      DeviceManagementService* device_management_service,
      absl::variant<UserCloudPolicyManager*, ProfileCloudPolicyManager*>
          policy_manager,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory);
  UserPolicySigninServiceBase(const UserPolicySigninServiceBase&) = delete;
  UserPolicySigninServiceBase& operator=(const UserPolicySigninServiceBase&) =
      delete;
  ~UserPolicySigninServiceBase() override;

  // Initiates a policy fetch as part of user signin, using a |dm_token| and
  // |client_id| fetched via RegisterForPolicyXXX(). |callback| is invoked
  // once the policy fetch is complete, passing true if the policy fetch
  // succeeded.
  // Virtual for testing.
  virtual void FetchPolicyForSignedInUser(
      const AccountId& account_id,
      const std::string& dm_token,
      const std::string& client_id,
      const std::vector<std::string>& user_affiliation_ids,
      scoped_refptr<network::SharedURLLoaderFactory> profile_url_loader_factory,
      PolicyFetchCallback callback);

  // CloudPolicyService::Observer implementation:
  void OnCloudPolicyServiceInitializationCompleted() override;
  void OnPolicyRefreshed(bool success) override;
  std::string_view name() const override;

  // CloudPolicyClient::Observer implementation:
  void OnPolicyFetched(CloudPolicyClient* client) override;
  void OnRegistrationStateChanged(CloudPolicyClient* client) override;
  void OnClientError(CloudPolicyClient* client) override;

  // KeyedService implementation:
  void Shutdown() override;

  // Registers to DM Server to get a DM token to fetch user policies for that
  // account.
  //
  // Registration goes through the following steps:
  //   1) Request an OAuth2 access token to let the account access the service.
  //   2) Fetch the user info tied to the access token.
  //   3) Register the account to DMServer using the access token and user info
  //      to get a DM token that allows fetching user policies for the
  //      registered account.
  //   4) Invoke |callback| when the DM token is available. Will pass an empty
  //      token when the account isn't managed OR there is an error during the
  //      registration.
  virtual void RegisterForPolicyWithAccountId(
      const std::string& username,
      const CoreAccountId& account_id,
      PolicyRegistrationCallback callback);

  // Set CloudPolicyClient::DeviceDMTokenCallback for policy fetch request.
  // Function is for testing purpose only to avoid setup affiliated id and
  // dm token for both user and device.
  void SetDeviceDMTokenCallbackForTesting(
      CloudPolicyClient::DeviceDMTokenCallback callback);

 protected:
  // Invoked to initialize the cloud policy service for |account_id|, which is
  // the account associated with the Profile that owns this service. This is
  // invoked from InitializeOnProfileReady() if the Profile already has a
  // signed-in account at startup, or (on the desktop platforms) as soon as the
  // user signs-in and an OAuth2 login refresh token becomes available.
  void InitializeForSignedInUser(const AccountId& account_id,
                                 scoped_refptr<network::SharedURLLoaderFactory>
                                     profile_url_loader_factory);

  // Initializes the cloud policy manager with the passed |client|. This is
  // called from InitializeForSignedInUser() when the Profile already has a
  // signed in account at startup, and from FetchPolicyForSignedInUser() during
  // the initial policy fetch after signing in.
  virtual void InitializeCloudPolicyManager(
      const AccountId& account_id,
      std::unique_ptr<CloudPolicyClient> client);

  // Prepares for the CloudPolicyManager to be shutdown due to
  // user signout or profile destruction.
  virtual void PrepareForCloudPolicyManagerShutdown();

  // Shuts down the CloudPolicyManager (for example, after the user signs
  // out) and deletes any cached policy.
  virtual void ShutdownCloudPolicyManager();

  // Updates the timestamp of the last policy check. Implemented on mobile
  // platforms for network efficiency.
  virtual void UpdateLastPolicyCheckTime();

  // Gets the sign-in consent level required to perform registration.
  virtual signin::ConsentLevel GetConsentLevelForRegistration();

  // Gets the delay before the next registration.
  virtual base::TimeDelta GetTryRegistrationDelay();

  // Prohibits signout if needed when the account is registered for cloud policy
  // . Might be no-op for some platforms (eg., iOS and Android).
  virtual void ProhibitSignoutIfNeeded();

  // Returns true when policies can be applied for the profile. The profile has
  // to be at least tied to an account.
  virtual bool CanApplyPolicies(bool check_for_refresh_token);

  // Cancels the pending task that does registration. This invalidates the
  // |weak_factory_for_registration_| weak pointers used for registration.
  void CancelPendingRegistration();

  // Fetches an OAuth token to allow the cloud policy service to register with
  // the cloud policy server. |oauth_login_token| should contain an OAuth login
  // refresh token that can be downscoped to get an access token for the
  // device_management service.
  virtual void RegisterCloudPolicyService();

  // Returns a callback that can be used to retrieve device dm token when user
  // is affiliated.
  virtual CloudPolicyClient::DeviceDMTokenCallback
  GetDeviceDMTokenIfAffiliatedCallback();

  virtual std::string GetProfileId() = 0;

  // Convenience helpers to get the associated CloudPolicyManager and
  // IdentityManager.
  CloudPolicyManager* policy_manager() { return policy_manager_; }
  signin::IdentityManager* identity_manager() { return identity_manager_; }
  PrefService* local_state() { return local_state_; }
  DeviceManagementService* device_management_service() {
    return device_management_service_;
  }

  signin::ConsentLevel consent_level() const { return consent_level_; }

  scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory() {
    return system_url_loader_factory_;
  }

  CloudPolicyClient::DeviceDMTokenCallback
      device_dm_token_callback_for_testing_;

 private:
  // A getter for `policy_fetch_callbacks_` that constructs a new instance if
  // it's null.
  base::OnceCallbackList<void(bool)>& policy_fetch_callbacks();

  // Returns a CloudPolicyClient to perform a registration with the DM server,
  // or NULL if |username| shouldn't register for policy management.
  std::unique_ptr<CloudPolicyClient> CreateClientForRegistrationOnly(
      const std::string& username);

  // Returns a CloudPolicyClient for policy fetch, reporting and many other
  // purposes. It attaches a callback to the client to retrieve device DM token
  // which is uploaded for policy fetch request.
  std::unique_ptr<CloudPolicyClient> CreateClientForNonRegistration(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  // Returns false if cloud policy is disabled or if the passed |email_address|
  // is definitely not from a hosted domain (according to the list in
  // signin::AccountManagedStatusFinder::IsEnterpriseUserBasedOnEmail()).
  bool ShouldLoadPolicyForUser(const std::string& email_address);

  // Handler to call the policy registration callback that provides the DM
  // token.
  void CallPolicyRegistrationCallbackForTemporaryClient(
      std::unique_ptr<CloudPolicyClient> client,
      PolicyRegistrationCallback callback);

  // Callback invoked when policy registration has finished.
  void OnRegistrationComplete();

  // Weak pointer to the CloudPolicyManager and IdentityManager this service
  // is associated with.
  raw_ptr<UserCloudPolicyManager> user_policy_manager_;
  raw_ptr<CloudPolicyManager> policy_manager_;
  raw_ptr<signin::IdentityManager> identity_manager_;

  raw_ptr<PrefService> local_state_;
  raw_ptr<DeviceManagementService> device_management_service_;
  scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory_;

  signin::ConsentLevel consent_level_ = signin::ConsentLevel::kSignin;

  // Callbacks to invoke upon policy fetch.
  std::unique_ptr<base::OnceCallbackList<void(bool)>> policy_fetch_callbacks_;

  // Helper for registering the client to DMServer to get a DM token using a
  // cloud policy client. When there is an instance of |registration_helper_|,
  // it means that registration is ongoing. There is no registration when null.
  std::unique_ptr<CloudPolicyClientRegistrationHelper> registration_helper_;
  // A separate helper instance for a registration only client created via
  // `RegisterForPolicyWithAccountId()`.
  std::unique_ptr<CloudPolicyClientRegistrationHelper>
      registration_helper_for_temporary_client_;

  // Callback to start the delayed registration. Cancelled when the service is
  // shut down.
  base::CancelableOnceCallback<void()> registration_callback_;

  base::WeakPtrFactory<UserPolicySigninServiceBase> weak_factory_{this};

  // Weak pointer factory used for registration.
  base::WeakPtrFactory<UserPolicySigninServiceBase>
      weak_factory_for_registration_{this};
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_CLOUD_USER_POLICY_SIGNIN_SERVICE_BASE_H_
