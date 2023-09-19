// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/cros_evaluate_seed/early_boot_enabled_state_provider.h"

namespace variations::cros_early_boot::evaluate_seed {

EarlyBootEnabledStateProvider::EarlyBootEnabledStateProvider() = default;
EarlyBootEnabledStateProvider::~EarlyBootEnabledStateProvider() = default;

bool EarlyBootEnabledStateProvider::IsConsentGiven() const {
  return false;
}

bool EarlyBootEnabledStateProvider::IsReportingEnabled() const {
  return false;
}

}  // namespace variations::cros_early_boot::evaluate_seed
