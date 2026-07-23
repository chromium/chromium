// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/permissions/autofill_ai/autofill_ai_personal_context_enablement_utils.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/system/sys_info.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

base::flat_set<int32_t> GetAutofillAmbientAutofillEligibleTiers() {
  const std::string tier_list =
      features::kAutofillAmbientAutofillEligibleTiers.Get();
  const std::vector<std::string_view> tier_pieces = base::SplitStringPiece(
      tier_list, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  base::flat_set<int32_t> eligible_tiers;
  eligible_tiers.reserve(tier_pieces.size());
  for (std::string_view piece : tier_pieces) {
    int32_t tier_id = 0;
    if (base::StringToInt(piece, &tier_id)) {
      eligible_tiers.insert(tier_id);
    }
  }
  return eligible_tiers;
}

[[nodiscard]] bool IsAndroidDeviceEligibleForAmbientAutofill() {
#if BUILDFLAG(IS_ANDROID)
  const std::string model_name = base::SysInfo::HardwareModelName();
  const base::flat_set<std::string> enabled_devices =
      base::SplitString(features::kAutofillAmbientAutofillEnabledDevices.Get(),
                        ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  return enabled_devices.contains(model_name);
#else
  return false;
#endif
}

}  // namespace autofill
