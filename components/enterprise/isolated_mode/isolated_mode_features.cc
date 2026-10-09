// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/isolated_mode/isolated_mode_features.h"

namespace enterprise_isolated_mode {

BASE_FEATURE(kEnableEnterpriseIsolatedMode, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kEnterpriseIsolatedModeMilestone2,
             base::FEATURE_DISABLED_BY_DEFAULT);

// For now CAA in Isolated Mode is controlled by the same flag as Milestone 2.
bool IsCaaEnabledInIsolatedMode() {
  return base::FeatureList::IsEnabled(kEnterpriseIsolatedModeMilestone2);
}

}  // namespace enterprise_isolated_mode
