// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_device_registry_impl.h"

#include <optional>
#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/services/device_sync/cryptauth_device.h"
#include "chromeos/ash/services/device_sync/pref_names.h"
#include "chromeos/ash/services/device_sync/value_string_encoding.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash::device_sync {

// static
CryptAuthDeviceRegistryImpl::Factory*
    CryptAuthDeviceRegistryImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<CryptAuthDeviceRegistry>
CryptAuthDeviceRegistryImpl::Factory::Create(PrefService* pref_service) {
  if (test_factory_)
    return test_factory_->CreateInstance(pref_service);

  return base::WrapUnique(new CryptAuthDeviceRegistryImpl(pref_service));
}

// static
void CryptAuthDeviceRegistryImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

CryptAuthDeviceRegistryImpl::Factory::~Factory() = default;

// static
void CryptAuthDeviceRegistryImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kCryptAuthDeviceRegistry);
}

CryptAuthDeviceRegistryImpl::CryptAuthDeviceRegistryImpl(
    PrefService* pref_service)
    : pref_service_(pref_service) {
  const base::Value::Dict& dict =
      pref_service_->GetDict(prefs::kCryptAuthDeviceRegistry);

  CryptAuthDeviceRegistry::InstanceIdToDeviceMap instance_id_to_device_map;
  for (const auto [key, value] : dict) {
    std::optional<std::string> instance_id = util::DecodeFromString(key);
    std::optional<CryptAuthDevice> device =
        CryptAuthDevice::FromDictionary(value.GetDict());
    if (!instance_id || !device || *instance_id != device->instance_id()) {
      PA_LOG(ERROR) << "Error retrieving device with Instance ID " << key
                    << " from preferences.";
      continue;
    }

    instance_id_to_device_map.insert_or_assign(device->instance_id(), *device);
  }

  SetRegistry(instance_id_to_device_map);
}

CryptAuthDeviceRegistryImpl::~CryptAuthDeviceRegistryImpl() = default;

void CryptAuthDeviceRegistryImpl::OnDeviceRegistryUpdated() {
  pref_service_->SetDict(prefs::kCryptAuthDeviceRegistry, AsDictionary());
}

base::Value::Dict CryptAuthDeviceRegistryImpl::AsDictionary() const {
  base::Value::Dict dict;
  for (const std::pair<std::string, CryptAuthDevice>& id_device_pair :
       instance_id_to_device_map()) {
    dict.Set(util::EncodeAsString(id_device_pair.first),
             id_device_pair.second.AsDictionary());
  }

  return dict;
}

}  // namespace ash::device_sync
