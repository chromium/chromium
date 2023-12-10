// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_KEY_DATA_PROVIDER_H_
#define COMPONENTS_METRICS_STRUCTURED_KEY_DATA_PROVIDER_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
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
  // Observer to be notified of events regarding the KeyDataProvider state.
  class Observer : public base::CheckedObserver {
   public:
    // Called when a key is ready to be used.
    virtual void OnKeyReady() = 0;
  };

  KeyDataProvider();

  KeyDataProvider(const KeyDataProvider& key_data_provider) = delete;
  KeyDataProvider& operator=(const KeyDataProvider& key_data_provider) = delete;

  virtual ~KeyDataProvider();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns true if the keys are ready to be used.
  virtual bool IsReady() = 0;

  // Called whenever a profile is added.
  virtual void OnProfileAdded(const base::FilePath& profile_path) = 0;

  // Retrieves the ID for given |project_name|.
  //
  // If no valid key is found for |project_name|, this function will return
  // absl::nullopt.
  virtual std::optional<uint64_t> GetId(const std::string& project_name) = 0;

  // Retrieves the secondary ID for given |project_name|.
  //
  // If no valid secondary key is found for |project_name|, this function will
  // return absl::nullopt.
  //
  // TODO(b/290096302): Refactor event sequence populator so there is no
  // dependency on concepts such as device/profile in //components.
  virtual std::optional<uint64_t> GetSecondaryId(
      const std::string& project_name) = 0;

  // Retrieves the key data to be used for |project_name|. Returns nullptr if
  // the KeyData is not available for given |project_name|.
  virtual KeyData* GetKeyData(const std::string& project_name) = 0;

  // Deletes all key data associated with the provider.
  virtual void Purge() = 0;

 protected:
  // Notifies observers that the key is ready.
  void NotifyKeyReady();

 private:
  base::ObserverList<Observer> observers_;
};

}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_KEY_DATA_PROVIDER_H_
