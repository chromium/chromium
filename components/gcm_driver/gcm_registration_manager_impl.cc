// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_registration_manager_impl.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_split.h"
#include "google_apis/gcm/engine/gcm_store.h"

namespace gcm {

namespace {

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

GCMRegistrationManagerImpl::GCMRegistrationManagerImpl(GCMStore& gcm_store)
    : gcm_store_(gcm_store) {}

GCMRegistrationManagerImpl::~GCMRegistrationManagerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void GCMRegistrationManagerImpl::OnRegistrationDataLoaded(
    const GCMStore::LoadResult& load_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& [key, value] : load_result.instance_id_data) {
    if (std::optional<std::pair<std::string, std::string>> data =
            DeserializeInstanceIDData(value)) {
      instance_id_data_[key] = *std::move(data);
    }
  }
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

}  // namespace gcm
