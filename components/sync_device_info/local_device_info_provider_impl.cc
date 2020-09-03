// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/local_device_info_provider_impl.h"

#include "base/bind.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/base/sync_util.h"
#include "components/sync/invalidations/switches.h"
#include "components/sync/invalidations/sync_invalidations_service.h"
#include "components/sync_device_info/device_info_sync_client.h"
#include "components/sync_device_info/device_info_util.h"
#include "components/sync_device_info/local_device_info_util.h"

namespace syncer {

LocalDeviceInfoProviderImpl::LocalDeviceInfoProviderImpl(
    version_info::Channel channel,
    const std::string& version,
    const DeviceInfoSyncClient* sync_client,
    SyncInvalidationsService* sync_invalidations_service)
    : channel_(channel),
      version_(version),
      sync_client_(sync_client),
      sync_invalidations_service_(sync_invalidations_service) {
  DCHECK(sync_client);
  if (sync_invalidations_service_) {
    sync_invalidations_service_->AddTokenObserver(this);
    sync_invalidations_service_->AddSubscribedDataTypesObserver(this);
  }
}

LocalDeviceInfoProviderImpl::~LocalDeviceInfoProviderImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (sync_invalidations_service_) {
    sync_invalidations_service_->RemoveTokenObserver(this);
    sync_invalidations_service_->RemoveSubscribedDataTypesObserver(this);
  }
}

version_info::Channel LocalDeviceInfoProviderImpl::GetChannel() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return channel_;
}

const DeviceInfo* LocalDeviceInfoProviderImpl::GetLocalDeviceInfo() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!local_device_info_) {
    return nullptr;
  }

  local_device_info_->set_send_tab_to_self_receiving_enabled(
      sync_client_->GetSendTabToSelfReceivingEnabled());
  local_device_info_->set_sharing_info(sync_client_->GetLocalSharingInfo());
  return local_device_info_.get();
}

std::unique_ptr<LocalDeviceInfoProvider::Subscription>
LocalDeviceInfoProviderImpl::RegisterOnInitializedCallback(
    const base::RepeatingClosure& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!local_device_info_);
  return callback_list_.Add(callback);
}

void LocalDeviceInfoProviderImpl::OnFCMRegistrationTokenChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(
      base::FeatureList::IsEnabled(switches::kSubscribeForSyncInvalidations));
  DCHECK(sync_invalidations_service_);
  if (local_device_info_) {
    local_device_info_->set_fcm_registration_token(
        sync_invalidations_service_->GetFCMRegistrationToken());
  }
  // TODO(crbug.com/1102336): nudge device info update.
}

void LocalDeviceInfoProviderImpl::OnSubscribedDataTypesChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(
      base::FeatureList::IsEnabled(switches::kSubscribeForSyncInvalidations));
  DCHECK(sync_invalidations_service_);
  if (local_device_info_) {
    local_device_info_->set_interested_data_types(
        sync_invalidations_service_->GetSubscribedDataTypes());
  }
  // TODO(crbug.com/1102336): nudge device info update.
}

void LocalDeviceInfoProviderImpl::Initialize(
    const std::string& cache_guid,
    const std::string& client_name,
    const std::string& manufacturer_name,
    const std::string& model_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!cache_guid.empty());

  // The local device doesn't have a last updated timestamps. It will be set in
  // the specifics when it will be synced up.
  local_device_info_ = std::make_unique<DeviceInfo>(
      cache_guid, client_name, version_, MakeUserAgentForSync(channel_),
      GetLocalDeviceType(), sync_client_->GetSigninScopedDeviceId(),
      manufacturer_name, model_name,
      /*last_updated_timestamp=*/base::Time(),
      DeviceInfoUtil::GetPulseInterval(),
      sync_client_->GetSendTabToSelfReceivingEnabled(),
      sync_client_->GetLocalSharingInfo(), GetFCMRegistrationToken(),
      GetInterestedDataTypes());

  // Notify observers.
  callback_list_.Notify();
}

void LocalDeviceInfoProviderImpl::Clear() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  local_device_info_.reset();
}

void LocalDeviceInfoProviderImpl::UpdateClientName(
    const std::string& client_name) {
  DCHECK(local_device_info_);
  local_device_info_->set_client_name(client_name);
}

std::string LocalDeviceInfoProviderImpl::GetFCMRegistrationToken() const {
  if (sync_invalidations_service_) {
    return sync_invalidations_service_->GetFCMRegistrationToken();
  }
  return std::string();
}

ModelTypeSet LocalDeviceInfoProviderImpl::GetInterestedDataTypes() const {
  if (sync_invalidations_service_) {
    return sync_invalidations_service_->GetSubscribedDataTypes();
  }
  return ModelTypeSet();
}

}  // namespace syncer
