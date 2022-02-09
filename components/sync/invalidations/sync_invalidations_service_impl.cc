// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/invalidations/sync_invalidations_service_impl.h"

#include <utility>

#include "components/sync/base/features.h"
#include "components/sync/invalidations/fcm_handler.h"

namespace syncer {

namespace {

// TODO(crbug.com/1082115): change to real sync sender id: 8181035976.
constexpr char kSenderId[] = "361488507004";
constexpr char kApplicationId[] = "com.google.chrome.sync.invalidations";

}  // namespace

SyncInvalidationsServiceImpl::SyncInvalidationsServiceImpl(
    gcm::GCMDriver* gcm_driver,
    instance_id::InstanceIDDriver* instance_id_driver) {
  fcm_handler_ = std::make_unique<FCMHandler>(gcm_driver, instance_id_driver,
                                              kSenderId, kApplicationId);
}

SyncInvalidationsServiceImpl::~SyncInvalidationsServiceImpl() = default;

void SyncInvalidationsServiceImpl::SetActive(bool active) {
  if (!base::FeatureList::IsEnabled(kUseSyncInvalidations) ||
      fcm_handler_->IsListening() == active) {
    return;
  }

  if (active) {
    fcm_handler_->StartListening();
  } else {
    fcm_handler_->StopListeningPermanently();
  }
}

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

absl::optional<std::string>
SyncInvalidationsServiceImpl::GetFCMRegistrationToken() const {
  if (fcm_handler_->IsWaitingForToken()) {
    return absl::nullopt;
  }
  return fcm_handler_->GetFCMRegistrationToken();
}

void SyncInvalidationsServiceImpl::SetInterestedDataTypesHandler(
    InterestedDataTypesHandler* handler) {
  data_types_manager_.SetInterestedDataTypesHandler(handler);
}

absl::optional<ModelTypeSet>
SyncInvalidationsServiceImpl::GetInterestedDataTypes() const {
  return data_types_manager_.GetInterestedDataTypes();
}

void SyncInvalidationsServiceImpl::SetInterestedDataTypes(
    const ModelTypeSet& data_types) {
  data_types_manager_.SetInterestedDataTypes(data_types);
}

void SyncInvalidationsServiceImpl::
    SetCommittedAdditionalInterestedDataTypesCallback(
        InterestedDataTypesAppliedCallback callback) {
  data_types_manager_.SetCommittedAdditionalInterestedDataTypesCallback(
      std::move(callback));
}

void SyncInvalidationsServiceImpl::Shutdown() {
  fcm_handler_.reset();
}

FCMHandler* SyncInvalidationsServiceImpl::GetFCMHandlerForTesting() {
  return fcm_handler_.get();
}

}  // namespace syncer
