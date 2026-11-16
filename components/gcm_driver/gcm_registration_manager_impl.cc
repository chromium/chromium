// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_registration_manager_impl.h"

#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "google_apis/gcm/engine/gcm_store.h"

namespace gcm {

namespace {

std::string SerializeInstanceIDData(const std::string& instance_id,
                                    const std::string& extra_data) {
  CHECK(!instance_id.empty() && !extra_data.empty());
  CHECK(instance_id.find(',') == std::string::npos);
  return instance_id + "," + extra_data;
}

}  // namespace

GCMRegistrationManagerImpl::GCMRegistrationManagerImpl(GCMStore& gcm_store)
    : gcm_store_(gcm_store) {}

GCMRegistrationManagerImpl::~GCMRegistrationManagerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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

}  // namespace gcm
