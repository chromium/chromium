// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_SERVICE_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_SERVICE_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/compiler_specific.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/policy_export.h"

namespace policy {

// Coordinates cloud policy handling, moving downloaded policy from the client
// to the store, and setting up client registrations from cached data in the
// store. Also coordinates actions on policy refresh triggers.
class POLICY_EXPORT CloudPolicyService : public CloudPolicyClient::Observer,
                                         public CloudPolicyStore::Observer {
 public:
  // Callback invoked once the policy refresh attempt has completed. Passed
  // bool parameter is true if the refresh was successful (no error).
  using RefreshPolicyCallback = base::OnceCallback<void(bool)>;

  class POLICY_EXPORT Observer {
   public:
    // Invoked when CloudPolicyService has finished initializing (any initial
    // policy load activity has completed and the CloudPolicyClient has
    // been registered, if possible).
    virtual void OnCloudPolicyServiceInitializationCompleted() = 0;

    // Called when policy refresh finshed. |success| indicates whether refresh
    // was successful.
    virtual void OnPolicyRefreshed(bool success) {}

    // Name of the observer for logging purposes.
    // TODO(b/40915114): Remove once solved.
    virtual std::string_view name() const = 0;

    virtual ~Observer() = default;
  };

  // |client| and |store| must remain valid for the object life time.
  CloudPolicyService(const std::string& policy_type,
                     const std::string& settings_entity_id,
                     CloudPolicyClient* client,
                     CloudPolicyStore* store);
  CloudPolicyService(const CloudPolicyService&) = delete;
  CloudPolicyService& operator=(const CloudPolicyService&) = delete;
  ~CloudPolicyService() override;

  // Refreshes policy. |callback| will be invoked after the operation completes
  // or aborts because of errors.
  //
  // The |reason| parameter will be used to tag the request to DMServer. This
  // will allow for more targeted monitoring and alerting.
  virtual void RefreshPolicy(RefreshPolicyCallback callback,
                             PolicyFetchReason reason);

  // Adds/Removes an Observer for this object.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // CloudPolicyClient::Observer:
  void OnPolicyFetched(CloudPolicyClient* client) override;
  void OnRegistrationStateChanged(CloudPolicyClient* client) override;
  void OnClientError(CloudPolicyClient* client) override;

  // CloudPolicyStore::Observer:
  void OnStoreLoaded(CloudPolicyStore* store) override;
  void OnStoreError(CloudPolicyStore* store) override;

  void ReportValidationResult(CloudPolicyStore* store, ValidationAction action);

  bool IsInitializationComplete() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return initialization_complete_;
  }

  // If initial policy refresh was completed returns its result.
  // This allows ChildPolicyObserver to know whether policy was fetched before
  // profile creation.
  std::optional<bool> initial_policy_refresh_result() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return initial_policy_refresh_result_;
  }

 private:
  // Helper function that is called when initialization may be complete, and
  // which is responsible for notifying observers.
  void CheckInitializationCompleted();

  // Invokes the refresh callbacks and clears refresh state. The |success| flag
  // is passed through to the refresh callbacks.
  void RefreshCompleted(bool success);

  // Assert non-concurrent usage in debug builds.
  SEQUENCE_CHECKER(sequence_checker_);

  // The policy type that will be fetched by the |client_|, with the optional
  // |settings_entity_id_|.
  std::string policy_type_;
  std::string settings_entity_id_;

  // The client used to talk to the cloud.
  raw_ptr<CloudPolicyClient> client_;

  // Takes care of persisting and decoding cloud policy.
  raw_ptr<CloudPolicyStore> store_;

  // Tracks the state of a pending refresh operation, if any.
  enum {
    // No refresh pending.
    REFRESH_NONE,
    // Policy fetch is pending.
    REFRESH_POLICY_FETCH,
    // Policy store is pending.
    REFRESH_POLICY_STORE,
  } refresh_state_;

  // Callbacks to invoke upon policy refresh.
  std::vector<RefreshPolicyCallback> refresh_callbacks_;

  // Set to true once the service is initialized (initial policy load/refresh
  // is complete).
  bool initialization_complete_;

  // Set to true if initial policy refresh was successful. Set to false
  // otherwise.
  std::optional<bool> initial_policy_refresh_result_;

  // Observers who will receive notifications when the service has finished
  // initializing.
  base::ObserverList<Observer, true>::Unchecked observers_;

  // Identifier from the stored policy. Policy validations results are only
  // reported once if the validated policy's data signature matches with this
  // one. Will be cleared once we send the validation report.
  std::string policy_pending_validation_signature_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_SERVICE_H_
