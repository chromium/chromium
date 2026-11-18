// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_registration_manager_impl.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_split.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "google_apis/gcm/base/gcm_features.h"
#include "google_apis/gcm/engine/gcm_store.h"

namespace gcm {

namespace {

// Client-side cache freshness threshold after which a cached registration/token
// is treated as stale and re-validated with the server on the next
// registration/token request (registrations do not expire on the server after 7
// days).
constexpr base::TimeDelta kTokenInvalidationInterval = base::Days(7);

// Previously, GCMAccountMapper registered with this app ID. Any persisted
// registration for it in GCMStore is obsolete and removed on load.
// Deprecated in October 2026.
constexpr char kDeprecatedGCMAccountMapperAppId[] = "com.google.android.gms";

// Serializes `instance_id` and `extra_data` into "<instance_id>,<extra_data>".
std::string SerializeInstanceIDData(const std::string& instance_id,
                                    const std::string& extra_data) {
  CHECK(!instance_id.empty() && !extra_data.empty());
  CHECK(instance_id.find(',') == std::string::npos);
  return instance_id + "," + extra_data;
}

// Deserializes "<instance_id>,<extra_data>" into `{instance_id, extra_data}`,
// or returns `std::nullopt` if `serialized_data` is malformed or either field
// is empty.
std::optional<std::pair<std::string, std::string>> DeserializeInstanceIDData(
    std::string_view serialized_data) {
  std::optional<std::pair<std::string_view, std::string_view>> parts =
      base::SplitStringOnce(serialized_data, ',');
  if (!parts || parts->first.empty() || parts->second.empty()) {
    return std::nullopt;
  }
  return std::make_pair(std::string(parts->first), std::string(parts->second));
}

}  // namespace

GCMRegistrationManagerImpl::GCMRegistrationManagerImpl(GCMStore& gcm_store,
                                                       base::Clock& clock)
    : gcm_store_(gcm_store), clock_(clock) {}

GCMRegistrationManagerImpl::~GCMRegistrationManagerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void GCMRegistrationManagerImpl::OnRegistrationDataLoaded(
    const GCMStore::LoadResult& load_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& [key, value] : load_result.registrations) {
    std::string registration_id;
    scoped_refptr<RegistrationInfo> registration =
        RegistrationInfo::BuildFromString(key, value, &registration_id);
    // TODO(jianli): Add UMA to track the error case.
    if (registration) {
      registrations_.emplace(std::move(registration), registration_id);
    }
  }

  for (const auto& [key, value] : load_result.instance_id_data) {
    if (std::optional<std::pair<std::string, std::string>> data =
            DeserializeInstanceIDData(value)) {
      instance_id_data_[key] = *std::move(data);
    }
  }

  RemoveDeprecatedData();
}

bool GCMRegistrationManagerImpl::HasStandaloneRegisteredApp() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !registrations_.empty();
}

std::optional<std::string> GCMRegistrationManagerImpl::GetCachedRegistrationId(
    scoped_refptr<RegistrationInfo> registration_info) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // `RegistrationInfoComparer` matches `GCMRegistrationInfo` by `app_id` alone
  // and `InstanceIDTokenInfo` by `(app_id, authorized_entity, scope)`, so
  // `sender_ids` and `last_validated` must be read from the cached map key
  // (`registrations_iter->first`) rather than the lookup `registration_info`.
  RegistrationInfoMap::const_iterator registrations_iter =
      registrations_.find(registration_info);
  if (registrations_iter == registrations_.end()) {
    return std::nullopt;
  }

  // For GCM registration, also match the sender IDs since multiple
  // registrations are not supported.
  const GCMRegistrationInfo* gcm_registration_info =
      GCMRegistrationInfo::FromRegistrationInfo(registration_info.get());
  if (gcm_registration_info) {
    const GCMRegistrationInfo* cached_gcm_registration_info =
        GCMRegistrationInfo::FromRegistrationInfo(
            registrations_iter->first.get());
    CHECK(cached_gcm_registration_info);
    if (gcm_registration_info->sender_ids !=
        cached_gcm_registration_info->sender_ids) {
      return std::nullopt;
    }
  }

  // Do not reuse the cached registration if it has become stale.
  base::TimeDelta time_since_last_validation =
      clock_->Now() - registrations_iter->first->last_validated;
  if (time_since_last_validation >= kTokenInvalidationInterval) {
    return std::nullopt;
  }

  return registrations_iter->second;
}

