// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_INVALIDATIONS_FCM_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_INVALIDATIONS_FCM_HANDLER_H_

#include <optional>
#include <string>

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/gcm_driver/gcm_app_handler.h"
#include "components/gcm_driver/instance_id/instance_id.h"

namespace gcm {
class GCMDriver;
}

namespace instance_id {
class InstanceIDDriver;
}

namespace ash::boca {

class InvalidationsListener;

// An interface to observe changes on FCM registration token.
class FCMRegistrationTokenObserver : public base::CheckedObserver {
 public:
  // Called on each change of FCM registration token.
  virtual void OnFCMRegistrationTokenChanged() = 0;
};

// This class provides an interface to handle received invalidations.
class InvalidationsListener : public base::CheckedObserver {
 public:
  // Called on each invalidation. |payload| is passed as is without any parsing.
  virtual void OnInvalidationReceived(const std::string& payload) = 0;
};

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
  // an FCM token. This method gets called after data types are configured.
  // Before StartListening() is called for the first time, the FCM registration
  // token will be null.
  void StartListening();

  // Stop handling incoming invalidations. It doesn't cleanup the FCM
  // registration token and doesn't unsubscribe from FCM. All incoming
  // invalidations will be dropped. This method gets called during browser
  // shutdown.
  void StopListening();

  // Stop handling incoming invalidations and delete Instance ID. It clears the
  // FCM registration token. This method gets called during sign-out.
  void StopListeningPermanently();

  // Returns if the handler is listening for incoming invalidations.
  bool IsListening() const;

  // Add a new |listener| which will be notified on each new incoming
  // invalidation. |listener| must not be nullptr. Does nothing if the
  // |listener| has already been added before. When a new |listener| is added,
  // previously received messages will be immediately replayed.
  void AddListener(InvalidationsListener* listener);

  // Returns whether `listener` was added before.
  bool HasListener(InvalidationsListener* listener);

  // Removes |listener|, does nothing if it wasn't added before. |listener| must
  // not be nullptr.
  void RemoveListener(InvalidationsListener* listener);

  // Add or remove an FCM token change observer. |observer| must not be nullptr.
  void AddTokenObserver(FCMRegistrationTokenObserver* observer);
  void RemoveTokenObserver(FCMRegistrationTokenObserver* observer);

  // Used to get an obtained FCM token. Returns null if it doesn't have a token.
  const std::optional<std::string>& GetFCMRegistrationToken() const;

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
  void DidRetrieveToken(base::TimeTicks fetch_time_for_metrics,
                        bool is_validation,
                        const std::string& subscription_token,
                        instance_id::InstanceID::Result result);
  void ScheduleNextTokenValidation();
  void StartTokenValidation();

  void StartTokenFetch(bool is_validation);

  SEQUENCE_CHECKER(sequence_checker_);

  raw_ptr<gcm::GCMDriver> gcm_driver_ = nullptr;
  raw_ptr<instance_id::InstanceIDDriver> instance_id_driver_ = nullptr;
  const std::string sender_id_;
  const std::string app_id_;

  // Contains an FCM registration token. Token is null if the experiment is off
  // or we don't have a valid token yet and contains valid token otherwise.
  std::optional<std::string> fcm_registration_token_;

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

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_INVALIDATIONS_FCM_HANDLER_H_
