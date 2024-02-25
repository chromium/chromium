// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_key_registry_impl.h"

#include "base/memory/ptr_util.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/services/device_sync/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash::device_sync {

// static
CryptAuthKeyRegistryImpl::Factory*
    CryptAuthKeyRegistryImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<CryptAuthKeyRegistry> CryptAuthKeyRegistryImpl::Factory::Create(
    PrefService* pref_service) {
  if (test_factory_)
    return test_factory_->CreateInstance(pref_service);

  return base::WrapUnique(new CryptAuthKeyRegistryImpl(pref_service));
}

// static
void CryptAuthKeyRegistryImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

CryptAuthKeyRegistryImpl::Factory::~Factory() = default;

// static
void CryptAuthKeyRegistryImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kCryptAuthKeyRegistry);
}

CryptAuthKeyRegistryImpl::CryptAuthKeyRegistryImpl(PrefService* pref_service)
    : pref_service_(pref_service) {
  const base::Value::Dict& dict =
      pref_service_->GetDict(prefs::kCryptAuthKeyRegistry);

  for (const CryptAuthKeyBundle::Name& name : CryptAuthKeyBundle::AllNames()) {
    std::string name_string =
        CryptAuthKeyBundle::KeyBundleNameEnumToString(name);
    const base::Value::Dict* bundle_dict = dict.FindDict(name_string);
    if (!bundle_dict)
      continue;

    std::optional<CryptAuthKeyBundle> bundle =
        CryptAuthKeyBundle::FromDictionary(*bundle_dict);
    if (!bundle) {
      PA_LOG(ERROR) << "Error retrieving key bundle " << name_string
                    << " from CryptAuthKeyRegistry pref.";
      continue;
    }

    key_bundles_.insert_or_assign(name, *bundle);
  }
}

CryptAuthKeyRegistryImpl::~CryptAuthKeyRegistryImpl() = default;

void CryptAuthKeyRegistryImpl::OnKeyRegistryUpdated() {
  pref_service_->SetDict(prefs::kCryptAuthKeyRegistry, AsDictionary());
}

base::Value::Dict CryptAuthKeyRegistryImpl::AsDictionary() const {
  base::Value::Dict dict;
  for (const auto& name_bundle_pair : key_bundles_) {
    dict.Set(
        CryptAuthKeyBundle::KeyBundleNameEnumToString(name_bundle_pair.first),
        name_bundle_pair.second.AsDictionary());
  }

  return dict;
}

}  // namespace ash::device_sync
