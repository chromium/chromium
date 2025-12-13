// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_PROMOTION_PROMOTION_PREFS_H_
#define COMPONENTS_ENTERPRISE_BROWSER_PROMOTION_PROMOTION_PREFS_H_

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace enterprise_promotion {

// Stores which enterprise promotion to show. The value is an int that
// corresponds to the extensions::PromotionType enum.
extern const char kEnterprisePromotionEligibility[];

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

}  // namespace enterprise_promotion

#endif  // COMPONENTS_ENTERPRISE_BROWSER_PROMOTION_PROMOTION_PREFS_H_
