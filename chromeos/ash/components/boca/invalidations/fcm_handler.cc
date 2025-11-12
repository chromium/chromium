// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/invalidations/fcm_handler.h"

#include <map>
#include <string_view>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/components/boca/boca_metrics_util.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"

namespace ash::boca {
namespace {

inline static constexpr std::string_view kSenderId = "947897361853";
inline static constexpr std::string_view kApplicationId =
    "com.google.chrome.boca.fcm.invalidations";

}  // namespace

// Lower bound time between two token validations when listening.
const int kTokenValidationPeriodMinutesDefault = 60 * 24;

// TODO(366316368): Revisit this TTL or should just remove it.
const int kInstanceIDTokenTTLSeconds = 28 * 24 * 60 * 60;  // 4 weeks.

FCMHandlerImpl::FCMHandlerImpl() = default;

FCMHandlerImpl::FCMHandlerImpl(
    gcm::GCMDriver* gcm_driver,
    instance_id::InstanceIDDriver* instance_id_driver) {
  Init(gcm_driver, instance_id_driver);
}

FCMHandlerImpl::~FCMHandlerImpl() {}

void FCMHandlerImpl::Init(gcm::GCMDriver* gcm_driver,
                          instance_id::InstanceIDDriver* instance_id_driver) {
  if (initialized_) {
    LOG(ERROR) << "[Boca] FCM handler is already initialized.";
    return;
  }
  gcm_driver_ = gcm_driver;
  instance_id_driver_ = instance_id_driver;
  initialized_ = true;
}

bool FCMHandlerImpl::IsInitialized() const {
  return initialized_;
}

void FCMHandlerImpl::StartListening() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!gcm_driver_) {
    return;
  }
  CHECK(!IsListening());
  CHECK(!fcm_registration_token_.has_value());
  gcm_driver_->AddAppHandler(std::string(kApplicationId), this);
  StartTokenFetch(/*is_validation=*/false);
}

void FCMHandlerImpl::StopListening() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // StopListening() may be called after StartListening() right away and
  // DidRetrieveToken() won't be called.
  if (gcm_driver_ && IsListening()) {
    gcm_driver_->RemoveAppHandler(std::string(kApplicationId));
    fcm_registration_token_ = std::nullopt;
    token_validation_timer_.Stop();
  }
}

void FCMHandlerImpl::StopListeningPermanently() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!instance_id_driver_) {
    return;
  }
  if (instance_id_driver_->ExistsInstanceID(std::string(kApplicationId))) {
    instance_id_driver_->GetInstanceID(std::string(kApplicationId))
        ->DeleteID(
            /*callback=*/base::DoNothing());
    fcm_registration_token_ = std::nullopt;
    for (FCMRegistrationTokenObserver& token_observer : token_observers_) {
      token_observer.OnFCMRegistrationTokenChanged();
    }
  }
  StopListening();
}

const std::optional<std::string>& FCMHandlerImpl::GetFCMRegistrationToken()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return fcm_registration_token_;
}

void FCMHandlerImpl::ShutdownHandler() {
  gcm_driver_ = nullptr;
  instance_id_driver_ = nullptr;
  fcm_registration_token_.reset();
}

void FCMHandlerImpl::AddListener(InvalidationsListener* listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (listeners_.HasObserver(listener)) {
    return;
  }
  listeners_.AddObserver(listener);
}

bool FCMHandlerImpl::HasListener(InvalidationsListener* listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return listeners_.HasObserver(listener);
}

void FCMHandlerImpl::RemoveListener(InvalidationsListener* listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  listeners_.RemoveObserver(listener);
}

void FCMHandlerImpl::OnStoreReset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The FCM registration token is not stored by FCMHandlerImpl.
}

void FCMHandlerImpl::AddTokenObserver(FCMRegistrationTokenObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  token_observers_.AddObserver(observer);
}

void FCMHandlerImpl::RemoveTokenObserver(
    FCMRegistrationTokenObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  token_observers_.RemoveObserver(observer);
}

void FCMHandlerImpl::OnMessage(const std::string& app_id,
                               const gcm::IncomingMessage& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(app_id, kApplicationId);
  const std::string kMethodKey = "method";
  const bool method_exists = base::Contains(message.data, kMethodKey);
  LOG_IF(ERROR, !method_exists)
      << "[Boca] Method does not exist in FCM message.";
  for (InvalidationsListener& listener : listeners_) {
    listener.OnInvalidationReceived(method_exists ? message.data.at(kMethodKey)
                                                  : std::string());
  }
}

void FCMHandlerImpl::OnMessagesDeleted(const std::string& app_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(app_id, kApplicationId);
}

void FCMHandlerImpl::OnSendError(
    const std::string& app_id,
    const gcm::GCMClient::SendErrorDetails& details) {
  // Should never be called because the invalidation service doesn't send GCM
  // messages to the server.
  NOTREACHED() << "FCMHandlerImpl doesn't send GCM messages.";
}

void FCMHandlerImpl::OnSendAcknowledged(const std::string& app_id,
                                        const std::string& message_id) {
  // Should never be called because the invalidation service doesn't send GCM
  // messages to the server.
  NOTREACHED() << "FCMHandlerImpl doesn't send GCM messages.";
}

std::string FCMHandlerImpl::GetAppIdForTesting() {
  return std::string(kApplicationId);
}

bool FCMHandlerImpl::IsListening() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return gcm_driver_ &&
         gcm_driver_->GetAppHandler(std::string(kApplicationId)) != nullptr;
}

void FCMHandlerImpl::DidRetrieveToken(base::TimeTicks fetch_time_for_metrics,
                                      bool is_validation,
                                      const std::string& subscription_token,
                                      instance_id::InstanceID::Result result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  boca::RecordTokenRetrievalIsValidation(is_validation);

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
    LOG(ERROR) << "[Boca] Messaging subscription failed: "
               << static_cast<int>(result);
    if (!is_validation) {
      token_observers_.Notify(
          &FCMRegistrationTokenObserver::OnFCMTokenFetchFailed);
    }
  }

  ScheduleNextTokenValidation();
}

void FCMHandlerImpl::ScheduleNextTokenValidation() {
  DCHECK(IsListening());

  token_validation_timer_.Start(
      FROM_HERE, base::Minutes(kTokenValidationPeriodMinutesDefault),
      base::BindOnce(&FCMHandlerImpl::StartTokenValidation,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FCMHandlerImpl::StartTokenValidation() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsListening());
  StartTokenFetch(/*is_validation=*/true);
}

void FCMHandlerImpl::StartTokenFetch(bool is_validation) {
  if (!instance_id_driver_) {
    return;
  }
  instance_id_driver_->GetInstanceID(std::string(kApplicationId))
      ->GetToken(
          std::string(kSenderId), instance_id::kGCMScope,
          /*time_to_live=*/base::Seconds(kInstanceIDTokenTTLSeconds),
          /*flags=*/{instance_id::InstanceID::Flags::kIsLazy},
          base::BindOnce(&FCMHandlerImpl::DidRetrieveToken,
                         weak_ptr_factory_.GetWeakPtr(),
                         /*fetch_time_for_metrics=*/base::TimeTicks::Now(),
                         is_validation));
}

}  // namespace ash::boca
