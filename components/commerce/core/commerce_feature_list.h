// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_COMMERCE_FEATURE_LIST_H_
#define COMPONENTS_COMMERCE_CORE_COMMERCE_FEATURE_LIST_H_

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "components/flags_ui/feature_entry.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "url/gurl.h"

class PrefService;

namespace variations {
class VariationsService;
}  // namespace variations

namespace commerce {

namespace switches {
extern const char kEnableChromeCart[];
}

BASE_DECLARE_FEATURE(kCommercePriceTracking);

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
         std::size(kCommercePriceTracking_PriceAlerts), nullptr},
        {"Price alerts with OptimizationGuide",
         kCommercePriceTracking_PriceAlerts_WithOptimizationGuide,
         std::size(kCommercePriceTracking_PriceAlerts_WithOptimizationGuide),
         nullptr},
        {"Price notifications", kCommercePriceTracking_PriceNotifications,
         std::size(kCommercePriceTracking_PriceNotifications), nullptr},
};

// Price tracking variations for iOS.
constexpr flags_ui::FeatureEntry::FeatureParam
    kCommercePriceTrackingNotifications[] = {
        {"enable_price_notification", "true"}};

constexpr flags_ui::FeatureEntry::FeatureVariation
    kCommercePriceTrackingVariations[] = {
        {"Price Tracking Notifications", kCommercePriceTrackingNotifications,
         std::size(kCommercePriceTrackingNotifications), nullptr}};

BASE_DECLARE_FEATURE(kCommerceAllowLocalImages);
BASE_DECLARE_FEATURE(kCommerceAllowOnDemandBookmarkUpdates);
BASE_DECLARE_FEATURE(kCommerceAllowServerImages);
BASE_DECLARE_FEATURE(kCommerceCoupons);
BASE_DECLARE_FEATURE(kCommerceMerchantViewer);
BASE_DECLARE_FEATURE(kCommerceMerchantViewerRegionLaunched);
extern const base::FeatureParam<bool> kDeleteAllMerchantsOnClearBrowsingHistory;
BASE_DECLARE_FEATURE(kShoppingList);
BASE_DECLARE_FEATURE(kShoppingListRegionLaunched);
BASE_DECLARE_FEATURE(kShoppingPDPMetrics);
BASE_DECLARE_FEATURE(kShoppingPDPMetricsRegionLaunched);
BASE_DECLARE_FEATURE(kRetailCoupons);
BASE_DECLARE_FEATURE(kCommerceDeveloper);
// Parameter for enabling feature variation of coupons with code.
extern const char kRetailCouponsWithCodeParam[];

// Feature flag for Discount user consent v2.
BASE_DECLARE_FEATURE(kDiscountConsentV2);

// Feature flag for exposing commerce hint on Android.
BASE_DECLARE_FEATURE(kCommerceHintAndroid);

// Feature flag for Merchant Wide promotion.
BASE_DECLARE_FEATURE(kMerchantWidePromotion);

// Feature flag for Code-based RBD.
BASE_DECLARE_FEATURE(kCodeBasedRBD);

// Feature flag for DOM-based heuristics for ChromeCart.
BASE_DECLARE_FEATURE(kChromeCartDomBasedHeuristics);

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

// Whether to use OptimizationGuide to optimize renderer signal collection.
constexpr base::FeatureParam<bool> kOptimizeRendererSignal(
#if !BUILDFLAG(IS_ANDROID)
    &ntp_features::kNtpChromeCartModule,
#else
    &kCommerceHintAndroid,
#endif
    "optimize-renderer-signal",
    true);

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
#if !BUILDFLAG(IS_ANDROID)
  &ntp_features::kNtpChromeCartModule,
#else
  &kCommerceHintAndroid,
#endif
      "cart-pattern",
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
#if !BUILDFLAG(IS_ANDROID)
  &ntp_features::kNtpChromeCartModule,
#else
  &kCommerceHintAndroid,
#endif
      "cart-pattern-mapping",
      // Empty JSON string.
      ""
};

