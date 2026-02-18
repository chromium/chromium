// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_FEATURE_UTILS_H_
#define COMPONENTS_COMMERCE_CORE_FEATURE_UTILS_H_

namespace commerce {

class AccountChecker;

// This is a feature check for the "shopping list". This will only return true
// if the user has the feature flag enabled, is signed-in, has MSBB enabled,
// has webapp activity enabled, is allowed by enterprise policy, and (if
// applicable) in an eligible country and locale. The value returned by this
// method can change at runtime, so it should not be used when deciding
// whether to create critical, feature-related infrastructure.
bool IsShoppingListEligible(AccountChecker* account_checker);

// Returns whether the API for getting price insights is available for use. This
// considers the user's region and locale.
bool IsPriceInsightsApiEnabled(AccountChecker* account_checker);

// This is a feature check for the "price insights", which will return true
// if the user has the feature flag enabled, has MSBB enabled, and (if
// applicable) is in an eligible country and locale. The value returned by
// this method can change at runtime, so it should not be used when deciding
// whether to create critical, feature-related infrastructure.
bool IsPriceInsightsEligible(AccountChecker* account_checker);

// Returns whether the subscriptions API is available for use. This considers
// the user's region and locale and is not necessarily bound to any specific
// user-facing feature.
bool IsSubscriptionsApiEnabled(AccountChecker* account_checker);

// Returns whether the price annotations feature is enabled. This check will
// check allowed country and locale.
bool IsPriceAnnotationsEnabled(AccountChecker* account_checker);

// Whether APIs like |GetDiscountInfoForUrls| are enabled and allowed to be
// used.
bool IsDiscountInfoApiEnabled(AccountChecker* account_checker);

// This is a feature check for "show discounts on navigation", which will
// return true if the user has the feature flag enabled, is signed-in and
// synced, has MSBB enabled, and (if applicable) is in an eligible country and
// locale. The value returned by this method can change at runtime, so it
// should not be used when deciding whether to create critical,
// feature-related infrastructure.
bool IsDiscountEligibleToShowOnNavigation(AccountChecker* account_checker);

// This is a feature check for the "merchant viewer", which will return true
// if the user has the feature flag enabled or (if applicable) is in an
// enabled country and locale.
bool IsMerchantViewerEnabled(AccountChecker* account_checker);

// Returns whether the |IsShoppingPage| API is available for use. This considers
// the user's region and locale and is not necessarily bound to any specific
// user-facing feature.
bool IsShoppingPageTypesApiEnabled(AccountChecker* account_checker);

// This is a feature check for showing discounts at the checkout page, which
// will return true if the user has the feature flag enabled, is signed-in and
// synced, has MSBB enabled, and (if applicable) is in an eligible country and
// locale. The value returned by this method can change at runtime, so it
// should not be used when deciding whether to create critical,
// feature-related infrastructure.
bool IsDiscountAutofillEnabled(AccountChecker* account_checker);
}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_FEATURE_UTILS_H_
