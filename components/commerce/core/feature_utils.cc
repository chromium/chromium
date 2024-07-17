// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/feature_utils.h"

#include "components/commerce/core/account_checker.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/pref_names.h"
#include "components/prefs/pref_service.h"

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
  // TODO(b/325109916): Implement enterprise policy.
  return true;
}

bool IsProductSpecificationsEnabled(AccountChecker* account_checker) {
  // TODO(352761768): Reintroduce the "region launched" version of the flag
  //                  with a supplementary kill switch flag so that it's
  //                  possible turn the whole feature off using one flag
  //                  while also supporting our "staggered" rollout.
  return base::FeatureList::IsEnabled(kProductSpecifications) &&
         IsEnabledForCountryAndLocale(kProductSpecifications,
                                      account_checker->GetCountry(),
                                      account_checker->GetLocale()) &&
         IsProductSpecificationsAllowedForEnterprise(
             account_checker->GetPrefs()) &&
         account_checker->IsSignedIn();
}

}  // namespace commerce
