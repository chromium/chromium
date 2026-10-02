// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_CLOUD_USER_CLOUD_MANAGEMENT_STATUS_FETCHER_H_
#define COMPONENTS_POLICY_CORE_BROWSER_CLOUD_USER_CLOUD_MANAGEMENT_STATUS_FETCHER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
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

// Asynchronously fetches user cloud management status and profile separation
// policies from DMServer.
//
// The result is passed to a `FetchStatusAndPoliciesCallback`:
// - The `UserManagementStatus` is `std::nullopt` if the fetch failed (access
//   token error, DMServer error or timeout).
// - The `ProfileSeparationPolicies` are empty if the fetch failed, if policies
//   were not requested, or if the server did not return any valid policies.
//
// Each instance executes at most one fetch. The fetcher supports two ownership
// modes:
// - Caller-owned: the caller creates an instance, calls `Start()` and keeps
//   the instance alive until the callback runs. Destroying the instance
//   cancels the fetch and the callback never runs.
// - Self-managed: `FetchStatusAndPolicies()` creates an internal instance that
//   lives until the callback runs.
class POLICY_EXPORT UserCloudManagementStatusFetcher {
 public:
  using FetchStatusAndPoliciesCallback =
      base::OnceCallback<void(std::optional<UserManagementStatus>,
                              ProfileSeparationPolicies)>;

  // Constructs a fetcher instance. `service` and `url_loader_factory` must not
  // be null. `should_fetch_policies` specifies whether to request profile
  // separation policies in addition to the management status.
  UserCloudManagementStatusFetcher(
      DeviceManagementService* service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      bool should_fetch_policies);

  UserCloudManagementStatusFetcher(const UserCloudManagementStatusFetcher&) =
      delete;
  UserCloudManagementStatusFetcher& operator=(
      const UserCloudManagementStatusFetcher&) = delete;
  ~UserCloudManagementStatusFetcher();

  // Initiates the fetch workflow for `account_id`. Must be called at most once
  // per instance.
  //
  // RAII Cancellation: Destroying this fetcher instance immediately cancels
  // any in-flight OAuth token fetch, DMServer network request, and timeout
  // timer, and `callback` is never run.
  //
  // `callback` may destroy this fetcher instance: no member is accessed after
  // `callback` runs.
  void Start(signin::IdentityManager* identity_manager,
             const CoreAccountId& account_id,
             FetchStatusAndPoliciesCallback callback);

  // Asynchronously fetches user management status and optional profile
  // separation policies for `account_id`.
  //
  // Self-Managed Lifecycle: Creates a one-shot internal
  // `UserCloudManagementStatusFetcher` instance whose ownership (`unique_ptr`)
  // is transferred directly into the completion callback closure. This binds
  // the fetcher's lifetime to the completion of the asynchronous network and
  // authentication pipeline, destroying it automatically when the fetch
  // succeeds, encounters an error, or times out. Callers do not need to retain
  // an instance.
  static void FetchStatusAndPolicies(
      DeviceManagementService* service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager,
      const CoreAccountId& account_id,
      bool should_fetch_policies,
      FetchStatusAndPoliciesCallback callback);

 private:
  // Called when the OAuth access token request completes. If successful,
  // proceeds to send the DMServer request; otherwise calls Finish() with
  // null parameters.
  void OnAccessTokenFetchComplete(GoogleServiceAuthError error,
                                  signin::AccessTokenInfo access_token_info);

  // Constructs and dispatches the DMServer request for user management status
  // and policies using the provided `access_token`.
  void SendDeviceManagementRequest(std::string access_token);

  // Called when the DMServer job finishes. Parses the proto response into
  // `UserManagementStatus` and `ProfileSeparationPolicies` structs and invokes
  // Finish().
  void OnJobDone(DMServerJobResult result);

  // Called when the overall fetch operation times out before completion.
  void OnTimeout();

  // Cleans up internal state, stops timers, and passes the final results
  // to `callback_`.
  void Finish(std::optional<UserManagementStatus> status,
              ProfileSeparationPolicies profile_separation_policies);

  raw_ref<DeviceManagementService> service_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const bool should_fetch_policies_;
  FetchStatusAndPoliciesCallback callback_;

  std::unique_ptr<signin::AccessTokenFetcher> token_fetcher_;
  std::unique_ptr<DeviceManagementService::Job> fetch_job_;
  base::OneShotTimer timeout_timer_;

  base::WeakPtrFactory<UserCloudManagementStatusFetcher> weak_factory_{this};
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_CLOUD_USER_CLOUD_MANAGEMENT_STATUS_FETCHER_H_
