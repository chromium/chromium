// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/feature_utils.h"

#include "components/commerce/core/account_checker.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/pref_names.h"
#include "components/commerce/core/product_specifications/product_specifications_service.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/user_selectable_type.h"

namespace commerce {

bool IsShoppingListEligible(AccountChecker* account_checker) {
  if (!commerce::IsRegionLockedFeatureEnabled(
          kShoppingList, kShoppingListRegionLaunched,
          account_checker->GetCountry(), account_checker->GetLocale())) {
    return false;
  }

  if (!account_checker->GetPrefs() ||
      !IsShoppingListAllowedForEnterprise(account_checker->GetPrefs())) {
    return false;
  }

  // Make sure the user allows subscriptions to be made and that we can fetch
  // store data.
  if (!account_checker || !account_checker->IsSignedIn() ||
      !account_checker->IsSyncingBookmarks() ||
      !account_checker->IsAnonymizedUrlDataCollectionEnabled() ||
      account_checker->IsSubjectToParentalControls()) {
    return false;
  }

  return true;
}

bool IsProductSpecificationsAllowedForEnterprise(PrefService* prefs) {
  // 0 is fully enabled, 1 is enabled without logging, 2 is totally disabled.
  return prefs->GetInteger(optimization_guide::prefs::
                               kProductSpecificationsEnterprisePolicyAllowed) <
             2 ||
         !prefs->IsManagedPreference(
             optimization_guide::prefs::
                 kProductSpecificationsEnterprisePolicyAllowed);
}

bool IsProductSpecificationsQualityLoggingAllowed(PrefService* prefs) {
  // Explicitly check that the enterprise setting is 0. We check the managed
  // state to ensure the policy is correctly defined (all enterprise prefs are
  // managed).
  return prefs->GetInteger(optimization_guide::prefs::
                               kProductSpecificationsEnterprisePolicyAllowed) ==
             0 ||
         !prefs->IsManagedPreference(
             optimization_guide::prefs::
                 kProductSpecificationsEnterprisePolicyAllowed);
}

bool IsSyncingProductSpecifications(AccountChecker* account_checker) {
  return account_checker && account_checker->IsSyncTypeEnabled(
                                syncer::UserSelectableType::kProductComparison);
}

bool CanLoadProductSpecificationsFullPageUi(AccountChecker* account_checker) {
  // TODO(352761768): Reintroduce the "region launched" version of the flag
  //                  with a supplementary kill switch flag so that it's
  //                  possible turn the whole feature off using one flag
  //                  while also supporting our "staggered" rollout.
  return base::FeatureList::IsEnabled(kProductSpecifications);
}

bool CanManageProductSpecificationsSets(
    AccountChecker* account_checker,
    ProductSpecificationsService* product_spec_service) {
  if (!account_checker) {
    return false;
  }

  // If we can't read sync entities, there's nothing to manage. Similarly,
  // being able to load the full-page is required so clicking on the items
  // works correctly.
  if (!IsSyncingProductSpecifications(account_checker) ||
      !CanLoadProductSpecificationsFullPageUi(account_checker)) {
    return false;
  }

  // We allow management of entities if they exist even if the feature is
  // otherwise inaccessible (assuming the flags are on).
  if (!CanFetchProductSpecificationsData(account_checker)) {
    int entity_count =
        product_spec_service
            ? product_spec_service->GetAllProductSpecifications().size()
            : 0;

    if (entity_count == 0) {
      // If there are no entities, we can only manage them if the full page
      // UI is enabled.
      return false;
    }
  }

  return true;
}

bool CanFetchProductSpecificationsData(AccountChecker* account_checker) {
  // msbb, enterprise, parental controls, sync type, and model execution
  // features.
  return account_checker &&
         IsProductSpecificationsAllowedForEnterprise(
             account_checker->GetPrefs()) &&
         account_checker->IsSignedIn() &&
         account_checker->IsAnonymizedUrlDataCollectionEnabled() &&
         !account_checker->IsSubjectToParentalControls() &&
         account_checker->CanUseModelExecutionFeatures() &&
         IsSyncingProductSpecifications(account_checker) &&
         CanLoadProductSpecificationsFullPageUi(account_checker);
}

}  // namespace commerce