bool GCMRegistrationManagerImpl::ValidateRegistration(
    scoped_refptr<RegistrationInfo> registration_info,
    const std::string& registration_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Must have a cached registration.
  RegistrationInfoMap::const_iterator registrations_iter =
      registrations_.find(registration_info);
  if (registrations_iter == registrations_.end()) {
    return false;
  }

  // Cached registration ID must match.
  const std::string& cached_registration_id = registrations_iter->second;
  if (registration_id != cached_registration_id) {
    return false;
  }

  // For GCM registration, also match the sender IDs since multiple
  // registrations are not supported.
  const GCMRegistrationInfo* gcm_registration_info =
      GCMRegistrationInfo::FromRegistrationInfo(registration_info.get());
  if (gcm_registration_info) {
    const GCMRegistrationInfo* cached_gcm_registration_info =
        GCMRegistrationInfo::FromRegistrationInfo(
            registrations_iter->first.get());
    CHECK(cached_gcm_registration_info);
    if (gcm_registration_info->sender_ids !=
        cached_gcm_registration_info->sender_ids) {
      return false;
    }
  }

  return true;
}

std::vector<std::string> GCMRegistrationManagerImpl::GetRegisteredAppIds()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<std::string> app_ids;
  for (const auto& [registration_info, registration_id] : registrations_) {
    app_ids.push_back(registration_info->app_id);
  }
  return app_ids;
}

void GCMRegistrationManagerImpl::AddInstanceIDData(
    const std::string& app_id,
    const std::string& instance_id,
    const std::string& extra_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // `InstanceIDImpl` re-sends the Instance ID before every token operation;
  // skip unchanged entries to avoid a synchronous LevelDB write on startup.
  auto iter = instance_id_data_.find(app_id);
  if (iter != instance_id_data_.end() && iter->second.first == instance_id &&
      iter->second.second == extra_data) {
    return;
  }
  instance_id_data_[app_id] = std::make_pair(instance_id, extra_data);
  // TODO(crbug.com/40109289): If this call fails, a registration may be leaked
  // (the entry stored in `instance_id_data_` would be used for a registration
  // but not persisted).
  gcm_store_->AddInstanceIDData(
      app_id, SerializeInstanceIDData(instance_id, extra_data),
      base::DoNothing());
}

void GCMRegistrationManagerImpl::RemoveInstanceIDData(
    const std::string& app_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  instance_id_data_.erase(app_id);
  // TODO(crbug.com/40691191): Implement proper error handling.
  gcm_store_->RemoveInstanceIDData(app_id, base::DoNothing());
}

std::pair<std::string, std::string>
GCMRegistrationManagerImpl::GetInstanceIDData(const std::string& app_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto iter = instance_id_data_.find(app_id);
  if (iter == instance_id_data_.end()) {
    return {};
  }
  return iter->second;
}

void GCMRegistrationManagerImpl::RemoveDeprecatedData() {
  if (!base::FeatureList::IsEnabled(features::kGCMRemoveDeprecatedStoreData)) {
    return;
  }
  auto deprecated_info = base::MakeRefCounted<GCMRegistrationInfo>();
  deprecated_info->app_id = kDeprecatedGCMAccountMapperAppId;
  if (registrations_.erase(deprecated_info)) {
    gcm_store_->RemoveRegistration(kDeprecatedGCMAccountMapperAppId,
                                   base::DoNothing());
  }
}

}  // namespace gcm
