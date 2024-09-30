// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_INVALIDATION_LISTENER_IMPL_H_
#define COMPONENTS_INVALIDATION_INVALIDATION_LISTENER_IMPL_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "components/gcm_driver/gcm_app_handler.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/invalidation/invalidation_listener.h"
#include "net/base/backoff_entry.h"

namespace gcm {
class GCMDriver;
}

namespace instance_id {
class InstanceIDDriver;
}

namespace invalidation {

// This is the actual implementation of the `InvalidationListener`.
//
// You should not need to use this directly:
// - production code should call `InvalidationListener::Create`.
// - test code should use a mocked or fake InvalidationListener.
class InvalidationListenerImpl : public InvalidationListener,
                                 public gcm::GCMAppHandler {
 public:
  static constexpr base::TimeDelta kRegistrationTokenTimeToLive =
      base::Days(14);
  static constexpr base::TimeDelta kRegistrationTokenValidationPeriod =
      base::Days(1);

  InvalidationListenerImpl(gcm::GCMDriver* gcm_driver,
                           instance_id::InstanceIDDriver* instance_id_driver,
                           std::string project_number,
                           std::string log_prefix);
  ~InvalidationListenerImpl() override;

  // `InvalidationListener`:
  void AddObserver(Observer* handler) override;
  bool HasObserver(const Observer* handler) const override;
  void RemoveObserver(const Observer* handler) override;

  void Start(RegistrationTokenHandler* handler) override;
  void Shutdown() override;
  void SetRegistrationUploadStatus(
      RegistrationTokenUploadStatus status) override;

  // `GCMAppHandler`:
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
  // Requests the current or new registration token.
  // This is used both when `StartListening` is called and once per
  // `kRegistrationTokenValidationPeriod`.
  void FetchRegistrationToken();

  // If `result` indicates success and the `new_registration_token` is different
  // from `registration_token_`, update `registration_token_` and inform the
  // `handler_`.
  // If `result` does not indicate success, assume that we are not currently
  // registered, update the `registration_token_` and inform the `handler_`.
  void OnRegistrationTokenReceived(const std::string& new_registration_token,
                                   instance_id::InstanceID::Result result);

  // Returns the status of the invalidations expectation.
  // Invalidations are not expected if a registration token is not received
  // yet, or if a registration upload status  is not `kSucceeded`.
  // Expected otherwise.
  InvalidationsExpected AreInvalidationsExpected() const;
  // Notifies observers about current invalidations expectation.
  void UpdateObserversExpectations();

  // Registration data.
  raw_ptr<gcm::GCMDriver> gcm_driver_;
  raw_ptr<instance_id::InstanceIDDriver> instance_id_driver_;
  const std::string project_number_;
  const std::string log_prefix_;

  std::optional<std::string> registration_token_;

  raw_ptr<RegistrationTokenHandler> registration_token_handler_ = nullptr;
  RegistrationTokenUploadStatus registration_upload_status_ =
      RegistrationTokenUploadStatus::kFailed;

  // Each observer is mapped to exactly one type.
  base::ObserverList<Observer, true> observers_;
  std::map<std::string, raw_ptr<Observer, CtnExperimental>> type_to_handler_;
  std::map<std::string, DirectInvalidation> type_to_invalidation_cache_;

  // Calculates timeout until next registration attempt on failure.
  net::BackoffEntry registration_retry_backoff_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<InvalidationListenerImpl> weak_ptr_factory_{this};
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_INVALIDATION_LISTENER_IMPL_H_
