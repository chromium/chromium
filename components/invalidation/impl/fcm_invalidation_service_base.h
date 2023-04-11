// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_FCM_INVALIDATION_SERVICE_BASE_H_
#define COMPONENTS_INVALIDATION_IMPL_FCM_INVALIDATION_SERVICE_BASE_H_

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/invalidation/impl/fcm_invalidation_listener.h"
#include "components/invalidation/impl/invalidation_logger.h"
#include "components/invalidation/impl/invalidator_registrar_with_memory.h"
#include "components/invalidation/public/invalidation_handler.h"
#include "components/invalidation/public/invalidation_service.h"

class PrefService;
class PrefRegistrySimple;

namespace instance_id {
class InstanceIDDriver;
}

namespace invalidation {

class FCMNetworkHandler;
class PerUserTopicSubscriptionManager;

using FCMNetworkHandlerCallback =
    base::RepeatingCallback<std::unique_ptr<FCMNetworkHandler>(
        const std::string& sender_id,
        const std::string& app_id)>;

using PerUserTopicSubscriptionManagerCallback =
    base::RepeatingCallback<std::unique_ptr<PerUserTopicSubscriptionManager>(
        const std::string& project_id)>;

// This InvalidationService wraps the C++ Invalidation Client (FCM) library.
// It provides invalidations for desktop platforms (Win, Mac, Linux).
// Subclasses should implement Init to set up their initial state and call
// StartInvalidator/StopInvalidator when they want to start/stop receiving
// invalidations.
class FCMInvalidationServiceBase : public InvalidationService,
                                   public FCMInvalidationListener::Delegate {
 public:
  FCMInvalidationServiceBase(
      FCMNetworkHandlerCallback fcm_network_handler_callback,
      PerUserTopicSubscriptionManagerCallback
          per_user_topic_subscription_manager_callback,
      instance_id::InstanceIDDriver* instance_id_driver,
      PrefService* pref_service,
      const std::string& sender_id);
  FCMInvalidationServiceBase(const FCMInvalidationServiceBase& other) = delete;
  FCMInvalidationServiceBase& operator=(
      const FCMInvalidationServiceBase& other) = delete;
  ~FCMInvalidationServiceBase() override;

  virtual void Init() = 0;

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // InvalidationService implementation.
  // It is an error to have registered handlers when the service is destroyed.
  void RegisterInvalidationHandler(InvalidationHandler* handler) override;
  bool UpdateInterestedTopics(InvalidationHandler* handler,
                              const TopicSet& topics) override;
  void UnsubscribeFromUnregisteredTopics(InvalidationHandler* handler) override;
  void UnregisterInvalidationHandler(InvalidationHandler* handler) override;
  InvalidatorState GetInvalidatorState() const override;
  std::string GetInvalidatorClientId() const override;
  InvalidationLogger* GetInvalidationLogger() override;
  void RequestDetailedStatus(
      base::RepeatingCallback<void(base::Value::Dict)> caller) const override;

  // FCMInvalidationListener::Delegate implementation.
  void OnInvalidate(const TopicInvalidationMap& invalidation_map) override;
  void OnInvalidatorStateChange(InvalidatorState state) override;

 protected:
  // Initializes with an injected listener.
  void InitForTest(
      std::unique_ptr<FCMInvalidationListener> invalidation_listener);

  virtual base::Value::Dict CollectDebugData() const;

  // Returns true if the service is currently started and able to receive
  // invalidations.
  bool IsStarted() const;
  // Subclasses should be calling StartInvalidator and StopInvalidator when it
  // is appropriate for their use case. This class will call StopInvalidator
  // when it's destroyed if it's still started at that point.
  // Start the invalidation service to start receiving invalidations.
  void StartInvalidator();
  // Stop the invalidation service to stop receiving invalidations. It's
  // appropriate to call this when the service is expected to be started again
  // with the same client ID (such as during shutdown).
  void StopInvalidator();
  // Stops the invalidation service to stop receiving invalidations. This also
  // resets the Instance ID driver's client ID and clears the client ID cache.
  void StopInvalidatorPermanently();

 private:
  struct Diagnostics {
    base::Time instance_id_requested;
    base::Time instance_id_received;
    base::Time instance_id_cleared;
    base::Time service_was_stopped;
    base::Time service_was_started;
  };

  void PopulateClientID();
  void ResetClientID();
  void OnInstanceIDReceived(const std::string& instance_id);
  void OnDeleteInstanceIDCompleted(instance_id::InstanceID::Result);
  void DoUpdateSubscribedTopicsIfNeeded();
  const std::string GetApplicationName();

  const std::string sender_id_;
  InvalidatorRegistrarWithMemory invalidator_registrar_;

  // The invalidation logger object we use to record state changes
  // and invalidations.
  InvalidationLogger logger_;

  FCMNetworkHandlerCallback fcm_network_handler_callback_;
  PerUserTopicSubscriptionManagerCallback
      per_user_topic_subscription_manager_callback_;

  const raw_ptr<instance_id::InstanceIDDriver> instance_id_driver_;
  // The invalidator client ID, aka instance ID.
  std::string client_id_;

  const raw_ptr<PrefService> pref_service_;

  bool update_was_requested_ = false;
  Diagnostics diagnostic_info_;

  // The invalidation listener.
  std::unique_ptr<FCMInvalidationListener> invalidation_listener_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_IMPL_FCM_INVALIDATION_SERVICE_BASE_H_
