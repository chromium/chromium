// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_FEATURE_UTILS_H_
#define COMPONENTS_COMMERCE_CORE_FEATURE_UTILS_H_

class PrefService;

namespace commerce {

class AccountChecker;

// This is a feature check for the "shopping list". This will only return true
// if the user has the feature flag enabled, is signed-in, has MSBB enabled,
// has webapp activity enabled, is allowed by enterprise policy, and (if
// applicable) in an eligible country and locale. The value returned by this
// method can change at runtime, so it should not be used when deciding
// whether to create critical, feature-related infrastructure.
bool IsShoppingListEligible(AccountChecker* account_checker);

// Check if the product specifications feature is allowed for enterprise.
bool IsProductSpecificationsAllowedForEnterprise(PrefService* prefs);

// Returns whether the product specifications feature is enabled. This checks
// the enterprise setting as well as the feature flag.
bool IsProductSpecificationsEnabled(AccountChecker* account_checker);

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_FEATURE_UTILS_H_
