// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/invalidations/sync_invalidations_service_impl.h"

#include "components/sync/invalidations/fcm_handler.h"

namespace syncer {

SyncInvalidationsServiceImpl::SyncInvalidationsServiceImpl(
    gcm::GCMDriver* gcm_driver,
    instance_id::InstanceIDDriver* instance_id_driver,
    const std::string& sender_id,
    const std::string& app_id) {
  fcm_handler_ = std::make_unique<FCMHandler>(gcm_driver, instance_id_driver,
                                              sender_id, app_id);
  fcm_handler_->StartListening();
}

SyncInvalidationsServiceImpl::~SyncInvalidationsServiceImpl() = default;

void SyncInvalidationsServiceImpl::AddListener(
    InvalidationsListener* listener) {
  fcm_handler_->AddListener(listener);
}

void SyncInvalidationsServiceImpl::RemoveListener(
    InvalidationsListener* listener) {
  fcm_handler_->RemoveListener(listener);
}

void SyncInvalidationsServiceImpl::AddTokenObserver(
    FCMRegistrationTokenObserver* observer) {
  fcm_handler_->AddTokenObserver(observer);
}

void SyncInvalidationsServiceImpl::RemoveTokenObserver(
    FCMRegistrationTokenObserver* observer) {
  fcm_handler_->RemoveTokenObserver(observer);
}

const std::string& SyncInvalidationsServiceImpl::GetFCMRegistrationToken()
    const {
  return fcm_handler_->GetFCMRegistrationToken();
}

void SyncInvalidationsServiceImpl::AddSubscribedDataTypesObserver(
    SubscribedDataTypesObserver* observer) {
  data_types_manager_.AddSubscribedDataTypesObserver(observer);
}

void SyncInvalidationsServiceImpl::RemoveSubscribedDataTypesObserver(
    SubscribedDataTypesObserver* observer) {
  data_types_manager_.RemoveSubscribedDataTypesObserver(observer);
}

const ModelTypeSet& SyncInvalidationsServiceImpl::GetSubscribedDataTypes()
    const {
  return data_types_manager_.GetSubscribedDataTypes();
}

void SyncInvalidationsServiceImpl::SetSubscribedDataTypes(
    const ModelTypeSet& data_types) {
  data_types_manager_.SetSubscribedDataTypes(data_types);
}

void SyncInvalidationsServiceImpl::Shutdown() {
  fcm_handler_.reset();
}

}  // namespace syncer
