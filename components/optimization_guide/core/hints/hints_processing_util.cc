// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/hints/hints_processing_util.h"

#include <string>

#include "base/containers/flat_set.h"
#include "base/hash/hash.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/stringprintf.h"
#include "components/optimization_guide/core/hints/store_update_data.h"
#include "components/optimization_guide/core/hints/url_pattern_with_wildcards.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace optimization_guide {

std::string GetStringNameForOptimizationType(
    proto::OptimizationType optimization_type) {
  switch (optimization_type) {
    case proto::OptimizationType::TYPE_UNSPECIFIED:
      return "Unspecified";
    case proto::OptimizationType::NOSCRIPT:
      return "NoScript";
    case proto::OptimizationType::RESOURCE_LOADING:
      return "ResourceLoading";
    case proto::OptimizationType::LITE_PAGE_REDIRECT:
      return "LitePageRedirect";
    case proto::OptimizationType::METADATA_FETCH_VALIDATION:
      return "MetadataFetchValidation";
    case proto::OptimizationType::DEFER_ALL_SCRIPT:
      return "DeferAllScript";
    case proto::OptimizationType::PERFORMANCE_HINTS:
      return "PerformanceHints";
    case proto::OptimizationType::LITE_PAGE:
      return "LitePage";
    case proto::OptimizationType::COMPRESS_PUBLIC_IMAGES:
      return "CompressPublicImages";
    case proto::OptimizationType::LOADING_PREDICTOR:
      return "LoadingPredictor";
    case proto::OptimizationType::FAST_HOST_HINTS:
      return "FastHostHints";
    case proto::OptimizationType::LITE_VIDEO:
      return "LiteVideo";
    case proto::OptimizationType::LINK_PERFORMANCE:
      return "LinkPerformance";
    case proto::OptimizationType::SHOPPING_PAGE_PREDICTOR:
      return "ShoppingPagePredictor";
    case proto::OptimizationType::MERCHANT_TRUST_SIGNALS:
      return "MerchantTrustSignals";
    case proto::OptimizationType::PRICE_TRACKING:
      return "PriceTracking";
    case proto::OptimizationType::BLOOM_FILTER_VALIDATION:
      return "BloomFilterValidation";
    case proto::OptimizationType::ABOUT_THIS_SITE:
      return "AboutThisSite";
    case proto::OptimizationType::MERCHANT_TRUST_SIGNALS_V2:
      return "MerchantTrustSignalsV2";
    case proto::OptimizationType::PAGE_ENTITIES:
      return "PageEntities";
    case proto::OptimizationType::HISTORY_CLUSTERS:
      return "HistoryClusters";
    case proto::OptimizationType::THANK_CREATOR_ELIGIBLE:
      return "ThankCreatorEligible";
    case proto::OptimizationType::IBAN_AUTOFILL_BLOCKED:
      return "IBANAutofillBlocked";
    case proto::OptimizationType::SALIENT_IMAGE:
      return "SalientImage";
    case proto::OptimizationType::AUTOFILL_SAMPLING_RATE:
      return "AutofillSamplingRate";
    case proto::OptimizationType::VCN_MERCHANT_OPT_OUT_VISA:
      return "VcnMerchantOptOutVisa";
    case proto::OptimizationType::PRICE_INSIGHTS:
      return "PriceInsights";
    case proto::OptimizationType::V8_COMPILE_HINTS:
      return "V8CompileHints";
    case proto::OptimizationType::SHOPPING_PAGE_TYPES:
      return "ShoppingPageTypes";
    case proto::OptimizationType::SHOPPING_DISCOUNTS:
      return "ShoppingDiscounts";
    case proto::OptimizationType::COMPOSE:
      return "Compose";
    case proto::OptimizationType::PIX_PAYMENT_MERCHANT_ALLOWLIST:
      return "PixPaymentMerchantAllowlist";
    case proto::OptimizationType::SHARED_CREDIT_CARD_FLIGHT_BENEFITS:
      return "SharedCreditCardFlightBenefits";
    case proto::OptimizationType::SHARED_CREDIT_CARD_DINING_BENEFITS:
      return "SharedCreditCardDiningBenefits";
    case proto::OptimizationType::SHARED_CREDIT_CARD_GROCERY_BENEFITS:
      return "SharedCreditCardGroceryBenefits";
    case proto::OptimizationType::SHARED_CREDIT_CARD_ENTERTAINMENT_BENEFITS:
      return "SharedCreditCardEntertainmentBenefits";
    case proto::OptimizationType::SHARED_CREDIT_CARD_STREAMING_BENEFITS:
      return "SharedCreditCardStreamingBenefits";
    case proto::OptimizationType::SHARED_CREDIT_CARD_SUBSCRIPTION_BENEFITS:
      return "SharedCreditCardSubscriptionBenefits";
    case proto::OptimizationType::CAPITAL_ONE_CREDIT_CARD_DINING_BENEFITS:
      return "CapitalOneCreditCardDiningBenefits";
    case proto::OptimizationType::CAPITAL_ONE_CREDIT_CARD_GROCERY_BENEFITS:
      return "CapitalOneCreditCardGroceryBenefits";
    case proto::OptimizationType::
        CAPITAL_ONE_CREDIT_CARD_ENTERTAINMENT_BENEFITS:
      return "CapitalOneCreditCardEntertainmentBenefits";
    case proto::OptimizationType::CAPITAL_ONE_CREDIT_CARD_STREAMING_BENEFITS:
      return "CapitalOneCreditCardStreamingBenefits";
    case proto::OptimizationType::AMERICAN_EXPRESS_CREDIT_CARD_FLIGHT_BENEFITS:
      return "AmericanExpressCreditCardFlightBenefits";
    case proto::OptimizationType::
        AMERICAN_EXPRESS_CREDIT_CARD_SUBSCRIPTION_BENEFITS:
      return "AmericanExpressCreditCardSubscriptionBenefits";
    case proto::OptimizationType::CAPITAL_ONE_CREDIT_CARD_BENEFITS_BLOCKED:
      return "CapitalOneCreditCardBenefitsBlocked";
    case proto::OptimizationType::VCN_MERCHANT_OPT_OUT_DISCOVER:
      return "VcnMerchantOptOutDiscover";
    case proto::OptimizationType::VCN_MERCHANT_OPT_OUT_MASTERCARD:
      return "VcnMerchantOptOutMastercard";
    case proto::OptimizationType::PIX_MERCHANT_ORIGINS_ALLOWLIST:
      return "PixMerchantOriginsAllowlist";
    case proto::OptimizationType::HISTORY_EMBEDDINGS:
      return "HistoryEmbeddings";
    case proto::OptimizationType::AUTOFILL_ABLATION_SITES_LIST1:
      return "AutofillAblationSitesList1";
    case proto::OptimizationType::AUTOFILL_ABLATION_SITES_LIST2:
      return "AutofillAblationSitesList2";
    case proto::OptimizationType::AUTOFILL_ABLATION_SITES_LIST3:
      return "AutofillAblationSitesList3";
    case proto::OptimizationType::AUTOFILL_ABLATION_SITES_LIST4:
      return "AutofillAblationSitesList4";
    case proto::OptimizationType::AUTOFILL_ABLATION_SITES_LIST5:
      return "AutofillAblationSitesList5";
    case proto::OptimizationType::AUTOFILL_ABLATION_SITES_LIST6:
      return "AutofillAblationSitesList6";
    case proto::OptimizationType::AMOUNT_EXTRACTION_ALLOWLIST:
      return "AmountExtractionAllowlist";
    case proto::OptimizationType::BUY_NOW_PAY_LATER_ALLOWLIST_AFFIRM:
      return "BuyNowPayLaterAllowlistAffirmDesktop";
    case proto::OptimizationType::BUY_NOW_PAY_LATER_ALLOWLIST_ZIP:
      return "BuyNowPayLaterAllowlistZipDesktop";
    case proto::OptimizationType::BUY_NOW_PAY_LATER_ALLOWLIST_KLARNA:
      return "BuyNowPayLaterAllowlistKlarnaDesktop";
    case proto::OptimizationType::BUY_NOW_PAY_LATER_ALLOWLIST_AFFIRM_ANDROID:
      return "BuyNowPayLaterAllowlistAffirmAndroid";
    case proto::OptimizationType::BUY_NOW_PAY_LATER_ALLOWLIST_ZIP_ANDROID:
      return "BuyNowPayLaterAllowlistZipAndroid";
    case proto::OptimizationType::BUY_NOW_PAY_LATER_ALLOWLIST_KLARNA_ANDROID:
      return "BuyNowPayLaterAllowlistKlarnaAndroid";
    case proto::OptimizationType::BUY_NOW_PAY_LATER_BLOCKLIST_AFFIRM:
      return "BuyNowPayLaterBlocklistAffirm";
    case proto::OptimizationType::BUY_NOW_PAY_LATER_BLOCKLIST_ZIP:
      return "BuyNowPayLaterBlocklistZip";
    case proto::OptimizationType::BUY_NOW_PAY_LATER_BLOCKLIST_KLARNA:
      return "BuyNowPayLaterBlocklistKlarna";
    case proto::OptimizationType::SAVED_TAB_GROUP:
      return "SavedTabGroup";
    case proto::OptimizationType::TEXT_CLASSIFIER_ENTITY_DETECTION:
      return "TextClassifierEntityDetection";
    case proto::OptimizationType::EWALLET_MERCHANT_ALLOWLIST:
      return "EwalletMerchantAllowlist";
    case proto::OptimizationType::OPTIMIZATION_GUIDE_ICON_VIEW:
      return "OptimizationGuideIconView";
    case proto::OptimizationType::PRIVACY_POLICY_ANNOTATION:
      return "PrivacyPolicyAnnotation";
    case proto::OptimizationType::BMO_CREDIT_CARD_AIR_MILES_PARTNER_BENEFITS:
      return "BmoCreditCardAirMilesPartnerBenefits";
    case proto::OptimizationType::BMO_CREDIT_CARD_ALCOHOL_STORE_BENEFITS:
      return "BmoCreditCardAlcoholStoreBenefits";
    case proto::OptimizationType::BMO_CREDIT_CARD_DINING_BENEFITS:
      return "BmoCreditCardDiningBenefits";
    case proto::OptimizationType::BMO_CREDIT_CARD_DRUGSTORE_BENEFITS:
      return "BmoCreditCardDrugstoreBenefits";
    case proto::OptimizationType::BMO_CREDIT_CARD_ENTERTAINMENT_BENEFITS:
      return "BmoCreditCardEntertainmentBenefits";
    case proto::OptimizationType::BMO_CREDIT_CARD_GROCERY_BENEFITS:
      return "BmoCreditCardGroceryBenefits";
    case proto::OptimizationType::BMO_CREDIT_CARD_OFFICE_SUPPLY_BENEFITS:
      return "BmoCreditCardOfficeSupplyBenefits";
    case proto::OptimizationType::BMO_CREDIT_CARD_RECURRING_BILL_BENEFITS:
      return "BmoCreditCardRecurringBillBenefits";
    case proto::OptimizationType::BMO_CREDIT_CARD_TRANSIT_BENEFITS:
      return "BmoCreditCardTransitBenefits";
    case proto::OptimizationType::BMO_CREDIT_CARD_TRAVEL_BENEFITS:
      return "BmoCreditCardTravelBenefits";
    case proto::OptimizationType::BMO_CREDIT_CARD_WHOLESALE_CLUB_BENEFITS:
      return "BmoCreditCardWholesaleClubBenefits";
    case proto::OptimizationType::GLIC_CONTEXTUAL_CUEING:
      return "GlicContextualCueing";
    case proto::OptimizationType::GLIC_ZERO_STATE_SUGGESTIONS:
      return "GlicZeroStateSuggestions";
    case proto::OptimizationType::GLIC_ACTION_PAGE_BLOCK:
      return "GlicActionPageBlock";
    case proto::OptimizationType::FEDCM_CLICKTHROUGH_RATE:
      return "FedCmClickthroughRate";
    case proto::OptimizationType::DIGITAL_CREDENTIALS_LOW_FRICTION:
      return "DigitalCredentialsLowFriction";
    case proto::OptimizationType::A2A_MERCHANT_ALLOWLIST:
      return "A2AMerchantAllowlist";
    case proto::OptimizationType::
        SHARED_CREDIT_CARD_FLAT_RATE_BENEFITS_BLOCKLIST:
      return "SharedCreditCardFlatRateBenefitBlocklist";
    case proto::OptimizationType::WALLETABLE_PASS_DETECTION_ALLOWLIST:
      return "WalletablePassDetectionAllowlist";
    case proto::OptimizationType::LENS_OVERLAY_EDU_ACTION_CHIP_BLOCKLIST:
      return "LensOverlayEduActionChipBlocklist";
    case proto::OptimizationType::LENS_OVERLAY_EDU_ACTION_CHIP_ALLOWLIST:
      return "LensOverlayEduActionChipAllowlist";
    case proto::OptimizationType::WALLETABLE_PASS_DETECTION_LOYALTY_ALLOWLIST:
      return "WalletablePassDetectionLoyaltyAllowlist";
    case proto::OptimizationType::
        WALLETABLE_PASS_DETECTION_BOARDING_PASS_ALLOWLIST:
      return "WalletablePassDetectionBoardingPassAllowlist";
    case proto::OptimizationType::NTP_NEXT_DEEP_DIVE_ACTION_CHIP_BLOCKLIST:
      return "NtpNextDeepDiveActionChipBlocklist";
    case proto::OptimizationType::NTP_NEXT_DEEP_DIVE_ACTION_CHIP_ALLOWLIST:
      return "NtpNextDeepDiveActionChipAllowlist";
    case proto::OptimizationType::READER_MODE_ELIGIBLE:
      return "ReaderModeEligible";
    case proto::OptimizationType::AUTOFILL_ACTOR_IFRAME_ORIGIN_ALLOWLIST:
      return "AutofillActorIframeOriginAllowlist";
  }

  // The returned string is used to record histograms for the optimization type.
  //
  // Whenever a new value is added, make sure to add it to the OptimizationType
  // variant list in
  // //tools/metrics/histograms/metadata/optimization/histograms.xml. Also
  // update enums.xml when adding new value in OptimizationType.
  NOTREACHED();
}

const proto::PageHint* FindPageHintForURL(const GURL& gurl,
                                          const proto::Hint* hint) {
  if (!hint) {
    return nullptr;
  }

  for (const auto& page_hint : hint->page_hints()) {
    if (page_hint.page_pattern().empty()) {
      continue;
    }
    URLPatternWithWildcards url_pattern(page_hint.page_pattern());
    if (url_pattern.Matches(gurl.spec())) {
      // Return the first matching page hint.
      return &page_hint;
    }
  }
  return nullptr;
}

std::string HashHostForDictionary(const std::string& host) {
  return base::StringPrintf("%x", base::PersistentHash(host));
}

bool IsValidURLForURLKeyedHint(const GURL& url) {
  if (!url.has_host()) {
    return false;
  }
  if (net::IsLocalhost(url)) {
    return false;
  }
  if (url.HostIsIPAddress()) {
    return false;
  }
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return false;
  }
  if (url.has_username() || url.has_password()) {
    return false;
  }
  return true;
}

}  // namespace optimization_guide
