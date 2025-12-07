// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SETTINGS_DEVICE_SETTINGS_CACHE_TEST_SUPPORT_H_
#define CHROMEOS_ASH_COMPONENTS_SETTINGS_DEVICE_SETTINGS_CACHE_TEST_SUPPORT_H_

#include <concepts>

#include "chromeos/ash/components/settings/device_settings_cache.h"
#include "components/policy/proto/device_management_backend.pb.h"

class PrefService;

namespace ash::device_settings_cache {

// Updates cache of device_settings policy data in local_state for testing.
// F is a lambda, being called in the function synchronously with the policy
// data of read from local_state. F is expected to update the passed
// policy_data, then the updated value will be (re-)stored into the
// local_state.
template <typename F>
  requires std::invocable<F, enterprise_management::PolicyData&>
bool Update(PrefService* local_state, F&& f) {
  enterprise_management::PolicyData policy_data;
  if (!Retrieve(&policy_data, local_state)) {
    policy_data.Clear();
  }
  f(policy_data);
  if (!Store(policy_data, local_state)) {
    return false;
  }
  return true;
}

}  // namespace ash::device_settings_cache

#endif  // CHROMEOS_ASH_COMPONENTS_SETTINGS_DEVICE_SETTINGS_CACHE_TEST_SUPPORT_H_
