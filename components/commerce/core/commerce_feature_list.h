// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_COMMERCE_FEATURE_LIST_H_
#define COMPONENTS_COMMERCE_CORE_COMMERCE_FEATURE_LIST_H_

#include "base/cxx17_backports.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "components/flags_ui/feature_entry.h"
#include "url/gurl.h"

namespace commerce {
extern const base::Feature kCommercePriceTracking;

// Price tracking variations for Android.
constexpr flags_ui::FeatureEntry::FeatureParam
    kCommercePriceTracking_PriceAlerts[] = {
        {"enable_price_tracking", "true"},
        {"price_tracking_with_optimization_guide", "false"}};

constexpr flags_ui::FeatureEntry::FeatureParam
    kCommercePriceTracking_PriceAlerts_WithOptimizationGuide[] = {
        {"enable_price_tracking", "true"},
        {"price_tracking_with_optimization_guide", "true"}};

constexpr flags_ui::FeatureEntry::FeatureParam
    kCommercePriceTracking_PriceNotifications[] = {
        {"enable_price_tracking", "true"},
        {"price_tracking_with_optimization_guide", "true"},
        {"enable_price_notification", "true"}};

constexpr flags_ui::FeatureEntry::FeatureVariation
    kCommercePriceTrackingAndroidVariations[] = {
        {"Price alerts", kCommercePriceTracking_PriceAlerts,
         base::size(kCommercePriceTracking_PriceAlerts), nullptr},
        {"Price alerts with OptimizationGuide",
         kCommercePriceTracking_PriceAlerts_WithOptimizationGuide,
         base::size(kCommercePriceTracking_PriceAlerts_WithOptimizationGuide),
         nullptr},
        {"Price notifications", kCommercePriceTracking_PriceNotifications,
         base::size(kCommercePriceTracking_PriceNotifications), nullptr},
};

// Price tracking variations for iOS.
constexpr flags_ui::FeatureEntry::FeatureParam
    kCommercePriceTrackingWithOptimizationGuide[] = {
        {"price_tracking_with_optimization_guide", "true"},
        {"price_tracking_opt_out", "false"}};

constexpr flags_ui::FeatureEntry::FeatureParam
    kCommercePriceTrackingWithOptimizationGuideAndOptOut[] = {
        {"price_tracking_with_optimization_guide", "true"},
        {"price_tracking_opt_out", "true"}};

constexpr flags_ui::FeatureEntry::FeatureVariation
    kCommercePriceTrackingVariations[] = {
        {"Price Tracking with Optimization Guide",
         kCommercePriceTrackingWithOptimizationGuide,
         base::size(kCommercePriceTrackingWithOptimizationGuide), nullptr},
        {"Price Tracking with Optimization Guide and Opt Out",
         kCommercePriceTrackingWithOptimizationGuideAndOptOut,
         base::size(kCommercePriceTrackingWithOptimizationGuideAndOptOut),
         nullptr}};

extern const base::Feature kCommerceMerchantViewer;
extern const base::FeatureParam<bool> kDeleteAllMerchantsOnClearBrowsingHistory;
extern const base::Feature kShoppingList;
extern const base::Feature kShoppingPDPMetrics;
extern const base::Feature kRetailCoupons;
extern const base::Feature kCommerceDeveloper;
// Parameter for enabling feature variation of coupons with code.
extern const char kRetailCouponsWithCodeParam[];

// Feature flag for Discount user consent v2.
extern const base::Feature kDiscountConsentV2;

// Interval that controls the frequency of showing coupons in infobar bubbles.
constexpr base::FeatureParam<base::TimeDelta> kCouponDisplayInterval{
    &commerce::kRetailCoupons, "coupon_display_interval", base::Hours(18)};

// Check if a URL belongs to a partner merchant of coupon discount.
bool IsCouponDiscountPartnerMerchant(const GURL& url);
// Check if the feature variation of coupons with code is enabled.
bool IsCouponWithCodeEnabled();
}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_COMMERCE_FEATURE_LIST_H_
