// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_INVALIDATIONS_FCM_HANDLER_H_
#define COMPONENTS_SYNC_INVALIDATIONS_FCM_HANDLER_H_

#include <string>
#include <vector>

#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "components/gcm_driver/gcm_app_handler.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/keyed_service/core/keyed_service.h"

namespace gcm {
class GCMDriver;
}

namespace instance_id {
class InstanceIDDriver;
}

namespace syncer {

class FCMRegistrationTokenObserver;
class InvalidationsListener;

// This handler is used to register with FCM and to process incoming messages.
class FCMHandler : public gcm::GCMAppHandler {
 public:
  FCMHandler(gcm::GCMDriver* gcm_driver,
             instance_id::InstanceIDDriver* instance_id_driver,
             const std::string& sender_id,
             const std::string& app_id);
  ~FCMHandler() override;
  FCMHandler(const FCMHandler&) = delete;
  FCMHandler& operator=(const FCMHandler&) = delete;

  // Used to start handling incoming invalidations from the server and to obtain
  // an FCM token. This method gets called after sign-in, or during browser
  // startup if the user is already signed in. Before StartListening() is called
  // for the first time, the FCM registration token will be empty.
  void StartListening();

  // Stop handling incoming invalidations. It doesn't cleanup the FCM
  // registration token and doesn't unsubscribe from FCM. All incoming
  // invalidations will be dropped. This method gets called during browser
  // shutdown.
  void StopListening();

  // Stop handling incoming invalidations and delete Instance ID. This method
  // gets called during sign-out.
  void StopListeningPermanently();

  // Returns if the handler is listening for incoming invalidations.
  bool IsListening() const;

  // Add or remove a new listener which will be notified on each new incoming
  // invalidation. |listener| must not be nullptr.
  void AddListener(InvalidationsListener* listener);
  void RemoveListener(InvalidationsListener* listener);

  // Add or remove an FCM token change observer. |observer| must not be nullptr.
  void AddTokenObserver(FCMRegistrationTokenObserver* observer);
  void RemoveTokenObserver(FCMRegistrationTokenObserver* observer);

  // Used to get an obtained FCM token. Returns empty string if it hasn't
  // received yet.
  const std::string& GetFCMRegistrationToken() const;

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

 private:
  // Called when a subscription token is obtained from the GCM server.
  void DidRetrieveToken(const std::string& subscription_token,
                        instance_id::InstanceID::Result result);
  void ScheduleNextTokenValidation();
  void StartTokenValidation();
  void DidReceiveTokenForValidation(const std::string& new_token,
                                    instance_id::InstanceID::Result result);

  void StartTokenFetch(instance_id::InstanceID::GetTokenCallback callback);

  SEQUENCE_CHECKER(sequence_checker_);

  gcm::GCMDriver* gcm_driver_ = nullptr;
  instance_id::InstanceIDDriver* instance_id_driver_ = nullptr;
  const std::string sender_id_;
  const std::string app_id_;

  // Contains an FCM registration token if not empty.
  std::string fcm_registration_token_;

  base::OneShotTimer token_validation_timer_;

  // Contains all listeners to notify about each incoming message in OnMessage
  // method.
  base::ObserverList<InvalidationsListener,
                     /*check_empty=*/true,
                     /*allow_reentrancy=*/false>
      listeners_;

  // Contains all FCM token observers to notify about each token change.
  base::ObserverList<FCMRegistrationTokenObserver,
                     /*check_empty=*/true,
                     /*allow_reentrancy=*/false>
      token_observers_;

  base::WeakPtrFactory<FCMHandler> weak_ptr_factory_{this};
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_INVALIDATIONS_FCM_HANDLER_H_
