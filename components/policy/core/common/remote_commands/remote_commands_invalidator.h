// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_REMOTE_COMMANDS_REMOTE_COMMANDS_INVALIDATOR_H_
#define COMPONENTS_POLICY_CORE_COMMON_REMOTE_COMMANDS_REMOTE_COMMANDS_INVALIDATOR_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/threading/thread_checker.h"
#include "components/invalidation/invalidation_listener.h"
#include "components/policy/core/common/cloud/policy_invalidation_scope.h"
#include "components/policy/policy_export.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace policy {

// This class provides basic intefaces for an invalidator for remote commands
// services. It's not interacting with CloudPolicyClient/CloudPolicyCore
// directly, instead, it handles the interacting with invalidation service
// only and leaves interfaces to integrate with subclasses.
class POLICY_EXPORT RemoteCommandsInvalidator
    : public invalidation::InvalidationListener::Observer {
 public:
  explicit RemoteCommandsInvalidator(PolicyInvalidationScope scope);
  RemoteCommandsInvalidator(const RemoteCommandsInvalidator&) = delete;
  RemoteCommandsInvalidator& operator=(const RemoteCommandsInvalidator&) =
      delete;
  ~RemoteCommandsInvalidator() override;

  // Initialize this invalidator to pair with `invalidation_listener`. Must be
  // called before Start().
  void Initialize(invalidation::InvalidationListener* invalidation_listener);

  // Shutdown this invalidator. Will stop the invalidator first, and after
  // shutting down, the invalidator can't be started anymore unless it's
  // initialized again.
  void Shutdown();

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

 protected:
  virtual void OnInitialize() = 0;
  virtual void OnShutdown() = 0;
  virtual void OnStart() = 0;
  virtual void OnStop() = 0;

  // Subclasses must override this method to implement the actual remote
  // commands fetch.
  virtual void DoRemoteCommandsFetch(
      const invalidation::DirectInvalidation& invalidation) = 0;

  // This function will be called when invalidator successfully subscribed to
  // invalidations to fetch initial remote commands that were triggered before
  // the invalidator has subscribed.
  virtual void DoInitialRemoteCommandsFetch() = 0;

 private:
  // Returns true if ready to receive invalidations.
  bool IsRegistered() const;

  // Returns true if ready to receive invalidations and invalidations are
  // enabled.
  bool AreInvalidationsEnabled() const;

  // The state of the object.
  enum State {
    SHUT_DOWN,
    STOPPED,
    STARTED,
  };
  State state_ = SHUT_DOWN;

  const PolicyInvalidationScope scope_;

  // The invalidation service or listener.
  raw_ptr<invalidation::InvalidationListener> invalidation_listener_;

  // The current status of `InvalidationListener`.
  invalidation::InvalidationsExpected are_invalidations_expected_ =
      invalidation::InvalidationsExpected::kMaybe;

  base::ScopedObservation<invalidation::InvalidationListener,
                          RemoteCommandsInvalidator>
      invalidation_listener_observation_{this};

  // A thread checker to make sure that callbacks are invoked on the correct
  // thread.
  base::ThreadChecker thread_checker_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_REMOTE_COMMANDS_REMOTE_COMMANDS_INVALIDATOR_H_
