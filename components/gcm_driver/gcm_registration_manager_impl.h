// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_GCM_REGISTRATION_MANAGER_IMPL_H_
#define COMPONENTS_GCM_DRIVER_GCM_REGISTRATION_MANAGER_IMPL_H_

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "components/gcm_driver/gcm_registration_manager.h"
#include "google_apis/gcm/engine/gcm_store.h"

namespace base {
class Clock;
}  // namespace base

namespace gcm {

// `GCMRegistrationManager` implementation that caches and persists registration
// and Instance ID data in `GCMStore`.
class GCMRegistrationManagerImpl : public GCMRegistrationManager {
 public:
  using InstanceIDDataMap =
      std::map<std::string, std::pair<std::string, std::string>>;

  // `gcm_store` and `clock` must outlive this instance.
  GCMRegistrationManagerImpl(GCMStore& gcm_store, base::Clock& clock);

  GCMRegistrationManagerImpl(const GCMRegistrationManagerImpl&) = delete;
  GCMRegistrationManagerImpl& operator=(const GCMRegistrationManagerImpl&) =
      delete;

  ~GCMRegistrationManagerImpl() override;

  // Populates cached registration and Instance ID data from `load_result`.
  void OnRegistrationDataLoaded(const GCMStore::LoadResult& load_result);

  // Returns true if at least one standalone app is registered for GCM.
  bool HasStandaloneRegisteredApp() const;

  // Returns the cached registration ID if a matching, non-stale registration
  // exists in `registrations_`, or `std::nullopt` otherwise.
  std::optional<std::string> GetCachedRegistrationId(
      scoped_refptr<RegistrationInfo> registration_info) const;

  // GCMRegistrationManager implementation:
  bool ValidateRegistration(scoped_refptr<RegistrationInfo> registration_info,
                            const std::string& registration_id) override;
  std::vector<std::string> GetRegisteredAppIds() const override;
  void AddInstanceIDData(const std::string& app_id,
                         const std::string& instance_id,
                         const std::string& extra_data) override;
  void RemoveInstanceIDData(const std::string& app_id) override;
  std::pair<std::string, std::string> GetInstanceIDData(
      const std::string& app_id) const override;

  // TODO(b/545115204): Remove temporary accessors once all registration and
  // Instance ID methods and tests are migrated from `GCMClientImpl`.
  const RegistrationInfoMap& registrations() const { return registrations_; }
  RegistrationInfoMap& mutable_registrations() { return registrations_; }
  const InstanceIDDataMap& instance_id_data() const {
    return instance_id_data_;
  }

 private:
  // Removes deprecated data (such as obsolete registrations) from the cache and
  // persistent store.
  void RemoveDeprecatedData();

  const raw_ref<GCMStore> gcm_store_;
  const raw_ref<base::Clock> clock_;

  // Cached registration info.
  RegistrationInfoMap registrations_;

  // Cached Instance ID data, key is app ID and value is pair of Instance ID
  // and extra data.
  InstanceIDDataMap instance_id_data_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_GCM_REGISTRATION_MANAGER_IMPL_H_
