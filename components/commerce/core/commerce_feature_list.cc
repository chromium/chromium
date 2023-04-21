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
#include "components/country_codes/country_codes.h"
#include "components/variations/service/variations_service.h"
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

    map[&kShoppingListRegionLaunched] = {{"us", {"en-us"}}};
    map[&kShoppingPDPMetricsRegionLaunched] = {{"us", {"en-us"}}};
    map[&ntp_features::kNtpChromeCartModule] = {{"us", {"en-us"}}};
    map[&kCommerceMerchantViewerRegionLaunched] = {{"us", {"en-us"}}};
    map[&kCommercePriceTrackingRegionLaunched] = {{"us", {"en-us"}}};

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

namespace switches {
// Specifies whether ChromeCart is enabled.
const char kEnableChromeCart[] = "enable-chrome-cart";
}  // namespace switches

BASE_FEATURE(kCommerceAllowLocalImages,
             "CommerceAllowLocalImages",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCommerceAllowOnDemandBookmarkUpdates,
             "CommerceAllowOnDemandBookmarkUpdates",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCommerceAllowOnDemandBookmarkBatchUpdates,
             "CommerceAllowOnDemandBookmarkBatchUpdates",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCommerceAllowServerImages,
             "CommerceAllowServerImages",
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

BASE_FEATURE(kCommercePriceTracking,
             "CommercePriceTracking",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCommercePriceTrackingChipExperiment,
             "CommercePriceTrackingChipExperiment",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kCommercePriceTrackingRegionLaunched,
             "CommercePriceTrackingRegionLaunched",
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
BASE_FEATURE(kCommercePriceTrackingRegionLaunched,
             "CommercePriceTrackingRegionLaunched",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

const base::FeatureParam<bool> kDeleteAllMerchantsOnClearBrowsingHistory{
    &kCommerceMerchantViewer, "delete_all_merchants_on_clear_history", false};

BASE_FEATURE(kShoppingList, "ShoppingList", base::FEATURE_DISABLED_BY_DEFAULT);
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kShoppingListRegionLaunched,
             "ShoppingListRegionLaunched",
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
BASE_FEATURE(kShoppingListRegionLaunched,
             "ShoppingListRegionLaunched",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kShoppingPDPMetrics,
             "ShoppingPDPMetrics",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kShoppingPDPMetricsRegionLaunched,
             "ShoppingPDPMetricsRegionLaunched",
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

BASE_FEATURE(kCommerceHintAndroid,
             "CommerceHintAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMerchantWidePromotion,
             "MerchantWidePromotion",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCodeBasedRBD, "CodeBasedRBD", base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kChromeCartDomBasedHeuristics,
             "ChromeCartDomBasedHeuristics",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Params for Discount Consent V2 in the NTP Cart module.
const char kNtpChromeCartModuleDiscountConsentNtpVariationParam[] =
    "discount-consent-ntp-variation";
const base::FeatureParam<int> kNtpChromeCartModuleDiscountConsentNtpVariation{
    &commerce::kDiscountConsentV2,
    kNtpChromeCartModuleDiscountConsentNtpVariationParam, 4};
const char kNtpChromeCartModuleDiscountConsentReshowTimeParam[] =
    "discount-consent-ntp-reshow-time";
const base::FeatureParam<base::TimeDelta>
    kNtpChromeCartModuleDiscountConsentReshowTime{
        &commerce::kDiscountConsentV2,
        kNtpChromeCartModuleDiscountConsentReshowTimeParam, base::Days(28)};
const char kNtpChromeCartModuleDiscountConsentMaxDismissalCountParam[] =
    "discount-consent-ntp-max-dismiss-count";
const base::FeatureParam<int>
    kNtpChromeCartModuleDiscountConsentMaxDismissalCount{
        &commerce::kDiscountConsentV2,
        kNtpChromeCartModuleDiscountConsentMaxDismissalCountParam, 1};

// String change variation params.
const char kNtpChromeCartModuleDiscountConsentStringChangeContentParam[] =
    "string-change-content";
const base::FeatureParam<std::string>
    kNtpChromeCartModuleDiscountConsentStringChangeContent{
        &commerce::kDiscountConsentV2,
        kNtpChromeCartModuleDiscountConsentStringChangeContentParam, ""};

const char kNtpChromeCartModuleDiscountConsentInlineShowCloseButtonParam[] =
    "inline-card-show-button";
const base::FeatureParam<bool>
    kNtpChromeCartModuleDiscountConsentInlineShowCloseButton{
        &commerce::kDiscountConsentV2,
        kNtpChromeCartModuleDiscountConsentStringChangeContentParam, true};

// Discount consent v2 step 1 params.
const char
    kNtpChromeCartModuleDiscountConsentNtpStepOneUseStaticContentParam[] =
        "step-one-use-static-content";
const base::FeatureParam<bool>
    kNtpChromeCartModuleDiscountConsentNtpStepOneUseStaticContent{
        &commerce::kDiscountConsentV2,
        kNtpChromeCartModuleDiscountConsentNtpStepOneUseStaticContentParam,
        true};
const char kNtpChromeCartModuleDiscountConsentNtpStepOneStaticContentParam[] =
    "step-one-static-content";
const base::FeatureParam<std::string>
    kNtpChromeCartModuleDiscountConsentNtpStepOneStaticContent{
        &commerce::kDiscountConsentV2,
        kNtpChromeCartModuleDiscountConsentNtpStepOneStaticContentParam, ""};
const char kNtpChromeCartModuleDiscountConsentNtpStepOneContentOneCartParam[] =
    "step-one-one-cart-content";
const base::FeatureParam<std::string>
    kNtpChromeCartModuleDiscountConsentNtpStepOneContentOneCart{
        &commerce::kDiscountConsentV2,
        kNtpChromeCartModuleDiscountConsentNtpStepOneContentOneCartParam, ""};
const char kNtpChromeCartModuleDiscountConsentNtpStepOneContentTwoCartsParam[] =
    "step-one-two-carts-content";
const base::FeatureParam<std::string>
    kNtpChromeCartModuleDiscountConsentNtpStepOneContentTwoCarts{
        &commerce::kDiscountConsentV2,
        kNtpChromeCartModuleDiscountConsentNtpStepOneContentTwoCartsParam, ""};
const char
    kNtpChromeCartModuleDiscountConsentNtpStepOneContentThreeCartsParam[] =
        "step-one-three-carts-content";
const base::FeatureParam<std::string>
    kNtpChromeCartModuleDiscountConsentNtpStepOneContentThreeCarts{
        &commerce::kDiscountConsentV2,
        kNtpChromeCartModuleDiscountConsentNtpStepOneContentThreeCartsParam,
        ""};

// Discount consent v2 step 2 params.
const char kNtpChromeCartModuleDiscountConsentNtpStepTwoContentParam[] =
    "step-two-content";
const base::FeatureParam<std::string>
    kNtpChromeCartModuleDiscountConsentNtpStepTwoContent{
        &commerce::kDiscountConsentV2,
        kNtpChromeCartModuleDiscountConsentNtpStepTwoContentParam, ""};
const char
    kNtpChromeCartModuleDiscountConsentInlineStepTwoDifferentColorParam[] =
        "step-two-different-color";
const base::FeatureParam<bool>
    kNtpChromeCartModuleDiscountConsentInlineStepTwoDifferentColor{
        &commerce::kDiscountConsentV2,
        kNtpChromeCartModuleDiscountConsentInlineStepTwoDifferentColorParam,
        false};
const char kNtpChromeCartModuleDiscountConsentNtpDialogContentTitleParam[] =
    "dialog-content-title";
const base::FeatureParam<std::string>
    kNtpChromeCartModuleDiscountConsentNtpDialogContentTitle{
        &commerce::kDiscountConsentV2,
        kNtpChromeCartModuleDiscountConsentNtpDialogContentTitleParam, ""};

const char kContextualConsentShowOnCartAndCheckoutPageParam[] =
    "show-on-cart-and-checkout-page";
const base::FeatureParam<bool> kContextualConsentShowOnCartAndCheckoutPage{
    &commerce::kDiscountConsentV2,
    kContextualConsentShowOnCartAndCheckoutPageParam, false};
const char kContextualConsentShowOnSRPParam[] = "show-on-srp";
const base::FeatureParam<bool> kContextualConsentShowOnSRP{
    &commerce::kDiscountConsentV2, kContextualConsentShowOnSRPParam, false};

const char kCommerceHintAndroidHeuristicsImprovementParam[] =
    "CommerceHintAndroidHeuristicsImprovementParam";

const char kReadyToFetchMerchantWidePromotionParam[] = "ready-to-fetch";
const base::FeatureParam<bool> kReadyToFetchMerchantWidePromotion{
    &commerce::kMerchantWidePromotion, kReadyToFetchMerchantWidePromotionParam,
    true};

const char kCodeBasedRuleDiscountParam[] = "code-based-rbd";
const base::FeatureParam<bool> kCodeBasedRuleDiscount{
    &commerce::kCodeBasedRBD, kCodeBasedRuleDiscountParam, false};
const char kCodeBasedRuleDiscountCouponDeletionTimeParam[] =
    "coupon-deletion-time";
const base::FeatureParam<base::TimeDelta>
    kCodeBasedRuleDiscountCouponDeletionTime{
        &commerce::kCodeBasedRBD, kCodeBasedRuleDiscountCouponDeletionTimeParam,
        base::Seconds(6)};

const char kRevertIconOnFailureParam[] =
    "shopping-list-revert-page-action-icon-on-failure";
const base::FeatureParam<bool> kRevertIconOnFailure{
    &kShoppingList, kRevertIconOnFailureParam, false};

// CommercePriceTrackingChipExperiment params.
const char kCommercePriceTrackingChipExperimentVariationParam[] =
    "price-tracking-chip-experiment-variation";
const base::FeatureParam<int> kCommercePriceTrackingChipExperimentVariation{
    &commerce::kCommercePriceTrackingChipExperiment,
    kCommercePriceTrackingChipExperimentVariationParam, 0};

bool IsPartnerMerchant(const GURL& url) {
  return commerce::IsCouponDiscountPartnerMerchant(url) ||
         IsRuleDiscountPartnerMerchant(url);
}

bool IsRuleDiscountPartnerMerchant(const GURL& url) {
  const std::string& url_string = url.spec();
  return RE2::PartialMatch(
      re2::StringPiece(url_string.data(), url_string.size()),
      GetRulePartnerMerchantPattern());
}

bool IsCouponDiscountPartnerMerchant(const GURL& url) {
  const std::string& url_string = url.spec();
  return RE2::PartialMatch(
      re2::StringPiece(url_string.data(), url_string.size()),
      GetCouponPartnerMerchantPattern());
}

bool IsCartDiscountFeatureEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(
      ntp_features::kNtpChromeCartModule,
      ntp_features::kNtpChromeCartModuleAbandonedCartDiscountParam, true);
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

bool isContextualConsentEnabled() {
  return kContextualConsentShowOnCartAndCheckoutPage.Get() ||
         kContextualConsentShowOnSRP.Get();
}

bool IsShoppingListAllowedForEnterprise(PrefService* prefs) {
  const base::Value* pref =
      prefs->GetUserPrefValue(kShoppingListEnabledPrefName);

  // Default to true if there is no value set.
  return !pref || pref->GetBool();
}

std::string GetCurrentCountryCode(variations::VariationsService* variations) {
  std::string country;

  if (variations)
    country = variations->GetStoredPermanentCountry();

  // Since variations doesn't provide a permanent country by default on things
  // like local builds, we try to fall back to the country_codes component which
  // should always have one.
  if (country.empty())
    country = country_codes::GetCurrentCountryCode();

  return country;
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
  const auto host_string = url.host_piece();
  auto* pattern_from_component =
      commerce_heuristics::CommerceHeuristicsData::GetInstance()
          .GetNoDiscountMerchantPattern();
  // If pattern from component updater is not available, merchants are
  // considered to have no discounts by default.
  if (!pattern_from_component)
    return true;
  return RE2::PartialMatch(
      re2::StringPiece(host_string.data(), host_string.size()),
      *pattern_from_component);
}
#endif
}  // namespace commerce
