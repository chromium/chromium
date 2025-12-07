// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_COMMERCE_FEATURE_LIST_H_
#define COMPONENTS_COMMERCE_CORE_COMMERCE_FEATURE_LIST_H_

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/buildflag.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "components/webui/flags/feature_entry.h"
#include "url/gurl.h"

class PrefService;

namespace commerce {

BASE_DECLARE_FEATURE(kCommerceAllowLocalImages);
BASE_DECLARE_FEATURE(kCommerceAllowOnDemandBookmarkUpdates);
BASE_DECLARE_FEATURE(kCommerceLocalPDPDetection);
BASE_DECLARE_FEATURE(kCommerceMerchantViewer);
extern const base::FeatureParam<bool> kDeleteAllMerchantsOnClearBrowsingHistory;

BASE_DECLARE_FEATURE(kPriceAnnotations);

// Feature flag for Price Insights.
BASE_DECLARE_FEATURE(kPriceInsights);
extern const char kPriceInsightsDelayChipParam[];
extern const base::FeatureParam<bool> kPriceInsightsDelayChip;
extern const char kPriceInsightsChipLabelExpandOnHighPriceParam[];
extern const base::FeatureParam<bool> kPriceInsightsChipLabelExpandOnHighPrice;
extern const char kPriceInsightsShowFeedbackParam[];
extern const base::FeatureParam<bool> kPriceInsightsShowFeedback;
extern const char kPriceInsightsUseCacheParam[];
extern const base::FeatureParam<bool> kPriceInsightsUseCache;
BASE_DECLARE_FEATURE(kTabResumptionShopCard);
BASE_DECLARE_FEATURE(kShopCardImpressionLimits);

std::string ShopCardExperiment();

BASE_DECLARE_FEATURE(kProductSpecifications);
BASE_DECLARE_FEATURE(kProductSpecificationsClearMetadataOnNewlySupportedFields);
BASE_DECLARE_FEATURE(kProductSpecificationsCache);

BASE_DECLARE_FEATURE(kShoppingList);
BASE_DECLARE_FEATURE(kPriceTrackingSubscriptionServiceLocaleKey);
BASE_DECLARE_FEATURE(kPriceTrackingSubscriptionServiceProductVersion);
BASE_DECLARE_FEATURE(kShoppingPageTypes);
BASE_DECLARE_FEATURE(kShoppingPDPMetrics);
BASE_DECLARE_FEATURE(kSubscriptionsApi);
// Feature flag for showing discounts on checkout autofill.
BASE_DECLARE_FEATURE(kDiscountAutofill);

BASE_DECLARE_FEATURE(kShoppingAlternateServer);

// Feature flag for Discounts on navigation.
enum class DiscountDialogAutoPopupBehavior {
  // Only popup for the first time
  kAutoPopupOnce = 0,
  kAlwaysAutoPopup = 1,
  kNoAutoPopup = 2
};
BASE_DECLARE_FEATURE(kEnableDiscountInfoApi);
BASE_DECLARE_FEATURE(kDiscountDialogAutoPopupBehaviorSetting);
BASE_DECLARE_FEATURE(kDiscountDialogAutoPopupCounterfactual);
extern const char kHistoryClustersBehaviorParam[];
extern const base::FeatureParam<int> kHistoryClustersBehavior;
extern const char kMerchantWideBehaviorParam[];
extern const base::FeatureParam<int> kMerchantWideBehavior;
extern const char kNonMerchantWideBehaviorParam[];
extern const base::FeatureParam<int> kNonMerchantWideBehavior;
extern const char kDiscountOnShoppyPageParam[];
extern const base::FeatureParam<bool> kDiscountOnShoppyPage;

BASE_DECLARE_FEATURE(kDiscountsUiRefactor);

BASE_DECLARE_FEATURE(kRetailCoupons);
BASE_DECLARE_FEATURE(kCommerceDeveloper);
// Parameter for enabling feature variation of coupons with code.
extern const char kRetailCouponsWithCodeParam[];

// Shopping list update interval.
constexpr base::FeatureParam<base::TimeDelta>
    kShoppingListBookmarkpdateIntervalParam(
        &kShoppingList,
        "shopping-list-bookmark-update-interval",
        base::Hours(6));

// Shopping list revert page action icon on failure.
extern const char kRevertIconOnFailureParam[];
extern const base::FeatureParam<bool> kRevertIconOnFailure;

// Feature parameters for ChromeCart on Desktop.
constexpr base::FeatureParam<base::TimeDelta> kDiscountFetchDelayParam(
    &ntp_features::kNtpChromeCartModule,
    "discount-fetch-delay",
    base::Hours(6));

// Interval that controls the frequency of showing coupons in infobar bubbles.
constexpr base::FeatureParam<base::TimeDelta> kCouponDisplayInterval{
    &commerce::kRetailCoupons, "coupon_display_interval", base::Hours(18)};

// The heuristics of cart pages are from top 100 US shopping domains.
// https://colab.corp.google.com/drive/1fTGE_SQw_8OG4ubzQvWcBuyHEhlQ-pwQ?usp=sharing
constexpr base::FeatureParam<std::string> kCartPattern{
    &ntp_features::kNtpChromeCartModule, "cart-pattern",
    // clang-format off
    "(^https?://cart\\.)"
    "|"
    "(/("
      "(((my|co|shopping|view)[-_]?)?(cart|bag)(view|display)?)"
      "|"
      "(checkout/([^/]+/)?(basket|bag))"
      "|"
      "(checkoutcart(display)?view)"
      "|"
      "(bundles/shop)"
      "|"
      "((ajax)?orderitemdisplay(view)?)"
      "|"
      "(cart-show)"
    ")(/|\\.|$))"
    // clang-format on
};

constexpr base::FeatureParam<std::string> kCartPatternMapping{
    &ntp_features::kNtpChromeCartModule, "cart-pattern-mapping",
    // Empty JSON string.
    ""};

constexpr base::FeatureParam<std::string> kCheckoutPattern{
    &ntp_features::kNtpChromeCartModule, "checkout-pattern",
    // clang-format off
    "/("
    "("
      "("
        "(begin|billing|cart|payment|start|review|final|order|secure|new)"
        "[-_]?"
      ")?"
      "(checkout|chkout)(s)?"
      "([-_]?(begin|billing|cart|payment|start|review))?"
    ")"
    "|"
    "(\\w+(checkout|chkout)(s)?)"
    ")(#|/|\\.|$|\\?)"
    // clang-format on
};

constexpr base::FeatureParam<std::string> kCheckoutPatternMapping{
    &ntp_features::kNtpChromeCartModule, "checkout-pattern-mapping",
    // Empty JSON string.
    ""};

inline constexpr base::FeatureParam<std::string> kShopCardVariation{
    &kTabResumptionShopCard, "ShopCardVariant", ""};
inline constexpr base::FeatureParam<std::string> kShopCardPosition{
    &kTabResumptionShopCard, "ShopCardPosition", ""};

extern const char kShopCardArm1[];
extern const char kShopCardArm3[];
extern const char kShopCardArm4[];
extern const char kShopCardArm5[];
extern const char kShopCardArm6[];
extern const char kShopCardFrontPosition[];
extern const char kShopCardMaxImpressions[];

// Feature params for product specifications.
extern const char kProductSpecificationsSetValidForClusteringTimeParam[];
extern const base::FeatureParam<base::TimeDelta>
    kProductSpecificationsSetValidForClusteringTime;
extern const char kProductSpecificationsUseServerClusteringParam[];
extern const base::FeatureParam<bool> kProductSpecificationsUseServerClustering;
extern const char kProductSpecificationsEnableQualityLoggingParam[];
extern const base::FeatureParam<bool>
    kProductSpecificationsEnableQualityLogging;

// Check if a URL belongs to a partner merchant of any type of discount.
bool IsPartnerMerchant(const GURL& url);
// Check if a URL belongs to a partner merchant of rule discount.
bool IsRuleDiscountPartnerMerchant(const GURL& url);
// Check if a URL belongs to a partner merchant of coupon discount.
bool IsCouponDiscountPartnerMerchant(const GURL& url);
// Check if cart discount feature is enabled.
bool IsCartDiscountFeatureEnabled();
// Check if the feature variation of coupons with code is enabled.
bool IsCouponWithCodeEnabled();
// Check if the variation with fake data is enabled.
bool IsFakeDataEnabled();
// Check if the contextual consent for discount is enabled.
bool isContextualConsentEnabled();
// Check if the shopping list feature is allowed for enterprise.
bool IsShoppingListAllowedForEnterprise(PrefService* prefs);

// Check if commerce features are allowed to run for the specified country
// and locale.
bool IsEnabledForCountryAndLocale(const base::Feature& feature,
                                  std::string country,
                                  std::string locale);

// A feature check for the specified |feature|, which will return true if the
// user has the feature flag enabled or (if applicable) is in an enabled
// country and locale.
bool IsRegionLockedFeatureEnabled(const base::Feature& feature,
                                  const std::string& country_code,
                                  const std::string& locale);

#if !BUILDFLAG(IS_ANDROID)
// Get the time delay between discount fetches.
base::TimeDelta GetDiscountFetchDelay();
// Check if a URL belongs to a merchant with no discounts.
bool IsNoDiscountMerchant(const GURL& url);
#endif
}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_COMMERCE_FEATURE_LIST_H_
