// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_REMOTE_COMMANDS_REMOTE_COMMANDS_INVALIDATOR_H_
#define COMPONENTS_POLICY_CORE_COMMON_REMOTE_COMMANDS_REMOTE_COMMANDS_INVALIDATOR_H_

#include <variant>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/threading/thread_checker.h"
#include "components/invalidation/invalidation_listener.h"
#include "components/invalidation/public/invalidation_handler.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/policy/core/common/cloud/policy_invalidation_scope.h"
#include "components/policy/policy_export.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace invalidation {
class Invalidation;
}  // namespace invalidation

namespace policy {

// This class provides basic intefaces for an invalidator for remote commands
// services. It's not interacting with CloudPolicyClient/CloudPolicyCore
// directly, instead, it handles the interacting with invalidation service
// only and leaves interfaces to integrate with subclasses.
class POLICY_EXPORT RemoteCommandsInvalidator
    : public invalidation::InvalidationHandler,
      public invalidation::InvalidationListener::Observer {
 public:
  RemoteCommandsInvalidator(std::string owner_name,
                            PolicyInvalidationScope scope);
  RemoteCommandsInvalidator(const RemoteCommandsInvalidator&) = delete;
  RemoteCommandsInvalidator& operator=(const RemoteCommandsInvalidator&) =
      delete;
  ~RemoteCommandsInvalidator() override;

  // Initialize this invalidator to pair with `InvalidationService` or
  // `InvalidationListener`. Must be called before Start().
  void Initialize(std::variant<invalidation::InvalidationService*,
                               invalidation::InvalidationListener*>
                      invalidation_service_or_listener);

  // Shutdown this invalidator. Will stop the invalidator first, and after
  // shutting down, the invalidator can't be started anymore unless it's
  // initialized again.
  void Shutdown();

  // Starts to process invalidations.
  void Start();

  // Stops to process invalidation. May only be called after Start() has been
  // called.
  void Stop();

  // Helpful accessors.
  invalidation::InvalidationService* invalidation_service();

  // Whether the invalidator currently has the ability to receive invalidations.
  bool invalidations_enabled() const {
    return IsRegistered() && AreInvalidationsEnabled();
  }

  // invalidation::InvalidationHandler:
  void OnInvalidatorStateChange(invalidation::InvalidatorState state) override;
  void OnIncomingInvalidation(
      const invalidation::Invalidation& invalidation) override;
  std::string GetOwnerName() const override;
  bool IsPublicTopic(const invalidation::Topic& topic) const override;
  void OnSuccessfullySubscribed(
      const invalidation::Topic& invalidation) override;

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
      const invalidation::Invalidation& invalidation) = 0;

  // This function will be called when invalidator successfully subscribed to
  // the topic to fetch initial remote commands that were triggered before the
  // device has subscribed.
  virtual void DoInitialRemoteCommandsFetch() = 0;

  // Subclasses must call this function to set the topic for remote command
  // invalidations.
  void ReloadPolicyData(const enterprise_management::PolicyData* policy);
  void ReloadPolicyDataWithInvalidationService(
      const enterprise_management::PolicyData* policy);

 private:
  // Returns true if `this` is observing `invalidation_service_or_listener_`.
  bool IsRegistered() const;

  // Returns true if `IsRegistered()` and `invalidation_service_or_listener_` is
  // enabled.
  bool AreInvalidationsEnabled() const;

  // Registers this handler with `InvalidationService` if needed and
  // subscribes to the given `topic` with the invalidation service.
  void RegisterWithInvalidationService(const invalidation::Topic& topic);

  // Unsubscribes from the current topics but keeps the registration as is.
  void UnsubscribeFromTopicsWithInvalidationService();

  // The state of the object.
  enum State {
    SHUT_DOWN,
    STOPPED,
    STARTED,
  };
  State state_ = SHUT_DOWN;

  // The unique name to be returned with by GetOwnerName().
  // TODO(b/343429042): Remove once does not implement
  // `invalidation::InvalidationHandler`.
  const std::string owner_name_;

  const PolicyInvalidationScope scope_;

  // The invalidation service or listener.
  std::variant<raw_ptr<invalidation::InvalidationService>,
               raw_ptr<invalidation::InvalidationListener>>
      invalidation_service_or_listener_ =
          static_cast<invalidation::InvalidationService*>(nullptr);

  // The current status of `InvalidationListener`.
  invalidation::InvalidationsExpected are_invalidations_expected_ =
      invalidation::InvalidationsExpected::kMaybe;

  base::ScopedObservation<invalidation::InvalidationListener,
                          RemoteCommandsInvalidator>
      invalidation_listener_observation_{this};
  base::ScopedObservation<invalidation::InvalidationService,
                          RemoteCommandsInvalidator>
      invalidation_service_observation_{this};

  // The Topic representing the remote commands in the invalidation service.
  invalidation::Topic topic_;

  // A thread checker to make sure that callbacks are invoked on the correct
  // thread.
  base::ThreadChecker thread_checker_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_REMOTE_COMMANDS_REMOTE_COMMANDS_INVALIDATOR_H_
