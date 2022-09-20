// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/commerce_feature_list.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
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

const base::Feature kCommerceAllowLocalImages{
    "CommerceAllowLocalImages", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCommerceAllowOnDemandBookmarkUpdates{
    "CommerceAllowOnDemandBookmarkUpdates", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kCommerceAllowServerImages{
    "CommerceAllowServerImages", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kCommerceCoupons{"CommerceCoupons",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCommerceMerchantViewer{"CommerceMerchantViewer",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCommercePriceTracking{"CommercePriceTracking",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

const base::FeatureParam<bool> kDeleteAllMerchantsOnClearBrowsingHistory{
    &kCommerceMerchantViewer, "delete_all_merchants_on_clear_history", false};

const base::Feature kShoppingList{"ShoppingList",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kShoppingListEnableDesyncResolution{
    "ShoppingListEnableDesyncResolution", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kShoppingPDPMetrics{"ShoppingPDPMetrics",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kRetailCoupons{"RetailCoupons",
                                   base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kCommerceDeveloper{"CommerceDeveloper",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const char kRetailCouponsWithCodeParam[] = "RetailCouponsWithCodeParam";

// Params use for Discount Consent v2.
const base::Feature kDiscountConsentV2{"DiscountConsentV2",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCommerceHintAndroid{"CommerceHintAndroid",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kMerchantWidePromotion{"MerchantWidePromotion",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Params for Discount Consent V2 in the NTP Cart module.
const char kNtpChromeCartModuleDiscountConsentNtpVariationParam[] =
    "discount-consent-ntp-variation";
const base::FeatureParam<int> kNtpChromeCartModuleDiscountConsentNtpVariation{
    &commerce::kDiscountConsentV2,
    kNtpChromeCartModuleDiscountConsentNtpVariationParam, 0};
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
        false};
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
    false};

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
