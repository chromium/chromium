// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/invalidations/sync_invalidations_service_impl.h"

#include <utility>

#include "components/sync/invalidations/fcm_handler.h"
#include "components/sync/invalidations/interested_data_types_handler.h"

namespace syncer {

namespace {

constexpr char kSenderId[] = "361488507004";
constexpr char kApplicationId[] = "com.google.chrome.sync.invalidations";

}  // namespace

SyncInvalidationsServiceImpl::SyncInvalidationsServiceImpl(
    gcm::GCMDriver* gcm_driver,
    instance_id::InstanceIDDriver* instance_id_driver) {
  fcm_handler_ = std::make_unique<FCMHandler>(gcm_driver, instance_id_driver,
                                              kSenderId, kApplicationId);
}

SyncInvalidationsServiceImpl::~SyncInvalidationsServiceImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void SyncInvalidationsServiceImpl::AddListener(
    InvalidationsListener* listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  fcm_handler_->AddListener(listener);
}

bool SyncInvalidationsServiceImpl::HasListener(
    InvalidationsListener* listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return fcm_handler_->HasListener(listener);
}

void SyncInvalidationsServiceImpl::RemoveListener(
    InvalidationsListener* listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  fcm_handler_->RemoveListener(listener);
}

void SyncInvalidationsServiceImpl::AddTokenObserver(
    FCMRegistrationTokenObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  fcm_handler_->AddTokenObserver(observer);
}

void SyncInvalidationsServiceImpl::RemoveTokenObserver(
    FCMRegistrationTokenObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  fcm_handler_->RemoveTokenObserver(observer);
}

void SyncInvalidationsServiceImpl::StartListening() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (fcm_handler_->IsListening()) {
    return;
  }
  fcm_handler_->StartListening();
}

void SyncInvalidationsServiceImpl::StopListening() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  fcm_handler_->StopListening();
}

void SyncInvalidationsServiceImpl::StopListeningPermanently() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!fcm_handler_->IsListening()) {
    return;
  }
  fcm_handler_->StopListeningPermanently();
}

std::optional<std::string>
SyncInvalidationsServiceImpl::GetFCMRegistrationToken() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return fcm_handler_->GetFCMRegistrationToken();
}

void SyncInvalidationsServiceImpl::SetInterestedDataTypesHandler(
    InterestedDataTypesHandler* handler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!interested_data_types_handler_ || !handler);
  interested_data_types_handler_ = handler;
}

std::optional<DataTypeSet>
SyncInvalidationsServiceImpl::GetInterestedDataTypes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return interested_data_types_;
}

void SyncInvalidationsServiceImpl::SetInterestedDataTypes(
    const DataTypeSet& data_types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(interested_data_types_handler_);

  interested_data_types_ = data_types;
  interested_data_types_handler_->OnInterestedDataTypesChanged();
}

void SyncInvalidationsServiceImpl::
    SetCommittedAdditionalInterestedDataTypesCallback(
        InterestedDataTypesAppliedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(interested_data_types_handler_);

  interested_data_types_handler_
      ->SetCommittedAdditionalInterestedDataTypesCallback(std::move(callback));
}

void SyncInvalidationsServiceImpl::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  fcm_handler_.reset();
}

FCMHandler* SyncInvalidationsServiceImpl::GetFCMHandlerForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return fcm_handler_.get();
}

}  // namespace syncer
