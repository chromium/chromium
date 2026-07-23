// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PERMISSIONS_AUTOFILL_AI_AUTOFILL_AI_PERSONAL_CONTEXT_ENABLEMENT_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PERMISSIONS_AUTOFILL_AI_AUTOFILL_AI_PERSONAL_CONTEXT_ENABLEMENT_UTILS_H_

#include <cstdint>

#include "base/containers/flat_set.h"

namespace autofill {

// Returns the set of eligible subscription tiers configured by feature
// parameters for Ambient Autofill.
base::flat_set<int32_t> GetAutofillAmbientAutofillEligibleTiers();

// Returns whether the current Android hardware model is configured as eligible
// for Ambient Autofill by feature parameters.
[[nodiscard]] bool IsAndroidDeviceEligibleForAmbientAutofill();

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PERMISSIONS_AUTOFILL_AI_AUTOFILL_AI_PERSONAL_CONTEXT_ENABLEMENT_UTILS_H_
