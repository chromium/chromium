// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_FCM_INVALIDATION_SERVICE_H_
#define COMPONENTS_INVALIDATION_IMPL_FCM_INVALIDATION_SERVICE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/invalidation/impl/fcm_invalidation_listener.h"
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
        IdentityProvider* identity_provider,
        PrefService* pref_service,
        const std::string& project_id)>;

using FCMInvalidationListenerCallback =
    base::RepeatingCallback<std::unique_ptr<FCMInvalidationListener>(
        std::unique_ptr<FCMSyncNetworkChannel>)>;

// The InvalidationService wraps the C++ Invalidation Client (FCM) library.
class FCMInvalidationService : public InvalidationService,
                               public IdentityProvider::Observer,
                               public FCMInvalidationListener::Delegate {
 public:
  FCMInvalidationService(
      IdentityProvider* identity_provider,
      FCMNetworkHandlerCallback fcm_network_handler_callback,
      FCMInvalidationListenerCallback fcm_invalidation_listener_callback,
      PerUserTopicSubscriptionManagerCallback
          per_user_topic_subscription_manager_callback,
      instance_id::InstanceIDDriver* instance_id_driver,
      PrefService* pref_service,
      const std::string& sender_id);
  FCMInvalidationService(const FCMInvalidationService& other) = delete;
  FCMInvalidationService& operator=(const FCMInvalidationService& other) =
      delete;
  ~FCMInvalidationService() override;

  void Init();

  static void RegisterPrefs(PrefRegistrySimple* registry);
  static void ClearDeprecatedPrefs(PrefService* prefs);

  // InvalidationService implementation.
  // It is an error to have registered handlers when the service is destroyed.
  void AddObserver(InvalidationHandler* handler) override;
  bool HasObserver(const InvalidationHandler* handler) const override;
  bool UpdateInterestedTopics(InvalidationHandler* handler,
                              const TopicSet& topics) override;
  void RemoveObserver(const InvalidationHandler* handler) override;
  InvalidatorState GetInvalidatorState() const override;
  std::string GetInvalidatorClientId() const override;

  // IdentityProvider::Observer implementation.
  void OnActiveAccountRefreshTokenUpdated() override;
  void OnActiveAccountLogin() override;
  void OnActiveAccountLogout() override;

  // FCMInvalidationListener::Delegate implementation.
  std::optional<Invalidation> OnInvalidate(
      const Invalidation& invalidation) override;
  void OnInvalidatorStateChange(InvalidatorState state) override;
  void OnSuccessfullySubscribed(const Topic& topic) override;

 private:
  friend class FCMInvalidationServiceTest;

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

  bool IsReadyToStart();

  void PopulateClientID();
  void ResetClientID();
  void OnInstanceIDReceived(const std::string& instance_id);
  void OnDeleteInstanceIDCompleted(instance_id::InstanceID::Result);
  void DoUpdateSubscribedTopicsIfNeeded();
  const std::string GetApplicationName();

  const std::string sender_id_;
  InvalidatorRegistrarWithMemory invalidator_registrar_;

  FCMNetworkHandlerCallback fcm_network_handler_callback_;
  FCMInvalidationListenerCallback fcm_invalidation_listener_callback_;
  PerUserTopicSubscriptionManagerCallback
      per_user_topic_subscription_manager_callback_;

  const raw_ptr<instance_id::InstanceIDDriver> instance_id_driver_;
  // The invalidator client ID, aka instance ID.
  std::string client_id_;

  const raw_ptr<PrefService> pref_service_;

  bool update_was_requested_ = false;

  // The invalidation listener.
  std::unique_ptr<FCMInvalidationListener> invalidation_listener_;

  const raw_ptr<IdentityProvider> identity_provider_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_IMPL_FCM_INVALIDATION_SERVICE_H_
