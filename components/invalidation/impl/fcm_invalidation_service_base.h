// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_FCM_INVALIDATION_SERVICE_BASE_H_
#define COMPONENTS_INVALIDATION_IMPL_FCM_INVALIDATION_SERVICE_BASE_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/invalidation/impl/fcm_invalidation_listener.h"
#include "components/invalidation/impl/invalidation_listener.h"
#include "components/invalidation/impl/invalidation_logger.h"
#include "components/invalidation/impl/invalidator_registrar_with_memory.h"
#include "components/invalidation/public/invalidation_handler.h"
#include "components/invalidation/public/invalidation_service.h"

class PrefService;
class PrefRegistrySimple;

namespace instance_id {
class InstanceIDDriver;
}

namespace syncer {
class FCMNetworkHandler;
class PerUserTopicRegistrationManager;
}  // namespace syncer

namespace invalidation {

using FCMNetworkHandlerCallback =
    base::RepeatingCallback<std::unique_ptr<syncer::FCMNetworkHandler>(
        const std::string& sender_id,
        const std::string& app_id)>;

using PerUserTopicRegistrationManagerCallback =
    base::RepeatingCallback<std::unique_ptr<
        syncer::PerUserTopicRegistrationManager>(const std::string& project_id,
                                                 bool migrate_prefs)>;

// This InvalidationService wraps the C++ Invalidation Client (FCM) library.
// It provides invalidations for desktop platforms (Win, Mac, Linux).
// Subclasses should implement Init to set up their initial state and call
// StartInvalidator/StopInvalidator when they want to start/stop receiving
// invalidations.
class FCMInvalidationServiceBase
    : public InvalidationService,
      public syncer::FCMInvalidationListener::Delegate {
 public:
  FCMInvalidationServiceBase(
      FCMNetworkHandlerCallback fcm_network_handler_callback,
      PerUserTopicRegistrationManagerCallback
          per_user_topic_registration_manager_callback,
      instance_id::InstanceIDDriver* client_id_driver,
      PrefService* pref_service,
      const std::string& sender_id = {});

  ~FCMInvalidationServiceBase() override;

  virtual void Init() = 0;

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // InvalidationService implementation.
  // It is an error to have registered handlers when the service is destroyed.
  void RegisterInvalidationHandler(
      syncer::InvalidationHandler* handler) override;
  bool UpdateRegisteredInvalidationIds(syncer::InvalidationHandler* handler,
                                       const syncer::ObjectIdSet& ids) override;
  void UnregisterInvalidationHandler(
      syncer::InvalidationHandler* handler) override;
  syncer::InvalidatorState GetInvalidatorState() const override;
  std::string GetInvalidatorClientId() const override;
  InvalidationLogger* GetInvalidationLogger() override;
  void RequestDetailedStatus(
      base::RepeatingCallback<void(const base::DictionaryValue&)> caller)
      const override;

  // syncer::FCMInvalidationListener::Delegate implementation.
  void OnInvalidate(
      const syncer::TopicInvalidationMap& invalidation_map) override;
  void OnInvalidatorStateChange(syncer::InvalidatorState state) override;

 protected:
  // Initializes with an injected invalidator.
  void InitForTest(
      std::unique_ptr<syncer::FCMInvalidationListener> invalidation_listener);

  virtual base::DictionaryValue CollectDebugData() const;
  void ReportInvalidatorState(syncer::InvalidatorState state);

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
    base::Time service_was_stopped;
    base::Time service_was_started;
  };

  void PopulateClientID();
  void ResetClientID();
  void OnInstanceIdReceived(const std::string& id);
  void OnDeleteIDCompleted(instance_id::InstanceID::Result);
  void DoUpdateRegisteredIdsIfNeeded();
  const std::string GetApplicationName();

  const std::string sender_id_;
  syncer::InvalidatorRegistrarWithMemory invalidator_registrar_;

  // The invalidation logger object we use to record state changes
  // and invalidations.
  InvalidationLogger logger_;

  FCMNetworkHandlerCallback fcm_network_handler_callback_;
  PerUserTopicRegistrationManagerCallback
      per_user_topic_registration_manager_callback_;

  instance_id::InstanceIDDriver* const instance_id_driver_;
  std::string client_id_;

  PrefService* const pref_service_;

  bool update_was_requested_ = false;
  Diagnostics diagnostic_info_;

  // The invalidation listener.
  std::unique_ptr<syncer::FCMInvalidationListener> invalidation_listener_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(FCMInvalidationServiceBase);
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_IMPL_FCM_INVALIDATION_SERVICE_BASE_H_
