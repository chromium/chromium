// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_KEY_DATA_PROVIDER_H_
#define COMPONENTS_METRICS_STRUCTURED_KEY_DATA_PROVIDER_H_

#include "base/functional/callback_forward.h"
#include "components/metrics/structured/key_data.h"

namespace base {
class FilePath;
}

namespace metrics::structured {

// Interface to provide key data to be used for hashing projects.
//
// There are two types of keys: device keys and profile keys. Device keys will
// be ready only InitializeDeviceKey has been called while profile keys should
// be ready once InitializeProfileKey has been called.
class KeyDataProvider {
 public:
  KeyDataProvider() = default;

  KeyDataProvider(const KeyDataProvider& key_data_provider) = delete;
  KeyDataProvider& operator=(const KeyDataProvider& key_data_provider) = delete;

  virtual ~KeyDataProvider() = default;

  // Initializes the device key data.
  virtual void InitializeDeviceKey(base::OnceClosure callback) = 0;

  // Called whenever a profile key should be initialized.
  virtual void InitializeProfileKey(const base::FilePath& profile_path,
                                    base::OnceClosure callback) = 0;

  // Returns the device key data.
  //
  // Returns nullptr if InitializeDeviceKey() has not been called or is in
  // progress.
  virtual KeyData* GetDeviceKeyData() = 0;

  // Returns the profile key data, if available. A call to HasProfileKey()
  // should guarantee that this value will not be nullptr.
  //
  // Returns nullptr otherwise.
  virtual KeyData* GetProfileKeyData() = 0;

  // Deletes all key data associated with the provider.
  virtual void Purge() = 0;

  virtual bool HasProfileKey() = 0;
  virtual bool HasDeviceKey() = 0;
};

}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_KEY_DATA_PROVIDER_H_
