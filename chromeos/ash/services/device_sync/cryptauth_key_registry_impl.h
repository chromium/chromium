// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_KEY_REGISTRY_IMPL_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_KEY_REGISTRY_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chromeos/ash/services/device_sync/cryptauth_key_registry.h"

class PrefRegistrySimple;
class PrefService;

namespace ash {

namespace device_sync {

// Implementation of CryptAuthKeyRegistry that persists the key-bundle map as a
// preference. The in-memory key bundle map is populated with these persisted
// key bundles on construction, and the preference is updated whenever the
// in-memory key bundle map changes.
class CryptAuthKeyRegistryImpl : public CryptAuthKeyRegistry {
 public:
  class Factory {
   public:
    static std::unique_ptr<CryptAuthKeyRegistry> Create(
        PrefService* pref_service);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<CryptAuthKeyRegistry> CreateInstance(
        PrefService* pref_service) = 0;

   private:
    static Factory* test_factory_;
  };

  // Registers the prefs used by this class to the given |registry|.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  CryptAuthKeyRegistryImpl(const CryptAuthKeyRegistryImpl&) = delete;
  CryptAuthKeyRegistryImpl& operator=(const CryptAuthKeyRegistryImpl&) = delete;

  ~CryptAuthKeyRegistryImpl() override;

 private:
  // Populates the in-memory key bundle map with the key bundles persisted in a
  // pref.
  explicit CryptAuthKeyRegistryImpl(PrefService* pref_service);

  // CryptAuthKeyRegistry:
  void OnKeyRegistryUpdated() override;

  // Converts the registry to a dictionary value in a form suitable for a pref.
  base::Value::Dict AsDictionary() const;

  // Contains preferences that outlive the lifetime of this object and across
  // process restarts. Not owned and must outlive this instance.
  raw_ptr<PrefService> pref_service_;
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_CRYPTAUTH_KEY_REGISTRY_IMPL_H_
