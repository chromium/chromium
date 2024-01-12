// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_DEVICE_REGISTRY_IMPL_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_DEVICE_REGISTRY_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chromeos/ash/services/device_sync/cryptauth_device_registry.h"

class PrefRegistrySimple;
class PrefService;

namespace ash {

namespace device_sync {

// Implementation of CryptAuthDeviceRegistry that persists the Instance ID to
// CryptAuthDevice map as a preference. The in-memory map is populated with
// these persisted devices on construction, and the preference is updated
// whenever the in-memory map changes.
class CryptAuthDeviceRegistryImpl : public CryptAuthDeviceRegistry {
 public:
  class Factory {
   public:
    static std::unique_ptr<CryptAuthDeviceRegistry> Create(
        PrefService* pref_service);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<CryptAuthDeviceRegistry> CreateInstance(
        PrefService* pref_service) = 0;

   private:
    static Factory* test_factory_;
  };

  // Registers the prefs used by this class to the given |registry|.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  CryptAuthDeviceRegistryImpl(const CryptAuthDeviceRegistryImpl&) = delete;
  CryptAuthDeviceRegistryImpl& operator=(const CryptAuthDeviceRegistryImpl&) =
      delete;

  ~CryptAuthDeviceRegistryImpl() override;

 private:
  // Populates the in-memory map from Instance ID to device with the map
  // persisted in a pref.
  explicit CryptAuthDeviceRegistryImpl(PrefService* pref_service);

  // CryptAuthDeviceRegistry:
  void OnDeviceRegistryUpdated() override;

  // Converts the registry to a dictionary value in a form suitable for a pref.
  base::Value::Dict AsDictionary() const;

  // Contains preferences that outlive the lifetime of this object and across
  // process restarts. Not owned and must outlive this instance.
  raw_ptr<PrefService> pref_service_;
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_DEVICE_REGISTRY_IMPL_H_
