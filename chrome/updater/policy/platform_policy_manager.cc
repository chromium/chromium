// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/policy/platform_policy_manager.h"

#include <optional>

#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "chrome/updater/policy/manager.h"

namespace updater {

#if !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_MAC)
scoped_refptr<PolicyManagerInterface> CreatePlatformPolicyManager(
    std::optional<bool> override_is_managed_device) {
  return nullptr;
}
#endif

}  // namespace updater
