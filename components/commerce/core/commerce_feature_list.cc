// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/commerce_feature_list.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "third_party/re2/src/re2/re2.h"

namespace commerce {

namespace {

constexpr base::FeatureParam<std::string> kPartnerMerchantPattern{
    &commerce::kRetailCoupons, "coupon-partner-merchant-pattern",
    // This regex does not match anything.
    "\\b\\B"};

const re2::RE2& GetPartnerMerchantPattern() {
  re2::RE2::Options options;
  options.set_case_sensitive(false);
  static base::NoDestructor<re2::RE2> instance(kPartnerMerchantPattern.Get(),
                                               options);
  return *instance;
}

}  // namespace

const base::Feature kCommerceMerchantViewer{"CommerceMerchantViewer",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCommercePriceTracking{"CommercePriceTracking",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

const base::FeatureParam<bool> kDeleteAllMerchantsOnClearBrowsingHistory{
    &kCommerceMerchantViewer, "delete_all_merchants_on_clear_history", false};

const base::Feature kShoppingList{"ShoppingList",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kShoppingPDPMetrics{"ShoppingPDPMetrics",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kRetailCoupons{"RetailCoupons",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCommerceDeveloper{"CommerceDeveloper",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const char kRetailCouponsWithCodeParam[] = "RetailCouponsWithCodeParam";

// Params use for Discount Consent v2.
const base::Feature kDiscountConsentV2{"DiscountConsentV2",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

bool IsCouponDiscountPartnerMerchant(const GURL& url) {
  const std::string& url_string = url.spec();
  return RE2::PartialMatch(
      re2::StringPiece(url_string.data(), url_string.size()),
      GetPartnerMerchantPattern());
}

bool IsCouponWithCodeEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kRetailCoupons, kRetailCouponsWithCodeParam, false);
}

}  // namespace commerce
