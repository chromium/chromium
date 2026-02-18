// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/feature_utils.h"

#include "base/feature_list.h"
#include "components/commerce/core/account_checker.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/pref_names.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/user_selectable_type.h"

namespace commerce {

bool IsShoppingListEligible(AccountChecker* account_checker) {
  if (!commerce::IsRegionLockedFeatureEnabled(kShoppingList,
                                              account_checker->GetCountry(),
                                              account_checker->GetLocale())) {
    return false;
  }

  if (!account_checker->GetPrefs() ||
      !IsShoppingListAllowedForEnterprise(account_checker->GetPrefs())) {
    return false;
  }

  // Make sure the user allows subscriptions to be made and that we can fetch
  // store data.
  if (!account_checker || !account_checker->IsSignedIn() ||
      !account_checker->IsSyncTypeEnabled(
          syncer::UserSelectableType::kBookmarks) ||
      !account_checker->IsAnonymizedUrlDataCollectionEnabled() ||
      account_checker->IsSubjectToParentalControls()) {
    return false;
  }

  return true;
}

bool IsPriceInsightsApiEnabled(AccountChecker* account_checker) {
  return account_checker && commerce::IsRegionLockedFeatureEnabled(
                                kPriceInsights, account_checker->GetCountry(),
                                account_checker->GetLocale());
}

bool IsPriceInsightsEligible(AccountChecker* account_checker) {
  return IsPriceInsightsApiEnabled(account_checker) &&
         account_checker->IsAnonymizedUrlDataCollectionEnabled();
}

bool IsSubscriptionsApiEnabled(AccountChecker* account_checker) {
  return IsRegionLockedFeatureEnabled(kSubscriptionsApi,
                                      account_checker->GetCountry(),
                                      account_checker->GetLocale());
}

bool IsPriceAnnotationsEnabled(AccountChecker* account_checker) {
  return account_checker &&
         commerce::IsRegionLockedFeatureEnabled(kPriceAnnotations,
                                                account_checker->GetCountry(),
                                                account_checker->GetLocale());
}

bool IsDiscountInfoApiEnabled(AccountChecker* account_checker) {
  return account_checker &&
         commerce::IsRegionLockedFeatureEnabled(kEnableDiscountInfoApi,
                                                account_checker->GetCountry(),
                                                account_checker->GetLocale());
}

bool IsDiscountEligibleToShowOnNavigation(AccountChecker* account_checker) {
  return IsDiscountInfoApiEnabled(account_checker) &&
         account_checker->IsSignedIn() &&
         account_checker->IsAnonymizedUrlDataCollectionEnabled();
}

bool IsMerchantViewerEnabled(AccountChecker* account_checker) {
  return account_checker &&
         commerce::IsRegionLockedFeatureEnabled(kCommerceMerchantViewer,
                                                account_checker->GetCountry(),
                                                account_checker->GetLocale());
}

bool IsShoppingPageTypesApiEnabled(AccountChecker* account_checker) {
  return account_checker &&
         commerce::IsRegionLockedFeatureEnabled(kShoppingPageTypes,
                                                account_checker->GetCountry(),
                                                account_checker->GetLocale());
}

bool IsDiscountAutofillEnabled(AccountChecker* account_checker) {
  return account_checker && account_checker->IsSignedIn() &&
         account_checker->IsAnonymizedUrlDataCollectionEnabled() &&
         commerce::IsRegionLockedFeatureEnabled(kDiscountAutofill,
                                                account_checker->GetCountry(),
                                                account_checker->GetLocale());
}
}  // namespace commerce
