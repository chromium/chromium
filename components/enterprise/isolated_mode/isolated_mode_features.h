// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_ISOLATED_MODE_ISOLATED_MODE_FEATURES_H_
#define COMPONENTS_ENTERPRISE_ISOLATED_MODE_ISOLATED_MODE_FEATURES_H_

#include "base/feature_list.h"

namespace enterprise_isolated_mode {

// Enables Enterprise Isolated Mode.
BASE_DECLARE_FEATURE(kEnableEnterpriseIsolatedMode);

// Enables Milestone 2 of Enterprise Isolated Mode.
BASE_DECLARE_FEATURE(kEnterpriseIsolatedModeMilestone2);

// Returns whether CAA is enabled in Isolated Mode.
bool IsCaaEnabledInIsolatedMode();

namespace switches {
inline constexpr char kForceEnterpriseIsolatedModeReplacesIncognito[] =
    "force-enterprise-isolated-mode";
}  // namespace switches

}  // namespace enterprise_isolated_mode

#endif  // COMPONENTS_ENTERPRISE_ISOLATED_MODE_ISOLATED_MODE_FEATURES_H_