constexpr base::FeatureParam<std::string> kCheckoutPattern{
#if !BUILDFLAG(IS_ANDROID)
  &ntp_features::kNtpChromeCartModule,
#else
  &kCommerceHintAndroid,
#endif
      "checkout-pattern",
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
#if !BUILDFLAG(IS_ANDROID)
  &ntp_features::kNtpChromeCartModule,
#else
  &kCommerceHintAndroid,
#endif
      "checkout-pattern-mapping",
      // Empty JSON string.
      ""
};

// The following are Feature parameters for DOM-based heuristics for ChromeCart.
constexpr base::FeatureParam<std::string> kAddToCartButtonTextPattern{
    &kChromeCartDomBasedHeuristics, "add-to-cart-text-pattern",
    "(add(ed|ing)?( \\w+)* (to (shopping )?(cart|bag|basket))|(for "
    "shipping))|(^add$)|(buy now)"};

constexpr base::FeatureParam<std::string> kAddToCartButtonTagPattern{
    &kChromeCartDomBasedHeuristics, "add-to-cart-tag-pattern",
    "BUTTON, INPUT, A, SPAN"};

constexpr base::FeatureParam<int> kAddToCartButtonWidthLimit{
    &kChromeCartDomBasedHeuristics, "add-to-cart-button-width", 700};

constexpr base::FeatureParam<int> kAddToCartButtonHeightLimit{
    &kChromeCartDomBasedHeuristics, "add-to-cart-button-height", 100};

constexpr base::FeatureParam<base::TimeDelta> kAddToCartButtonActiveTime{
    &kChromeCartDomBasedHeuristics, "add-to-cart-button-active-time",
    base::Seconds(5)};

constexpr base::FeatureParam<bool> kAddToCartProductImage{
    &kChromeCartDomBasedHeuristics, "add-to-cart-product-image", true};

constexpr base::FeatureParam<std::string> kSkipHeuristicsDomainPattern{
    &kChromeCartDomBasedHeuristics, "skip-heuristics-domain-pattern",
    // This regex does not match anything.
    "\\b\\B"};

// The following are Feature params for Discount user consent v2.
// This indicates the Discount Consent v2 variation on the NTP Cart module.
enum class DiscountConsentNtpVariation {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  kDefault = 0,
  kStringChange = 1,
  kInline = 2,
  kDialog = 3,
  kNativeDialog = 4,
  kMaxValue = kNativeDialog
};

// Param indicates the ConsentV2 variation. See DiscountConsentNtpVariation
// enum.
extern const char kNtpChromeCartModuleDiscountConsentNtpVariationParam[];
extern const base::FeatureParam<int>
    kNtpChromeCartModuleDiscountConsentNtpVariation;
// The time interval, after the last dismissal, before reshowing the consent.
extern const char kNtpChromeCartModuleDiscountConsentReshowTimeParam[];
extern const base::FeatureParam<base::TimeDelta>
    kNtpChromeCartModuleDiscountConsentReshowTime;
// The max number of dismisses allowed.
extern const char kNtpChromeCartModuleDiscountConsentMaxDismissalCountParam[];
extern const base::FeatureParam<int>
    kNtpChromeCartModuleDiscountConsentMaxDismissalCount;

// String change variation params. This string is replacing the content string
// of the v1 consent.
extern const char kNtpChromeCartModuleDiscountConsentStringChangeContentParam[];
extern const base::FeatureParam<std::string>
    kNtpChromeCartModuleDiscountConsentStringChangeContent;

// DiscountConsentNtpVariation::kInline and DiscountConsentNtpVariation::kDialog
// params. This indicate whether the 'x' button should show.
extern const char
    kNtpChromeCartModuleDiscountConsentInlineShowCloseButtonParam[];
extern const base::FeatureParam<bool>
    kNtpChromeCartModuleDiscountConsentInlineShowCloseButton;

// The following are discount consent step 1 params.
// This indicates whether the content in step 1 is a static string that does not
// contain any merchant names.
extern const char
    kNtpChromeCartModuleDiscountConsentNtpStepOneUseStaticContentParam[];
