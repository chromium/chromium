// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_device_registry_impl.h"

#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/services/device_sync/cryptauth_device.h"
#include "chromeos/services/device_sync/pref_names.h"
#include "chromeos/services/device_sync/value_string_encoding.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

namespace device_sync {

// static
CryptAuthDeviceRegistryImpl::Factory*
    CryptAuthDeviceRegistryImpl::Factory::test_factory_ = nullptr;

// static
CryptAuthDeviceRegistryImpl::Factory*
CryptAuthDeviceRegistryImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<CryptAuthDeviceRegistryImpl::Factory> factory;
  return factory.get();
}

// static
void CryptAuthDeviceRegistryImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

CryptAuthDeviceRegistryImpl::Factory::~Factory() = default;

std::unique_ptr<CryptAuthDeviceRegistry>
CryptAuthDeviceRegistryImpl::Factory::BuildInstance(PrefService* pref_service) {
  return base::WrapUnique(new CryptAuthDeviceRegistryImpl(pref_service));
}

// static
void CryptAuthDeviceRegistryImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kCryptAuthDeviceRegistry);
}

CryptAuthDeviceRegistryImpl::CryptAuthDeviceRegistryImpl(
    PrefService* pref_service)
    : pref_service_(pref_service) {
  const base::Value* dict = pref_service_->Get(prefs::kCryptAuthDeviceRegistry);

  CryptAuthDeviceRegistry::InstanceIdToDeviceMap instance_id_to_device_map;
  for (const std::pair<const std::string&, const base::Value&>& id_device_pair :
       dict->DictItems()) {
    base::Optional<std::string> instance_id =
        util::DecodeFromString(id_device_pair.first);
    base::Optional<CryptAuthDevice> device =
        CryptAuthDevice::FromDictionary(id_device_pair.second);
    if (!instance_id || !device || *instance_id != device->instance_id()) {
      PA_LOG(ERROR) << "Error retrieving device with Instance ID "
                    << id_device_pair.first << " from preferences.";
      continue;
    }

    instance_id_to_device_map.insert_or_assign(device->instance_id(), *device);
  }

  SetRegistry(instance_id_to_device_map);
}

CryptAuthDeviceRegistryImpl::~CryptAuthDeviceRegistryImpl() = default;

void CryptAuthDeviceRegistryImpl::OnDeviceRegistryUpdated() {
  pref_service_->Set(prefs::kCryptAuthDeviceRegistry, AsDictionary());
}

base::Value CryptAuthDeviceRegistryImpl::AsDictionary() const {
  base::Value dict(base::Value::Type::DICTIONARY);
  for (const std::pair<std::string, CryptAuthDevice>& id_device_pair :
       instance_id_to_device_map()) {
    dict.SetKey(util::EncodeAsString(id_device_pair.first),
                id_device_pair.second.AsDictionary());
  }

  return dict;
}

}  // namespace device_sync

}  // namespace chromeos
