// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_KEY_REGISTRY_IMPL_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_KEY_REGISTRY_IMPL_H_

#include "base/macros.h"
#include "base/values.h"
#include "chromeos/services/device_sync/cryptauth_key_registry.h"

class PrefRegistrySimple;
class PrefService;

namespace chromeos {

namespace device_sync {

// Implementation of CryptAuthKeyRegistry that persists the key-bundle map as a
// preference. The in-memory key bundle map is populated with these persisted
// key bundles on construction, and the preference is updated whenever the
// in-memory key bundle map changes.
class CryptAuthKeyRegistryImpl : public CryptAuthKeyRegistry {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<CryptAuthKeyRegistry> BuildInstance(
        PrefService* pref_service);

   private:
    static Factory* test_factory_;
  };

  // Registers the prefs used by this class to the given |registry|.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  ~CryptAuthKeyRegistryImpl() override;

 private:
  // Populates the in-memory key bundle map with the key bundles persisted in a
  // pref.
  explicit CryptAuthKeyRegistryImpl(PrefService* pref_service);

  // CryptAuthKeyRegistry:
  void OnKeyRegistryUpdated() override;

  // Converts the registry to a dictionary value in a form suitable for a pref.
  base::Value AsDictionary() const;

  // Contains preferences that outlive the lifetime of this object and across
  // process restarts. Not owned and must outlive this instance.
  PrefService* pref_service_;

  DISALLOW_COPY_AND_ASSIGN(CryptAuthKeyRegistryImpl);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_CRYPTAUTH_KEY_REGISTRY_IMPL_H_
