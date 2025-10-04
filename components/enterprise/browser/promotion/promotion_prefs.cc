// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/promotion/promotion_prefs.h"

#include "base/time/time.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace enterprise_promotion {

const char kEnterprisePromotionEligibility[] =
    "enterprise.promotion_eligibility";

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(
      kEnterprisePromotionEligibility,
      static_cast<int>(
          enterprise_management::PromotionType::PROMOTION_TYPE_UNSPECIFIED));
}

}  // namespace enterprise_promotion
