// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_POLICY_PLATFORM_POLICY_MANAGER_H_
#define CHROME_UPDATER_POLICY_PLATFORM_POLICY_MANAGER_H_

#include <optional>

#include "base/memory/scoped_refptr.h"
#include "chrome/updater/policy/manager.h"

namespace updater {

// A factory method to create the platform-specific policy manager.
// This is Group Policy on Windows or Managed Preferences on macOS.
scoped_refptr<PolicyManagerInterface> CreatePlatformPolicyManager(
    std::optional<bool> override_is_managed_device);

}  // namespace updater

#endif  // CHROME_UPDATER_POLICY_PLATFORM_POLICY_MANAGER_H_
