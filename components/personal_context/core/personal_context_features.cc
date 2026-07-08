// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/personal_context/core/personal_context_features.h"

#include "base/feature_list.h"

namespace personal_context::features {

BASE_FEATURE(kPersonalContext, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPersonalContextLogNonEligibilityUma,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPersonalContextFirstRunOptIn, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsPersonalContextFirstRunOptInEnabled() {
  return base::FeatureList::IsEnabled(kPersonalContextFirstRunOptIn);
}
}  // namespace personal_context::features
