// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_KEY_DATA_PROVIDER_H_
#define COMPONENTS_METRICS_STRUCTURED_KEY_DATA_PROVIDER_H_

#include "base/functional/callback_forward.h"
#include "components/metrics/structured/key_data.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

  // Returns true if the keys are ready to be used.
  virtual bool IsReady() = 0;

  // Callback to be made once the key is ready.
  virtual void OnKeyReady() = 0;

  // Initializes the device key data.
  virtual void InitializeDeviceKey(base::OnceClosure callback) = 0;

  // Called whenever a profile key should be initialized.
  virtual void InitializeProfileKey(const base::FilePath& profile_path,
                                    base::OnceClosure callback) = 0;

  // Retrieves the ID for given |project_name|.
  //
  // If no valid key is found for |project_name|, this function will return
  // absl::nullopt.
  virtual absl::optional<uint64_t> GetId(const std::string& project_name) = 0;

  // Retrieves the secondary ID for given |project_name|.
  //
  // If no valid secondary key is found for |project_name|, this function will
  // return absl::nullopt.
  //
  // TODO(b/290096302): Refactor event sequence populator so there is no
  // dependency on concepts such as device/profile in //components.
  virtual absl::optional<uint64_t> GetSecondaryId(
      const std::string& project_name) = 0;

  // Retrieves the key data to be used for |project_name|. Returns nullptr if
  // the KeyData is not available for given |project_name|.
  virtual KeyData* GetKeyData(const std::string& project_name) = 0;

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
