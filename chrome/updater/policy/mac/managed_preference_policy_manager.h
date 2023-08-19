// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_POLICY_MAC_MANAGED_PREFERENCE_POLICY_MANAGER_H_
#define CHROME_UPDATER_POLICY_MAC_MANAGED_PREFERENCE_POLICY_MANAGER_H_

#include "base/memory/scoped_refptr.h"
#include "chrome/updater/policy/manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {

// A factory method to create a managed preference policy manager.
scoped_refptr<PolicyManagerInterface> CreateManagedPreferencePolicyManager(
    const absl::optional<bool>& override_is_managed_device = absl::nullopt);

}  // namespace updater

#endif  // CHROME_UPDATER_POLICY_MAC_MANAGED_PREFERENCE_POLICY_MANAGER_H_
