// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/invalidations/invalidation_service_impl.h"

#include <string>

#include "base/time/time.h"
#include "chromeos/ash/components/boca/invalidations/fcm_handler.h"
#include "chromeos/ash/components/boca/invalidations/invalidation_service_delegate.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"

namespace ash::boca {
namespace {

const net::BackoffEntry::Policy kBackoffPolicy = {
    0,  // Number of initial errors to ignore before applying
        // exponential back-off rules.
    base::Seconds(2).InMilliseconds(),  // Initial delay in ms.
    2,    // Factor by which the waiting time will be multiplied.
    0.2,  // Fuzzing percentage.
    base::Hours(1)
        .InMilliseconds(),  // Maximum amount of time to delay th request in ms.
    -1,                     // Never discard the entry.
    false                   // Do not always use initial delay.
};

}  // namespace

InvalidationServiceImpl::InvalidationServiceImpl(
    gcm::GCMDriver* gcm_driver,
    instance_id::InstanceIDDriver* instance_id_driver,
    InvalidationServiceDelegate* delegate)
    : upload_retry_backoff_{&kBackoffPolicy}, delegate_(delegate) {
  fcm_handler_ =
      std::make_unique<FCMHandlerImpl>(gcm_driver, instance_id_driver);
  // Add token refresh observer.
  fcm_handler_->AddTokenObserver(this);
  // Add invalidation message observer.
  fcm_handler_->AddListener(this);
  // Register app handler and start token fetch.
  fcm_handler_->StartListening();
}

InvalidationServiceImpl::~InvalidationServiceImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  fcm_handler_->RemoveTokenObserver(this);
  fcm_handler_->RemoveListener(this);
}

void InvalidationServiceImpl::OnInvalidationReceived(
    const std::string& payload) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->OnInvalidationReceived(payload);
}

void InvalidationServiceImpl::OnFCMRegistrationTokenChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!fcm_handler_->GetFCMRegistrationToken().has_value()) {
    return;
  } else {
    upload_retry_backoff_.Reset();
    UploadToken();
  }
}

void InvalidationServiceImpl::UploadToken() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->UploadToken(
      fcm_handler_->GetFCMRegistrationToken().value(),
      base::BindOnce(&InvalidationServiceImpl::OnTokenUploaded,
                     weak_factory_.GetWeakPtr()));
}

void InvalidationServiceImpl::OnTokenUploaded(bool success) {
  if (!success) {
    LOG(WARNING) << "[Boca]Failed to upload token, retrying";
    upload_retry_backoff_.InformOfRequest(/*succeeded=*/false);
    base::TimeDelta backoff_delay = upload_retry_backoff_.GetTimeUntilRelease();
    token_refresh_timer_.Start(
        FROM_HERE, backoff_delay,
        base::BindOnce(&InvalidationServiceImpl::UploadToken,
                       base::Unretained(this)));
    return;
  }
  LOG(WARNING) << "[Boca]Uploaded invalidation token.";
  upload_retry_backoff_.Reset();
}

void InvalidationServiceImpl::ShutDown() {
  if (fcm_handler_->IsListening()) {
    fcm_handler_->StopListeningPermanently();
  }
}
}  // namespace ash::boca
