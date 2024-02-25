// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_FCM_NETWORK_HANDLER_H_
#define COMPONENTS_INVALIDATION_IMPL_FCM_NETWORK_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "components/gcm_driver/gcm_app_handler.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/invalidation/impl/channels_states.h"
#include "components/invalidation/impl/fcm_sync_network_channel.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace gcm {
class GCMDriver;
}

namespace instance_id {
class InstanceIDDriver;
}

namespace invalidation {

/*
 * The class responsible for communication via GCM channel:
 *  - It retrieves the token required for the subscription
 *  and passes it by invoking token callback.
 *  - It receives the messages and passes them to the
 *  invalidation infrustructure, so they can be converted to the
 *  invalidations and consumed by listeners.
 */
class FCMNetworkHandler : public gcm::GCMAppHandler,
                          public FCMSyncNetworkChannel {
 public:
  FCMNetworkHandler(gcm::GCMDriver* gcm_driver,
                    instance_id::InstanceIDDriver* instance_id_driver,
                    const std::string& sender_id,
                    const std::string& app_id);
  FCMNetworkHandler(const FCMNetworkHandler& other) = delete;
  FCMNetworkHandler& operator=(const FCMNetworkHandler& other) = delete;
  ~FCMNetworkHandler() override;

  // Just calls std::make_unique. For ease of base::Bind'ing.
  static std::unique_ptr<FCMNetworkHandler> Create(
      gcm::GCMDriver* gcm_driver,
      instance_id::InstanceIDDriver* instance_id_driver,
      const std::string& sender_id,
      const std::string& app_id);

  bool IsListening() const;
  void UpdateChannelState(FcmChannelState state);

  // FCMSyncNetworkChannel overrides.
  void StartListening() override;
  void StopListening() override;

  // GCMAppHandler overrides.
  void ShutdownHandler() override;
  void OnStoreReset() override;
  void OnMessage(const std::string& app_id,
                 const gcm::IncomingMessage& message) override;
  void OnMessagesDeleted(const std::string& app_id) override;
  void OnSendError(const std::string& app_id,
                   const gcm::GCMClient::SendErrorDetails& details) override;
  void OnSendAcknowledged(const std::string& app_id,
                          const std::string& message_id) override;

  void SetTokenValidationTimerForTesting(
      std::unique_ptr<base::OneShotTimer> token_validation_timer);

 private:
  // Called when a subscription token is obtained from the GCM server.
  void DidRetrieveToken(const std::string& subscription_token,
                        instance_id::InstanceID::Result result);
  void ScheduleNextTokenValidation();
  void StartTokenValidation();
  void DidReceiveTokenForValidation(const std::string& new_token,
                                    instance_id::InstanceID::Result result);

  const raw_ptr<gcm::GCMDriver> gcm_driver_;
  const raw_ptr<instance_id::InstanceIDDriver> instance_id_driver_;

  FcmChannelState channel_state_ = FcmChannelState::NOT_STARTED;
  std::string token_;

  std::unique_ptr<base::OneShotTimer> token_validation_timer_;

  const std::string sender_id_;
  const std::string app_id_;

  base::WeakPtrFactory<FCMNetworkHandler> weak_ptr_factory_{this};
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_IMPL_FCM_NETWORK_HANDLER_H_
