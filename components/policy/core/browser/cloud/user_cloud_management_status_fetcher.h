// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_CLOUD_USER_CLOUD_MANAGEMENT_STATUS_FETCHER_H_
#define COMPONENTS_POLICY_CORE_BROWSER_CLOUD_USER_CLOUD_MANAGEMENT_STATUS_FETCHER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/policy/core/browser/signin/profile_separation_policies.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/policy_export.h"
#include "google_apis/gaia/google_service_auth_error.h"

struct CoreAccountId;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
struct AccessTokenInfo;
class AccessTokenFetcher;
class IdentityManager;
}  // namespace signin

namespace policy {

struct DMServerJobResult;

// Holds the account management state returned by Device Management Server.
struct POLICY_EXPORT UserManagementStatus {
  UserManagementStatus();
  ~UserManagementStatus();
  UserManagementStatus(const UserManagementStatus&);
  UserManagementStatus& operator=(const UserManagementStatus&);
  UserManagementStatus(UserManagementStatus&&);
  UserManagementStatus& operator=(UserManagementStatus&&);

  bool operator==(const UserManagementStatus& other) const = default;

  // Returns true if the account is managed and Chrome profile cloud management
  // is enabled for the user.
  bool CanBeSubjectedToEnterprisePolicies() const {
    return is_account_managed && is_chrome_profile_management_enabled;
  }

  // Whether the account is managed by an enterprise/organization.
  bool is_account_managed = false;

  // Whether Chrome profile cloud management is enabled for this user account.
  bool is_chrome_profile_management_enabled = false;
};

// Holds enterprise sign-in interception policies returned by Device Management
// Server.
struct POLICY_EXPORT UserInterceptionPolicies {
  UserInterceptionPolicies();
  ~UserInterceptionPolicies();
  UserInterceptionPolicies(const UserInterceptionPolicies&);
  UserInterceptionPolicies& operator=(const UserInterceptionPolicies&);
  UserInterceptionPolicies(UserInterceptionPolicies&&);
  UserInterceptionPolicies& operator=(UserInterceptionPolicies&&);

  bool operator==(const UserInterceptionPolicies& other) const = default;

  // Profile separation policies.
  ProfileSeparationPolicies profile_separation_policies;

  // Whether sync is disabled for this user by enterprise policy.
  std::optional<bool> sync_disabled;

  // Organization-managed browser theme color (e.g. "#rrggbb").
  std::optional<std::string> browser_theme_color;

  // URL of the organization logo image to display in sign-in dialogs.
  std::optional<std::string> enterprise_logo_url;

  // Custom organization name/label to display in sign-in dialogs.
  std::optional<std::string> enterprise_custom_label;
};

// Asynchronously fetches user cloud management status and optional sign-in
// interception policies from DMServer.
//
// This is a one-shot fetcher that manages its own lifecycle and executes at
// most one fetch per request. Callers are responsible for storing the returned
// results as needed.
class POLICY_EXPORT UserCloudManagementStatusFetcher {
 public:
  using FetchStatusAndPoliciesCallback =
      base::OnceCallback<void(std::optional<UserManagementStatus>,
                              std::optional<UserInterceptionPolicies>)>;

  // Constructs a fetcher instance. `service` and `url_loader_factory` must not be
  // null. `should_fetch_policies` specifies whether to request sign-in
  // experience policies in addition to the management status.
  UserCloudManagementStatusFetcher(
      DeviceManagementService* service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      bool should_fetch_policies);

  UserCloudManagementStatusFetcher(const UserCloudManagementStatusFetcher&) =
      delete;
  UserCloudManagementStatusFetcher& operator=(
      const UserCloudManagementStatusFetcher&) = delete;
  ~UserCloudManagementStatusFetcher();

  // Asynchronously fetches user management status and optional interception
  // policies for `account_id`.
  //
  // Lifecycle: Creates an internal `UserCloudManagementStatusFetcher` instance
  // that manages its own lifecycle. Ownership of the fetcher is bound into the
  // completion callback closure, keeping it alive until the request succeeds,
  // encounters an error, or times out, after which it is automatically
  // destroyed. The caller does not own the fetcher and is responsible for
  // storing the result provided to `callback`.
  static void FetchStatusAndPolicies(
      DeviceManagementService* service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager,
      const CoreAccountId& account_id,
      bool should_fetch_policies,
      FetchStatusAndPoliciesCallback callback);

 private:
  using InternalFetchCallback =
      base::OnceCallback<void(std::optional<UserManagementStatus>,
                              std::optional<UserInterceptionPolicies>)>;

  // Starts the fetch workflow by setting up the timeout timer and initiating
  // the OAuth access token fetch for `account_id`.
  void Start(signin::IdentityManager* identity_manager,
             const CoreAccountId& account_id,
             InternalFetchCallback callback);

  // Called when the OAuth access token request completes. If successful,
  // proceeds to send the DMServer request; otherwise calls Finish() with
  // null parameters.
  void OnAccessTokenFetchComplete(GoogleServiceAuthError error,
                                  signin::AccessTokenInfo access_token_info);

  // Constructs and dispatches the DMServer request for user management status
  // and sign-in experience policies using the provided `access_token`.
  void SendDeviceManagementRequest(const std::string& access_token);

  // Called when the DMServer job finishes. Parses the proto response into
  // `UserManagementStatus` and `UserInterceptionPolicies` structs and invokes
  // Finish().
  void OnJobDone(DMServerJobResult result);

  // Called when the overall fetch operation times out before completion.
  void OnTimeout();

  // Cleans up internal state, stops timers, and passes the final results
  // to `callback_`.
  void Finish(std::optional<UserManagementStatus> status,
              std::optional<UserInterceptionPolicies> interception_policies);

  raw_ref<DeviceManagementService> service_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const bool should_fetch_policies_;
  InternalFetchCallback callback_;

  std::unique_ptr<signin::AccessTokenFetcher> token_fetcher_;
  std::unique_ptr<DeviceManagementService::Job> fetch_job_;
  base::OneShotTimer timeout_timer_;

  base::WeakPtrFactory<UserCloudManagementStatusFetcher> weak_factory_{this};
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_CLOUD_USER_CLOUD_MANAGEMENT_STATUS_FETCHER_H_
