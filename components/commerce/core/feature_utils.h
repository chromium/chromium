// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_FEATURE_UTILS_H_
#define COMPONENTS_COMMERCE_CORE_FEATURE_UTILS_H_

class PrefService;

namespace commerce {

class AccountChecker;
class ProductSpecificationsService;

// This is a feature check for the "shopping list". This will only return true
// if the user has the feature flag enabled, is signed-in, has MSBB enabled,
// has webapp activity enabled, is allowed by enterprise policy, and (if
// applicable) in an eligible country and locale. The value returned by this
// method can change at runtime, so it should not be used when deciding
// whether to create critical, feature-related infrastructure.
bool IsShoppingListEligible(AccountChecker* account_checker);

// Check if the product specifications feature is allowed for enterprise.
bool IsProductSpecificationsAllowedForEnterprise(PrefService* prefs);

// Returns whether quality logging is allowed for the product specifications
// feature. This is directly tied to the enterprise setting.
bool IsProductSpecificationsQualityLoggingAllowed(PrefService* prefs);

// Returns whether the sync type for product specifications is enabled and
// syncing.
bool IsSyncingProductSpecifications(AccountChecker* account_checker);

// Returns whether the full-page UI for product specifications is allowed to
// load.
bool CanLoadProductSpecificationsFullPageUi(AccountChecker* account_checker);

// Returns whether a user is allowed to manage their product specifications
// sets. This check is not 1:1 with the feature being enabled. There are some
// cases where we'd like the user to be able to view or remove their sets
// without necessarily being able to use the full feature.
bool CanManageProductSpecificationsSets(
    AccountChecker* account_checker,
    ProductSpecificationsService* product_spec_service);

// Returns whether the data for product specifications can be fetched. This
// should be used to test if we can call the product specs backend. The user
// may still be able to manage their sets.
bool CanFetchProductSpecificationsData(AccountChecker* account_checker);

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_FEATURE_UTILS_H_
