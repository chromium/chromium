// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_REMOTE_COMMANDS_REMOTE_COMMANDS_INVALIDATOR_H_
#define COMPONENTS_POLICY_CORE_COMMON_REMOTE_COMMANDS_REMOTE_COMMANDS_INVALIDATOR_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/threading/thread_checker.h"
#include "components/invalidation/invalidation_listener.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/policy_invalidation_scope.h"
#include "components/policy/policy_export.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace base {
class Clock;
}

namespace policy {

// This class provides basic intefaces for an invalidator for remote commands
// services. It's not interacting with CloudPolicyClient/CloudPolicyCore
// directly, instead, it handles the interacting with invalidation service
// only and leaves interfaces to integrate with subclasses.
class POLICY_EXPORT RemoteCommandsInvalidator
    : public invalidation::InvalidationListener::Observer,
      public CloudPolicyCore::Observer,
      public CloudPolicyStore::Observer {
 public:
  RemoteCommandsInvalidator(
      invalidation::InvalidationListener* invalidation_listener,
      CloudPolicyCore* core,
      const base::Clock* clock,
      PolicyInvalidationScope scope);
  RemoteCommandsInvalidator(const RemoteCommandsInvalidator&) = delete;
  RemoteCommandsInvalidator& operator=(const RemoteCommandsInvalidator&) =
      delete;
  ~RemoteCommandsInvalidator() override;

  // Starts to process invalidations.
  void Start();

  // Stops to process invalidation. May only be called after Start() has been
  // called.
  void Stop();

  // Whether the invalidator currently has the ability to receive invalidations.
  bool invalidations_enabled() const {
    return IsRegistered() && AreInvalidationsEnabled();
  }

  // invalidation::InvalidationListener::Observer:
  void OnExpectationChanged(
      invalidation::InvalidationsExpected expected) override;
  void OnInvalidationReceived(
      const invalidation::DirectInvalidation& invalidation) override;
  std::string GetType() const override;

  // CloudPolicyCore::Observer:
  void OnCoreConnected(CloudPolicyCore* core) override;
  void OnRefreshSchedulerStarted(CloudPolicyCore* core) override;
  void OnCoreDisconnecting(CloudPolicyCore* core) override;
  void OnRemoteCommandsServiceStarted(CloudPolicyCore* core) override;

  // CloudPolicyStore::Observer:
  void OnStoreLoaded(CloudPolicyStore* store) override;
  void OnStoreError(CloudPolicyStore* store) override;

 private:
  void Initialize();
  void Shutdown();

  // Subclasses must override this method to implement the actual remote
  // commands fetch.
  void DoRemoteCommandsFetch(
      const invalidation::DirectInvalidation& invalidation);

  // This function will be called when invalidator successfully subscribed to
  // invalidations to fetch initial remote commands that were triggered before
  // the invalidator has subscribed.
  void DoInitialRemoteCommandsFetch();

  // Returns true if ready to receive invalidations.
  bool IsRegistered() const;

  // Returns true if ready to receive invalidations and invalidations are
  // enabled.
  bool AreInvalidationsEnabled() const;

  void RecordInvalidationMetric(
      const invalidation::DirectInvalidation& invalidation) const;

  // The state of the object.
  enum State {
    SHUT_DOWN,
    STOPPED,
    STARTED,
  };
  State state_ = SHUT_DOWN;

  const PolicyInvalidationScope scope_;

  // The invalidation service or listener.
  const raw_ptr<invalidation::InvalidationListener> invalidation_listener_;
  const raw_ptr<CloudPolicyCore> core_;
  const raw_ptr<const base::Clock> clock_;

  // The current status of `InvalidationListener`.
  invalidation::InvalidationsExpected are_invalidations_expected_ =
      invalidation::InvalidationsExpected::kMaybe;

  base::ScopedObservation<invalidation::InvalidationListener,
                          RemoteCommandsInvalidator>
      invalidation_listener_observation_{this};
  base::ScopedObservation<CloudPolicyCore, RemoteCommandsInvalidator>
      core_observation_{this};
  base::ScopedObservation<CloudPolicyStore, RemoteCommandsInvalidator>
      store_observation_{this};

  // A thread checker to make sure that callbacks are invoked on the correct
  // thread.
  base::ThreadChecker thread_checker_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_REMOTE_COMMANDS_REMOTE_COMMANDS_INVALIDATOR_H_
