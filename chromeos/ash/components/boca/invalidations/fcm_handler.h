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
}  // namespace gcm

namespace instance_id {
class InstanceIDDriver;
}  // namespace instance_id

namespace ash::boca {

// An interface to observe changes on FCM registration token.
class FCMRegistrationTokenObserver : public base::CheckedObserver {
 public:
  // Called on each change of FCM registration token.
  virtual void OnFCMRegistrationTokenChanged() = 0;

  // Called on token fetch failed if the fetch is not for validation. FCM token
  // may get fetched after this failure because of a retry and
  // `OnFCMRegistrationTokenChanged` will be called in this case.
  virtual void OnFCMTokenFetchFailed() {}
};

// This class provides an interface to handle received invalidations.
class InvalidationsListener : public base::CheckedObserver {
 public:
  // Called on each invalidation. |payload| is passed as is without any parsing.
  virtual void OnInvalidationReceived(const std::string& payload) = 0;
};

class FCMHandler {
 public:
  FCMHandler(const FCMHandler&) = delete;
  FCMHandler& operator=(const FCMHandler&) = delete;

  virtual ~FCMHandler() = default;

  // Used to start handling incoming invalidations from the server and to obtain
  // an FCM token. This method gets called after data types are configured.
  // Before StartListening() is called for the first time, the FCM registration
  // token will be null.
  virtual void StartListening() = 0;

  // Stop handling incoming invalidations. It doesn't cleanup the FCM
  // registration token and doesn't unsubscribe from FCM. All incoming
  // invalidations will be dropped. This method gets called during browser
  // shutdown.
  virtual void StopListening() = 0;

  // Stop handling incoming invalidations and delete Instance ID. It clears the
  // FCM registration token. This method gets called during sign-out.
  virtual void StopListeningPermanently() = 0;

  // Returns if the handler is listening for incoming invalidations.
  virtual bool IsListening() const = 0;

  // Add a new |listener| which will be notified on each new incoming
  // invalidation. |listener| must not be nullptr. Does nothing if the
  // |listener| has already been added before. When a new |listener| is added,
  // previously received messages will be immediately replayed.
  virtual void AddListener(InvalidationsListener* listener) = 0;

  // Returns whether `listener` was added before.
  virtual bool HasListener(InvalidationsListener* listener) = 0;

  // Removes |listener|, does nothing if it wasn't added before. |listener| must
  // not be nullptr.
  virtual void RemoveListener(InvalidationsListener* listener) = 0;

  // Add or remove an FCM token change observer. |observer| must not be nullptr.
  virtual void AddTokenObserver(FCMRegistrationTokenObserver* observer) = 0;
  virtual void RemoveTokenObserver(FCMRegistrationTokenObserver* observer) = 0;

  // Used to get an obtained FCM token. Returns null if it doesn't have a token.
  virtual const std::optional<std::string>& GetFCMRegistrationToken() const = 0;

 protected:
  FCMHandler() = default;
};

// This handler is used to register with FCM and to process incoming messages.
class FCMHandlerImpl : public FCMHandler, public gcm::GCMAppHandler {
 public:
  FCMHandlerImpl();
  FCMHandlerImpl(gcm::GCMDriver* gcm_driver,
                 instance_id::InstanceIDDriver* instance_id_driver);
  ~FCMHandlerImpl() override;
  FCMHandlerImpl(const FCMHandlerImpl&) = delete;
  FCMHandlerImpl& operator=(const FCMHandlerImpl&) = delete;

  void Init(gcm::GCMDriver* gcm_driver,
            instance_id::InstanceIDDriver* instance_id_driver);
  bool IsInitialized() const;

  // FCMHandler:
  void StartListening() override;
  void StopListening() override;
  void StopListeningPermanently() override;
  bool IsListening() const override;
  void AddListener(InvalidationsListener* listener) override;
  bool HasListener(InvalidationsListener* listener) override;
  void RemoveListener(InvalidationsListener* listener) override;
  void AddTokenObserver(FCMRegistrationTokenObserver* observer) override;
  void RemoveTokenObserver(FCMRegistrationTokenObserver* observer) override;
  const std::optional<std::string>& GetFCMRegistrationToken() const override;

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

  std::string GetAppIdForTesting();

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

  bool initialized_ = false;

  base::WeakPtrFactory<FCMHandlerImpl> weak_ptr_factory_{this};
};

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_INVALIDATIONS_FCM_HANDLER_H_
