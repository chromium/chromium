// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/policy/device_policy/cached_device_policy_updater.h"

#include <string>

#include "base/check.h"
#include "base/check_deref.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"

namespace policy {

CachedDevicePolicyUpdater::CachedDevicePolicyUpdater()
    : session_manager_(CHECK_DEREF(ash::FakeSessionManagerClient::Get())) {
  const std::string& device_policy = session_manager_->device_policy();
  if (!device_policy.empty()) {
    CHECK(policy_builder_.policy().ParseFromString(device_policy));
    CHECK(policy_builder_.policy_data().ParseFromString(
        policy_builder_.policy().policy_data()));
    CHECK(policy_builder_.payload().ParseFromString(
        policy_builder_.policy_data().policy_value()));
  }
}

CachedDevicePolicyUpdater::~CachedDevicePolicyUpdater() {
  CHECK(committed_) << "CachedDevicePolicyUpdater::Commit() must be called";
}

void CachedDevicePolicyUpdater::Commit() {
  CHECK(!committed_)
      << "CachedDevicePolicyUpdater::::Commit() was already called";
  committed_ = true;

  policy_builder_.SetDefaultSigningKey();
  policy_builder_.Build();
  session_manager_->set_device_policy(policy_builder_.GetBlob());
  // Notify that the device policy is updated.
  session_manager_->OnPropertyChangeComplete(true);
}

enterprise_management::PolicyData& CachedDevicePolicyUpdater::policy_data() {
  CHECK(!committed_) << "policy data was already committed";
  return policy_builder_.policy_data();
}
const enterprise_management::PolicyData&
CachedDevicePolicyUpdater::policy_data() const {
  return policy_builder_.policy_data();
}

enterprise_management::ChromeDeviceSettingsProto&
CachedDevicePolicyUpdater::payload() {
  CHECK(!committed_) << "policy data was already committed";
  return policy_builder_.payload();
}
const enterprise_management::ChromeDeviceSettingsProto&
CachedDevicePolicyUpdater::payload() const {
  return policy_builder_.payload();
}

}  // namespace policy
