// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/local_device_info_provider_impl.h"

#include "base/bind.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/base/sync_util.h"
#include "components/sync_device_info/device_info_sync_client.h"
#include "components/sync_device_info/device_info_util.h"
#include "components/sync_device_info/local_device_info_util.h"

namespace syncer {

LocalDeviceInfoProviderImpl::LocalDeviceInfoProviderImpl(
    version_info::Channel channel,
    const std::string& version,
    const DeviceInfoSyncClient* sync_client)
    : channel_(channel), version_(version), sync_client_(sync_client) {
  DCHECK(sync_client);
}

LocalDeviceInfoProviderImpl::~LocalDeviceInfoProviderImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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

  // Pull new values for settings that aren't automatically updated.
  local_device_info_->set_send_tab_to_self_receiving_enabled(
      sync_client_->GetSendTabToSelfReceivingEnabled());
  local_device_info_->set_sharing_info(sync_client_->GetLocalSharingInfo());

  // Do not update previous values if the service is not fully initialized.
  // base::nullopt means that the value is unknown yet and the previous value
  // should be kept.
  const base::Optional<std::string> fcm_token =
      sync_client_->GetFCMRegistrationToken();
  if (fcm_token) {
    local_device_info_->set_fcm_registration_token(*fcm_token);
  }

  const base::Optional<ModelTypeSet> interested_data_types =
      sync_client_->GetInterestedDataTypes();
  if (interested_data_types) {
    local_device_info_->set_interested_data_types(*interested_data_types);
  }

  return local_device_info_.get();
}

base::CallbackListSubscription
LocalDeviceInfoProviderImpl::RegisterOnInitializedCallback(
    const base::RepeatingClosure& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!local_device_info_);
  return callback_list_.Add(callback);
}

void LocalDeviceInfoProviderImpl::Initialize(
    const std::string& cache_guid,
    const std::string& client_name,
    const std::string& manufacturer_name,
    const std::string& model_name,
    const std::string& last_fcm_registration_token,
    const ModelTypeSet& last_interested_data_types) {
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
      sync_client_->GetLocalSharingInfo(), last_fcm_registration_token,
      last_interested_data_types);

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

}  // namespace syncer
