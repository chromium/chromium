// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/invalidations/fcm_handler.h"

#include <map>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"

namespace ash::boca {

// Lower bound time between two token validations when listening.
const int kTokenValidationPeriodMinutesDefault = 60 * 24;

// TODO(366316368): Revisit this TTL or should just remove it.
const int kInstanceIDTokenTTLSeconds = 28 * 24 * 60 * 60;  // 4 weeks.

FCMHandler::FCMHandler(gcm::GCMDriver* gcm_driver,
                       instance_id::InstanceIDDriver* instance_id_driver,
                       const std::string& sender_id,
                       const std::string& app_id)
    : gcm_driver_(gcm_driver),
      instance_id_driver_(instance_id_driver),
      sender_id_(sender_id),
      app_id_(app_id) {}

FCMHandler::~FCMHandler() {}

void FCMHandler::StartListening() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!IsListening());
  CHECK(!fcm_registration_token_.has_value());
  gcm_driver_->AddAppHandler(app_id_, this);
  StartTokenFetch(/*is_validation=*/false);
}

void FCMHandler::StopListening() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // StopListening() may be called after StartListening() right away and
  // DidRetrieveToken() won't be called.
  if (IsListening()) {
    gcm_driver_->RemoveAppHandler(app_id_);
    fcm_registration_token_ = std::nullopt;
    token_validation_timer_.AbandonAndStop();
  }
}

void FCMHandler::StopListeningPermanently() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (instance_id_driver_->ExistsInstanceID(app_id_)) {
    instance_id_driver_->GetInstanceID(app_id_)->DeleteID(
        /*callback=*/base::DoNothing());
    for (FCMRegistrationTokenObserver& token_observer : token_observers_) {
      token_observer.OnFCMRegistrationTokenChanged();
    }
  }
  StopListening();
}

const std::optional<std::string>& FCMHandler::GetFCMRegistrationToken() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return fcm_registration_token_;
}

void FCMHandler::ShutdownHandler() {
  // In profile service two-phase shutdown, FCM shutdown should have been called
  // before Dtor, which would remove us from listener list. This will never be
  // reached.
  NOTREACHED_IN_MIGRATION();
}

void FCMHandler::AddListener(InvalidationsListener* listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (listeners_.HasObserver(listener)) {
    return;
  }
  listeners_.AddObserver(listener);
}

bool FCMHandler::HasListener(InvalidationsListener* listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return listeners_.HasObserver(listener);
}

void FCMHandler::RemoveListener(InvalidationsListener* listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  listeners_.RemoveObserver(listener);
}

void FCMHandler::OnStoreReset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The FCM registration token is not stored by FCMHandler.
}

void FCMHandler::AddTokenObserver(FCMRegistrationTokenObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  token_observers_.AddObserver(observer);
}

void FCMHandler::RemoveTokenObserver(FCMRegistrationTokenObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  token_observers_.RemoveObserver(observer);
}

void FCMHandler::OnMessage(const std::string& app_id,
                           const gcm::IncomingMessage& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(app_id, app_id_);

  for (InvalidationsListener& listener : listeners_) {
    listener.OnInvalidationReceived(message.raw_data);
  }
}

void FCMHandler::OnMessagesDeleted(const std::string& app_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(app_id, app_id_);
}

void FCMHandler::OnSendError(const std::string& app_id,
                             const gcm::GCMClient::SendErrorDetails& details) {
  // Should never be called because the invalidation service doesn't send GCM
  // messages to the server.
  NOTREACHED_IN_MIGRATION() << "FCMHandler doesn't send GCM messages.";
}

void FCMHandler::OnSendAcknowledged(const std::string& app_id,
                                    const std::string& message_id) {
  // Should never be called because the invalidation service doesn't send GCM
  // messages to the server.
  NOTREACHED_IN_MIGRATION() << "FCMHandler doesn't send GCM messages.";
}

bool FCMHandler::IsListening() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return gcm_driver_->GetAppHandler(app_id_) != nullptr;
}

void FCMHandler::DidRetrieveToken(base::TimeTicks fetch_time_for_metrics,
                                  bool is_validation,
                                  const std::string& subscription_token,
                                  instance_id::InstanceID::Result result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!is_validation) {
    // TODO(b/366316261):Add metrics for token retrival status.
  }

  if (!IsListening()) {
    // After we requested the token, |StopListening| has been called. Thus,
    // ignore the token.
    return;
  }

  // Notify observers only if the token has changed.
  if (result == instance_id::InstanceID::SUCCESS &&
      (fcm_registration_token_ != subscription_token)) {
    fcm_registration_token_ = subscription_token;
    for (FCMRegistrationTokenObserver& token_observer : token_observers_) {
      token_observer.OnFCMRegistrationTokenChanged();
    }
  } else if (result != instance_id::InstanceID::SUCCESS) {
    DLOG(WARNING) << "Messaging subscription failed: " << result;
  }

  ScheduleNextTokenValidation();
}

void FCMHandler::ScheduleNextTokenValidation() {
  DCHECK(IsListening());

  token_validation_timer_.Start(
      FROM_HERE, base::Minutes(kTokenValidationPeriodMinutesDefault),
      base::BindOnce(&FCMHandler::StartTokenValidation,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FCMHandler::StartTokenValidation() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsListening());
  StartTokenFetch(/*is_validation=*/true);
}

void FCMHandler::StartTokenFetch(bool is_validation) {
  instance_id_driver_->GetInstanceID(app_id_)->GetToken(
      sender_id_, instance_id::kGCMScope,
      /*time_to_live=*/base::Seconds(kInstanceIDTokenTTLSeconds),
      /*flags=*/{instance_id::InstanceID::Flags::kIsLazy},
      base::BindOnce(
          &FCMHandler::DidRetrieveToken, weak_ptr_factory_.GetWeakPtr(),
          /*fetch_time_for_metrics=*/base::TimeTicks::Now(), is_validation));
}

}  // namespace ash::boca
