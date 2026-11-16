// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_GCM_REGISTRATION_MANAGER_IMPL_H_
#define COMPONENTS_GCM_DRIVER_GCM_REGISTRATION_MANAGER_IMPL_H_

#include <map>
#include <string>
#include <utility>

#include "base/memory/raw_ref.h"
#include "base/sequence_checker.h"
#include "components/gcm_driver/gcm_registration_manager.h"

namespace gcm {

class GCMStore;

// `GCMRegistrationManager` implementation that caches and persists registration
// and Instance ID data in `GCMStore`.
class GCMRegistrationManagerImpl : public GCMRegistrationManager {
 public:
  using InstanceIDDataMap =
      std::map<std::string, std::pair<std::string, std::string>>;

  // `gcm_store` must outlive this instance.
  explicit GCMRegistrationManagerImpl(GCMStore& gcm_store);

  GCMRegistrationManagerImpl(const GCMRegistrationManagerImpl&) = delete;
  GCMRegistrationManagerImpl& operator=(const GCMRegistrationManagerImpl&) =
      delete;

  ~GCMRegistrationManagerImpl() override;

  // GCMRegistrationManager implementation:
  void AddInstanceIDData(const std::string& app_id,
                         const std::string& instance_id,
                         const std::string& extra_data) override;

  // TODO(b/545115204): Remove temporary accessors once all Instance ID
  // methods are migrated from `GCMClientImpl`.
  const InstanceIDDataMap& instance_id_data() const {
    return instance_id_data_;
  }
  InstanceIDDataMap& mutable_instance_id_data() { return instance_id_data_; }

 private:
  const raw_ref<GCMStore> gcm_store_;

  // Cached Instance ID data, key is app ID and value is pair of Instance ID
  // and extra data.
  InstanceIDDataMap instance_id_data_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_GCM_REGISTRATION_MANAGER_IMPL_H_
