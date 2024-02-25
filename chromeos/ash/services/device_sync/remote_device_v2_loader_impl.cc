// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/remote_device_v2_loader_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "chromeos/ash/components/multidevice/beacon_seed.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/multidevice/secure_message_delegate_impl.h"
#include "chromeos/ash/services/device_sync/async_execution_time_metrics_logger.h"
#include "chromeos/ash/services/device_sync/cryptauth_task_metrics_logger.h"

namespace ash {

namespace device_sync {

// static
RemoteDeviceV2LoaderImpl::Factory*
    RemoteDeviceV2LoaderImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<RemoteDeviceV2Loader>
RemoteDeviceV2LoaderImpl::Factory::Create() {
  if (test_factory_)
    return test_factory_->CreateInstance();

  return base::WrapUnique(new RemoteDeviceV2LoaderImpl());
}

// static
void RemoteDeviceV2LoaderImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

RemoteDeviceV2LoaderImpl::Factory::~Factory() = default;

RemoteDeviceV2LoaderImpl::RemoteDeviceV2LoaderImpl()
    : secure_message_delegate_(
          multidevice::SecureMessageDelegateImpl::Factory::Create()) {}

RemoteDeviceV2LoaderImpl::~RemoteDeviceV2LoaderImpl() = default;

void RemoteDeviceV2LoaderImpl::Load(
    const CryptAuthDeviceRegistry::InstanceIdToDeviceMap& id_to_device_map,
    const std::string& user_email,
    const std::string& user_private_key,
    LoadCallback callback) {
  DCHECK(!user_email.empty());
  DCHECK(!user_private_key.empty());

  DCHECK(callback_.is_null());
  callback_ = std::move(callback);

  DCHECK(id_to_device_map_.empty());
  id_to_device_map_ = id_to_device_map;
  if (id_to_device_map_.empty()) {
    std::move(callback_).Run(remote_devices_);
    return;
  }

  DCHECK(remaining_ids_to_process_.empty());
  for (const auto& id_device_pair : id_to_device_map_)
    remaining_ids_to_process_.insert(id_device_pair.first);

  for (const auto& id_device_pair : id_to_device_map_) {
    if (!id_device_pair.second.better_together_device_metadata ||
        id_device_pair.second.better_together_device_metadata->public_key()
            .empty()) {
      AddRemoteDevice(id_device_pair.second, user_email,
                      std::string() /* psk */);
      continue;
    }

    // Performs ECDH key agreement to generate a shared secret between the local
    // device and the remote device of |id_device_pair|.
    secure_message_delegate_->DeriveKey(
        user_private_key,
        id_device_pair.second.better_together_device_metadata->public_key(),
        base::BindOnce(&RemoteDeviceV2LoaderImpl::OnPskDerived,
                       base::Unretained(this), id_device_pair.second,
                       user_email));
  }
}

void RemoteDeviceV2LoaderImpl::OnPskDerived(const CryptAuthDevice& device,
                                            const std::string& user_email,
                                            const std::string& psk) {
  if (psk.empty())
    PA_LOG(WARNING) << "Derived persistent symmetric key is empty.";

  AddRemoteDevice(device, user_email, psk);
}

void RemoteDeviceV2LoaderImpl::AddRemoteDevice(const CryptAuthDevice& device,
                                               const std::string& user_email,
                                               const std::string& psk) {
  const std::optional<cryptauthv2::BetterTogetherDeviceMetadata>&
      beto_metadata = device.better_together_device_metadata;
  remote_devices_.emplace_back(
      user_email, device.instance_id(), device.device_name,
      beto_metadata ? beto_metadata->no_pii_device_name() : std::string(),
      beto_metadata ? beto_metadata->public_key() : std::string(), psk,
      device.last_update_time.InMillisecondsSinceUnixEpoch(),
      device.feature_states,
      beto_metadata ? multidevice::FromCryptAuthV2SeedRepeatedPtrField(
                          beto_metadata->beacon_seeds())
                    : std::vector<multidevice::BeaconSeed>(),
      beto_metadata ? beto_metadata->bluetooth_public_address()
                    : std::string());

  remaining_ids_to_process_.erase(device.instance_id());

  if (remaining_ids_to_process_.empty())
    std::move(callback_).Run(remote_devices_);
}

}  // namespace device_sync

}  // namespace ash
