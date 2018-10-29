// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/fake_host_device_timestamp_manager.h"

namespace chromeos {

namespace multidevice_setup {

FakeHostDeviceTimestampManager::FakeHostDeviceTimestampManager() {
  was_host_set_from_this_chromebook_ = false;
}

FakeHostDeviceTimestampManager::~FakeHostDeviceTimestampManager() = default;

void FakeHostDeviceTimestampManager::set_was_host_set_from_this_chromebook(
    bool was_host_set_from_this_chromebook) {
  was_host_set_from_this_chromebook_ = was_host_set_from_this_chromebook;
}

void FakeHostDeviceTimestampManager::set_completion_timestamp(
    const base::Time& timestamp) {
  completion_time_ = timestamp;
}

void FakeHostDeviceTimestampManager::set_verification_timestamp(
    const base::Time& timestamp) {
  verification_time_ = timestamp;
}

bool FakeHostDeviceTimestampManager::WasHostSetFromThisChromebook() {
  return was_host_set_from_this_chromebook_;
}

base::Optional<base::Time>
FakeHostDeviceTimestampManager::GetLatestSetupFlowCompletionTimestamp() {
  return completion_time_;
}

base::Optional<base::Time>
FakeHostDeviceTimestampManager::GetLatestVerificationTimestamp() {
  return verification_time_;
}

}  // namespace multidevice_setup

}  // namespace chromeos
