// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/commerce_feature_list.h"

#include <unordered_map>
#include <unordered_set>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "build/buildflag.h"
#if !BUILDFLAG(IS_ANDROID)
#include "components/commerce/core/commerce_heuristics_data.h"
#endif  // !BUILDFLAG(IS_ANDROID)
#include "components/commerce/core/commerce_heuristics_data_metrics_helper.h"
#include "components/commerce/core/pref_names.h"
#include "third_party/re2/src/re2/re2.h"

namespace commerce {

namespace {

typedef std::unordered_map<
    const base::Feature*,
    std::unordered_map<std::string, std::unordered_set<std::string>>>
    CountryLocaleMap;

// Get a map of enabled countries to the set of allowed locales for that
// country on a per-feature basis. Just because a locale is enabled for one
// country doesn't mean it can or should be enabled in others. The checks using
// this map should convert all countries and locales to lower case as they may
// differ depending on the API used to access them.
const CountryLocaleMap& GetAllowedCountryToLocaleMap() {
  // Declaring the variable "static" means it isn't recreated each time this
  // function is called. This gets around the "static initializers" problem.
  static const base::NoDestructor<CountryLocaleMap> allowed_map([] {
    CountryLocaleMap map;

    map[&kCommerceMerchantViewerRegionLaunched] = {{"us", {"en-us"}}};
    map[&kEnableDiscountInfoApiRegionLaunched] = {{"us", {"en-us"}}};
    map[&ntp_features::kNtpChromeCartModule] = {{"us", {"en-us"}}};
    map[&kPriceAnnotationsRegionLaunched] = {{"us", {"en-us"}}};
    map[&kPriceInsightsRegionLaunched] = {{"us", {"en-us"}}};
    map[&kProductSpecifications] = {{"us", {"en-us"}}};
    map[&kShoppingListRegionLaunched] = {{"us", {"en-us"}}};
    map[&kShoppingPageTypesRegionLaunched] = {{"us", {"en-us"}}};
    map[&kShoppingPDPMetricsRegionLaunched] = {{"us", {"en-us"}}};
    map[&kSubscriptionsApiRegionLaunched] = {
        {"us", {"en", "en-gb", "en-us"}},
        {"au", {"en", "en-au", "en-gb", "en-us"}},
        {"ca", {"en", "en-ca", "en-gb", "en-us"}},
        {"in", {"en", "en-gb", "en-in", "en-us"}},
        {"jp", {"ja", "ja-jp"}}};
    map[&kDiscountAutofillRegionLaunched] = {{"us", {"en-us"}}};

    return map;
  }());
  return *allowed_map;
}

constexpr base::FeatureParam<std::string> kRulePartnerMerchantPattern{
    &ntp_features::kNtpChromeCartModule, "partner-merchant-pattern",
    // This regex does not match anything.
    "\\b\\B"};

constexpr base::FeatureParam<std::string> kCouponPartnerMerchantPattern{
    &commerce::kRetailCoupons, "coupon-partner-merchant-pattern",
    // This regex does not match anything.
    "\\b\\B"};

const re2::RE2& GetRulePartnerMerchantPattern() {
#if !BUILDFLAG(IS_ANDROID)
  auto* pattern_from_component =
      commerce_heuristics::CommerceHeuristicsData::GetInstance()
          .GetRuleDiscountPartnerMerchantPattern();
  if (pattern_from_component && kRulePartnerMerchantPattern.Get() ==
                                    kRulePartnerMerchantPattern.default_value) {
    CommerceHeuristicsDataMetricsHelper::RecordPartnerMerchantPatternSource(
        CommerceHeuristicsDataMetricsHelper::HeuristicsSource::FROM_COMPONENT);
    return *pattern_from_component;
  }
#endif  // !BUILDFLAG(IS_ANDROID)
  re2::RE2::Options options;
  options.set_case_sensitive(false);
  static base::NoDestructor<re2::RE2> instance(
      kRulePartnerMerchantPattern.Get(), options);
  CommerceHeuristicsDataMetricsHelper::RecordPartnerMerchantPatternSource(
      CommerceHeuristicsDataMetricsHelper::HeuristicsSource::
          FROM_FEATURE_PARAMETER);
  return *instance;
}

const re2::RE2& GetCouponPartnerMerchantPattern() {
#if !BUILDFLAG(IS_ANDROID)
  auto* pattern_from_component =
      commerce_heuristics::CommerceHeuristicsData::GetInstance()
          .GetCouponDiscountPartnerMerchantPattern();
  if (pattern_from_component &&
      kCouponPartnerMerchantPattern.Get() ==
          kCouponPartnerMerchantPattern.default_value) {
    return *pattern_from_component;
  }
#endif  // !BUILDFLAG(IS_ANDROID)
  re2::RE2::Options options;
  options.set_case_sensitive(false);
  static base::NoDestructor<re2::RE2> instance(
      kCouponPartnerMerchantPattern.Get(), options);
  return *instance;
}

}  // namespace

BASE_FEATURE(kCommerceAllowLocalImages,
             "CommerceAllowLocalImages",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCommerceAllowOnDemandBookmarkUpdates,
             "CommerceAllowOnDemandBookmarkUpdates",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCommerceMerchantViewer,
             "CommerceMerchantViewer",
             base::FEATURE_DISABLED_BY_DEFAULT);
#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kCommerceMerchantViewerRegionLaunched,
             "CommerceMerchantViewerRegionLaunched",
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
BASE_FEATURE(kCommerceMerchantViewerRegionLaunched,
             "CommerceMerchantViewerRegionLaunched",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kCommerceLocalPDPDetection,
             "CommerceLocalPDPDetection",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPriceAnnotations,
             "PriceAnnotations",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kPriceAnnotationsRegionLaunched,
             "PriceAnnotationsRegionLaunched",
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
BASE_FEATURE(kPriceAnnotationsRegionLaunched,
             "PriceAnnotationsRegionLaunched",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kPriceInsights,
             "PriceInsights",
             base::FEATURE_DISABLED_BY_DEFAULT);
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kPriceInsightsRegionLaunched,
             "PriceInsightsRegionLaunched",
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
BASE_FEATURE(kPriceInsightsRegionLaunched,
             "PriceInsightsRegionLaunched",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif
const char kPriceInsightsDelayChipParam[] = "price-inishgts-delay-chip";
const base::FeatureParam<bool> kPriceInsightsDelayChip{
    &commerce::kPriceInsights, kPriceInsightsDelayChipParam, false};
const char kPriceInsightsChipLabelExpandOnHighPriceParam[] =
    "chip-expand-on-high-price";
const base::FeatureParam<bool> kPriceInsightsChipLabelExpandOnHighPrice{
    &commerce::kPriceInsights, kPriceInsightsChipLabelExpandOnHighPriceParam,
    false};
const char kPriceInsightsShowFeedbackParam[] = "price-insights-show-feedback";
const base::FeatureParam<bool> kPriceInsightsShowFeedback{
    &commerce::kPriceInsights, kPriceInsightsShowFeedbackParam, true};
const char kPriceInsightsUseCacheParam[] = "price-insights-use-cache";
const base::FeatureParam<bool> kPriceInsightsUseCache{
    &commerce::kPriceInsights, kPriceInsightsUseCacheParam, true};

// Discount Autofill at Checkout
BASE_FEATURE(kDiscountAutofill,
             "DiscountAutofill",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kDiscountAutofillRegionLaunched,
             "DiscountAutofillRegionLaunched",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Promotion in Magic Stack for Price Tracking users from other platforms.
BASE_FEATURE(kPriceTrackingPromo,
             "PriceTrackingPromo",
             base::FEATURE_ENABLED_BY_DEFAULT);

// ShopCard in Magic Stack, including shopping features like price drop,
// reviews, etc.
BASE_FEATURE(kShopCard, "ShopCard", base::FEATURE_DISABLED_BY_DEFAULT);

// Impression limits on ShopCards
BASE_FEATURE(kShopCardImpressionLimits,
             "ShopCardImpressionLimits",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kProductSpecifications,
             "ProductSpecifications",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Kill switch for unsupported fields becoming supported in the event of a
// browser upgrade.
BASE_FEATURE(kProductSpecificationsClearMetadataOnNewlySupportedFields,
             "ProductSpecificationsClearMetadataOnNewlySupportedFields",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCompareConfirmationToast,
             "CompareConfirmationToast",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kProductSpecificationsCache,
             "ProductSpecificationsCache",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Discount on navigation
BASE_FEATURE(kEnableDiscountInfoApi,
             "EnableDiscountInfoApi",
             base::FEATURE_DISABLED_BY_DEFAULT);

// TODO(crbug.com/406555154): Clean up this flag when discount on clank launched.
const char kDiscountOnShoppyPageParam[] = "discount-on-shoppy-page";

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
const base::FeatureParam<bool> kDiscountOnShoppyPage{
    &kEnableDiscountInfoApi, kDiscountOnShoppyPageParam, true};
#else
const base::FeatureParam<bool> kDiscountOnShoppyPage{
    &kEnableDiscountInfoApi, kDiscountOnShoppyPageParam, false};
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kEnableDiscountInfoApiRegionLaunched,
             "EnableDiscountInfoApiRegionLaunched",
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
BASE_FEATURE(kEnableDiscountInfoApiRegionLaunched,
             "EnableDiscountInfoApiRegionLaunched",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

const char kHistoryClustersBehaviorParam[] = "history-cluster-behavior";
const char kMerchantWideBehaviorParam[] = "merchant-wide-behavior";
const char kNonMerchantWideBehaviorParam[] = "non-merchant-wide-behavior";

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kDiscountDialogAutoPopupBehaviorSetting,
             "DiscountDialogAutoPopupBehaviorSetting",
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<int> kHistoryClustersBehavior{
    &commerce::kDiscountDialogAutoPopupBehaviorSetting,
    kHistoryClustersBehaviorParam, 1};
const base::FeatureParam<int> kMerchantWideBehavior{
    &commerce::kDiscountDialogAutoPopupBehaviorSetting,
    kMerchantWideBehaviorParam, 2};
const base::FeatureParam<int> kNonMerchantWideBehavior{
    &commerce::kDiscountDialogAutoPopupBehaviorSetting,
    kNonMerchantWideBehaviorParam, 0};
#else
BASE_FEATURE(kDiscountDialogAutoPopupBehaviorSetting,
             "DiscountDialogAutoPopupBehaviorSetting",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<int> kHistoryClustersBehavior{
    &commerce::kDiscountDialogAutoPopupBehaviorSetting,
    kHistoryClustersBehaviorParam, 0};
const base::FeatureParam<int> kMerchantWideBehavior{
    &commerce::kDiscountDialogAutoPopupBehaviorSetting,
    kMerchantWideBehaviorParam, 2};
const base::FeatureParam<int> kNonMerchantWideBehavior{
    &commerce::kDiscountDialogAutoPopupBehaviorSetting,
    kNonMerchantWideBehaviorParam, 2};
#endif

BASE_FEATURE(kDiscountDialogAutoPopupCounterfactual,
             "DiscountDialogAutoPopupCounterfactual",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDiscountsUiRefactor,
             "DiscountsUiRefactor",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<bool> kDeleteAllMerchantsOnClearBrowsingHistory{
    &kCommerceMerchantViewer, "delete_all_merchants_on_clear_history", false};

BASE_FEATURE(kShoppingList, "ShoppingList", base::FEATURE_DISABLED_BY_DEFAULT);
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_IOS)
BASE_FEATURE(kShoppingListRegionLaunched,
             "ShoppingListRegionLaunched",
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
BASE_FEATURE(kShoppingListRegionLaunched,
             "ShoppingListRegionLaunched",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kPriceTrackingSubscriptionServiceLocaleKey,
             "PriceTrackingSubscriptionServiceLocaleKey",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPriceTrackingSubscriptionServiceProductVersion,
             "PriceTrackingSubscriptionServiceProductVersion",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kShoppingPDPMetrics,
             "ShoppingPDPMetrics",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kShoppingPDPMetricsRegionLaunched,
             "ShoppingPDPMetricsRegionLaunched",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_DECLARE_FEATURE(kSubscriptionsApi);
BASE_DECLARE_FEATURE(kSubscriptionsApiRegionLaunched);

BASE_FEATURE(kSubscriptionsApi,
             "SubscriptionsApi",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSubscriptionsApiRegionLaunched,
             "SubscriptionsApiRegionLaunched",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTrackByDefaultOnMobile,
             "TrackByDefaultOnMobile",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_IOS)
BASE_FEATURE(kPriceInsightsIos,
             "PriceInsightsIos",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPriceInsightsHighPriceIos,
             "PriceInsightsHighPrice",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kShoppingPageTypes,
             "ShoppingPageTypes",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kShoppingPageTypesRegionLaunched,
             "ShoppingPageTypesRegionLaunched",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kRetailCoupons, "RetailCoupons", base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCommerceDeveloper,
             "CommerceDeveloper",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kRetailCouponsWithCodeParam[] = "RetailCouponsWithCodeParam";

// Params use for Discount Consent v2.
BASE_FEATURE(kDiscountConsentV2,
             "DiscountConsentV2",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCodeBasedRBD, "CodeBasedRBD", base::FEATURE_ENABLED_BY_DEFAULT);

extern const char kShopCardArm1[] = "arm_1";
extern const char kShopCardArm2[] = "arm_2";
extern const char kShopCardArm3[] = "arm_3";
extern const char kShopCardArm4[] = "arm_4";
// Regular Tab Resumption with same impression limits as ShopCard
// (max 3 impressions). So ShopCard variations of Tab Resumption can
// be conclusively benchmarked against regular Tab Resumption.
extern const char kShopCardArm5[] = "arm_5";
extern const char kShopCardFrontPosition[] = "shop_card_front";
extern const char kShopCardMaxImpressions[] = "max_impressions";

const char kProductSpecificationsSetValidForClusteringTimeParam[] =
    "set-valid-for-clustering-time";
const base::FeatureParam<base::TimeDelta>
    kProductSpecificationsSetValidForClusteringTime{
        &commerce::kProductSpecifications,
        kProductSpecificationsSetValidForClusteringTimeParam, base::Days(14)};
const char kProductSpecificationsUseServerClusteringParam[] =
    "use-server-clustering";
const base::FeatureParam<bool> kProductSpecificationsUseServerClustering{
    &commerce::kProductSpecifications,
    kProductSpecificationsUseServerClusteringParam, true};
const char kProductSpecificationsEnableQualityLoggingParam[] =
    "enable-quality-logging";
const base::FeatureParam<bool> kProductSpecificationsEnableQualityLogging{
    &commerce::kProductSpecifications,
    kProductSpecificationsEnableQualityLoggingParam, true};

const char kRevertIconOnFailureParam[] =
    "shopping-list-revert-page-action-icon-on-failure";
const base::FeatureParam<bool> kRevertIconOnFailure{
    &kShoppingList, kRevertIconOnFailureParam, false};

bool IsPartnerMerchant(const GURL& url) {
  return commerce::IsCouponDiscountPartnerMerchant(url) ||
         IsRuleDiscountPartnerMerchant(url);
}

bool IsRuleDiscountPartnerMerchant(const GURL& url) {
  return RE2::PartialMatch(url.spec(), GetRulePartnerMerchantPattern());
}

bool IsCouponDiscountPartnerMerchant(const GURL& url) {
  return RE2::PartialMatch(url.spec(), GetCouponPartnerMerchantPattern());
}

bool IsCartDiscountFeatureEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(
      ntp_features::kNtpChromeCartModule,
      ntp_features::kNtpChromeCartModuleAbandonedCartDiscountParam, false);
}

bool IsCouponWithCodeEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kRetailCoupons, kRetailCouponsWithCodeParam, true);
}

bool IsFakeDataEnabled() {
  return base::GetFieldTrialParamValueByFeature(
             ntp_features::kNtpChromeCartModule,
             ntp_features::kNtpChromeCartModuleDataParam) == "fake";
}

bool IsShoppingListAllowedForEnterprise(PrefService* prefs) {
  return prefs->GetBoolean(kShoppingListEnabledPrefName) ||
         !prefs->IsManagedPreference(kShoppingListEnabledPrefName);
}

bool IsEnabledForCountryAndLocale(const base::Feature& feature,
                                  std::string country,
                                  std::string locale) {
  const CountryLocaleMap& allowedCountryLocales =
      GetAllowedCountryToLocaleMap();

  // First make sure the feature is in the map.
  auto feature_it = allowedCountryLocales.find(&feature);
  if (feature_it == allowedCountryLocales.end()) {
    return false;
  }

  auto it = feature_it->second.find(base::ToLowerASCII(country));

  // If the country isn't in the map, it's not valid.
  if (it == feature_it->second.end()) {
    return false;
  }

  // If the set of allowed locales contains our locale, we're considered to be
  // enabled.
  return it->second.find(base::ToLowerASCII(locale)) != it->second.end();
}

bool IsRegionLockedFeatureEnabled(const base::Feature& feature,
                                  const base::Feature& feature_region_launched,
                                  const std::string& country_code,
                                  const std::string& locale) {
  bool flag_enabled = base::FeatureList::IsEnabled(feature);
  bool region_launched =
      base::FeatureList::IsEnabled(feature_region_launched) &&
      IsEnabledForCountryAndLocale(feature_region_launched, country_code,
                                   locale);
  return flag_enabled || region_launched;
}

#if !BUILDFLAG(IS_ANDROID)
base::TimeDelta GetDiscountFetchDelay() {
  auto delay_from_component =
      commerce_heuristics::CommerceHeuristicsData::GetInstance()
          .GetDiscountFetchDelay();
  if (delay_from_component.has_value() &&
      kDiscountFetchDelayParam.Get() ==
          kDiscountFetchDelayParam.default_value) {
    return *delay_from_component;
  }
  return kDiscountFetchDelayParam.Get();
}

bool IsNoDiscountMerchant(const GURL& url) {
  auto* pattern_from_component =
      commerce_heuristics::CommerceHeuristicsData::GetInstance()
          .GetNoDiscountMerchantPattern();
  // If pattern from component updater is not available, merchants are
  // considered to have no discounts by default.
  if (!pattern_from_component) {
    return true;
  }
  return RE2::PartialMatch(url.host_piece(), *pattern_from_component);
}
#endif
}  // namespace commerce