extern const base::FeatureParam<bool>
    kNtpChromeCartModuleDiscountConsentNtpStepOneUseStaticContent;
// This the content string use in step 1 if
// kNtpChromeCartModuleDiscountConsentNtpStepOneUseStaticContent.Get() is true.
extern const char
    kNtpChromeCartModuleDiscountConsentNtpStepOneStaticContentParam[];
extern const base::FeatureParam<std::string>
    kNtpChromeCartModuleDiscountConsentNtpStepOneStaticContent;
// This is a string template that takes in one merchant name, and it's used when
// there is only 1 Chrome Cart.
extern const char
    kNtpChromeCartModuleDiscountConsentNtpStepOneContentOneCartParam[];
extern const base::FeatureParam<std::string>
    kNtpChromeCartModuleDiscountConsentNtpStepOneContentOneCart;
// This is a string template that takes in two merchant names, and it's used
// when there are only 2 Chrome Carts.
extern const char
    kNtpChromeCartModuleDiscountConsentNtpStepOneContentTwoCartsParam[];
extern const base::FeatureParam<std::string>
    kNtpChromeCartModuleDiscountConsentNtpStepOneContentTwoCarts;
// This is a string template that takes in two merchant names, and it's used
// when there are 3 or more Chrome Carts.
extern const char
    kNtpChromeCartModuleDiscountConsentNtpStepOneContentThreeCartsParam[];
extern const base::FeatureParam<std::string>
    kNtpChromeCartModuleDiscountConsentNtpStepOneContentThreeCarts;

// The following are discount consent step 2 params.
// This is the content string used in step 2. This is the actual consent string.
extern const char kNtpChromeCartModuleDiscountConsentNtpStepTwoContentParam[];
extern const base::FeatureParam<std::string>
    kNtpChromeCartModuleDiscountConsentNtpStepTwoContent;
// This is used to indicate whether the backgound-color of step 2 should change.
extern const char
    kNtpChromeCartModuleDiscountConsentInlineStepTwoDifferentColorParam[];
extern const base::FeatureParam<bool>
    kNtpChromeCartModuleDiscountConsentInlineStepTwoDifferentColor;
// This is the content title use in the dialog consent.
extern const char
    kNtpChromeCartModuleDiscountConsentNtpDialogContentTitleParam[];
extern const base::FeatureParam<std::string>
    kNtpChromeCartModuleDiscountConsentNtpDialogContentTitle;
// Feature params for showing the contextual discount consent on the cart and
// checkout page.
extern const char kContextualConsentShowOnCartAndCheckoutPageParam[];
extern const base::FeatureParam<bool>
    kContextualConsentShowOnCartAndCheckoutPage;
// Feature params for showing the contextual discount consent on the search
// result page.
extern const char kContextualConsentShowOnSRPParam[];
extern const base::FeatureParam<bool> kContextualConsentShowOnSRP;

// Feature params for enabling the cart heuristics improvement on Android.
extern const char kCommerceHintAndroidHeuristicsImprovementParam[];

// Feature params for merchant wide promotion.
extern const char kReadyToFetchMerchantWidePromotionParam[];
extern const base::FeatureParam<bool> kReadyToFetchMerchantWidePromotion;

// Feature params for code-based Rule-based Discount (RBD).
extern const char kCodeBasedRuleDiscountParam[];
extern const base::FeatureParam<bool> kCodeBasedRuleDiscount;

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

// Get the user's current country code. If access through variations fails,
// the country_codes component is used.
std::string GetCurrentCountryCode(variations::VariationsService* variations);

// Check if commerce features are allowed to run for the specified country
// and locale.
bool IsEnabledForCountryAndLocale(const base::Feature& feature,
                                  std::string country,
                                  std::string locale);

// A feature check for the specified |feature|, which will return true if the
// user has the feature flag enabled or (if applicable) is in an enabled
// country and locale.
bool IsRegionLockedFeatureEnabled(const base::Feature& feature,
                                  const base::Feature& feature_region_launched,
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
