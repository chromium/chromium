// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/bnpl_util.h"

#include <optional>
#include <string>
#include <utility>

#include "base/containers/to_vector.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/integrators/optimization_guide/autofill_optimization_guide_decider.h"
#include "components/autofill/core/browser/payments/bnpl_manager.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/payments/core/currency_formatter.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill::payments {

BnplIssuerContext::BnplIssuerContext() = default;

BnplIssuerContext::BnplIssuerContext(BnplIssuer issuer,
                                     BnplIssuerEligibilityForPage eligibility)
    : issuer(std::move(issuer)), eligibility(eligibility) {}

BnplIssuerContext::BnplIssuerContext(const BnplIssuerContext& other) = default;

BnplIssuerContext::BnplIssuerContext(BnplIssuerContext&&) = default;

BnplIssuerContext& BnplIssuerContext::operator=(
    const BnplIssuerContext& other) = default;

BnplIssuerContext& BnplIssuerContext::operator=(BnplIssuerContext&&) = default;

BnplIssuerContext::~BnplIssuerContext() = default;

bool BnplIssuerContext::operator==(const BnplIssuerContext&) const = default;

bool BnplIssuerContext::IsEligible() const {
  switch (eligibility) {
    case BnplIssuerEligibilityForPage::kUndefined:
      NOTREACHED();
    case BnplIssuerEligibilityForPage::kIsEligible:
    case BnplIssuerEligibilityForPage::
        kTemporarilyEligibleCheckoutAmountNotYetKnown:
      return true;
    case BnplIssuerEligibilityForPage::kNotEligibleIssuerDoesNotSupportMerchant:
    case BnplIssuerEligibilityForPage::kNotEligibleCheckoutAmountTooLow:
    case BnplIssuerEligibilityForPage::kNotEligibleCheckoutAmountTooHigh:
      return false;
  }
  NOTREACHED();
}

BnplTosModel::BnplTosModel() = default;

BnplTosModel::BnplTosModel(const BnplTosModel& other) = default;

BnplTosModel::BnplTosModel(BnplTosModel&& other) = default;

BnplTosModel& BnplTosModel::operator=(const BnplTosModel& other) = default;

BnplTosModel& BnplTosModel::operator=(BnplTosModel&& other) = default;

BnplTosModel::~BnplTosModel() = default;

bool BnplTosModel::operator==(const BnplTosModel&) const = default;

std::vector<BnplIssuerContext> GetSortedBnplIssuerContext(
    const AutofillClient& client,
    std::optional<int64_t> checkout_amount) {
  AutofillOptimizationGuideDecider* autofill_optimization_guide =
      client.GetAutofillOptimizationGuideDecider();
  const GURL& merchant_url =
      client.GetLastCommittedPrimaryMainFrameOrigin().GetURL();

  // Check BNPL issuer eligibility for the current page and save the
  // eligibility with the corresponding issuer to the vector of
  // `BnplIssuerContext`.
  std::vector<BnplIssuerContext> result = base::ToVector(
      client.GetPaymentsAutofillClient()
          ->GetPaymentsDataManager()
          .GetBnplIssuers(),
      [&autofill_optimization_guide, &merchant_url,
       checkout_amount](const BnplIssuer& issuer) -> BnplIssuerContext {
        // For MVP, BNPL will only target US users and support USD.
        const base::optional_ref<const BnplIssuer::EligiblePriceRange>
            price_range =
                issuer.GetEligiblePriceRangeForCurrency(/*currency=*/"USD");
        CHECK(price_range.has_value());

        BnplIssuerEligibilityForPage eligibility;

        if (!autofill_optimization_guide->IsUrlEligibleForBnplIssuer(
                issuer.issuer_id(), merchant_url)) {
          eligibility = BnplIssuerEligibilityForPage::
              kNotEligibleIssuerDoesNotSupportMerchant;
        } else if (!checkout_amount) {
          // The only case this code gets hit is if the BNPL issuer needs to be
          // shown before the LLM call returns a valid checkout amount.
          eligibility = BnplIssuerEligibilityForPage::
              kTemporarilyEligibleCheckoutAmountNotYetKnown;
        } else if (checkout_amount < price_range->price_lower_bound) {
          eligibility =
              BnplIssuerEligibilityForPage::kNotEligibleCheckoutAmountTooLow;
        } else if (checkout_amount > price_range->price_upper_bound) {
          eligibility =
              BnplIssuerEligibilityForPage::kNotEligibleCheckoutAmountTooHigh;
        } else {
          eligibility = BnplIssuerEligibilityForPage::kIsEligible;
        }
        return {issuer, eligibility};
      });

  // Shuffle `result` before sorting so that the order of two
  // equivalently-sorted elements are randomized. This is to ensure there is no
  // implicit preference towards any issuers.
  base::RandomShuffle(result.begin(), result.end());

  // Sort the `BnplIssuerContext` vector so that it follows below rules:
  // 1. Eligible issuers should be in front of uneligible ones in a sorted
  //    vector.
  // 2. Linked issuers must go before unlinked ones if they have the same
  //    eligibility.
  // Note: If one issuer has a payment instrument and the other doesn't,
  //    then one is linked and the other is unlinked.
  std::ranges::stable_sort(
      result, [](const BnplIssuerContext& rhs, const BnplIssuerContext& lhs) {
        // Lambda comparator which returns true if `rhs` should be in front of
        // `lhs`.
        // Note: Boolean value `false` is less than boolean value `true`.
        return std::forward_as_tuple(
                   rhs.eligibility == BnplIssuerEligibilityForPage::kIsEligible,
                   rhs.issuer.payment_instrument().has_value()) >
               std::forward_as_tuple(
                   lhs.eligibility == BnplIssuerEligibilityForPage::kIsEligible,
                   lhs.issuer.payment_instrument().has_value());
      });

  return result;
}

Suggestion::Icon GetBnplSuggestionIcon(BnplIssuer::IssuerId issuer_id,
                                       bool is_linked) {
  switch (issuer_id) {
    case BnplIssuer::IssuerId::kBnplAffirm:
      return is_linked ? Suggestion::Icon::kBnplAffirmLinked
                       : Suggestion::Icon::kBnplAffirmUnlinked;
    case BnplIssuer::IssuerId::kBnplAfterpay:
      return is_linked ? Suggestion::Icon::kBnplAfterpayLinked
                       : Suggestion::Icon::kBnplAfterpayUnlinked;
    case BnplIssuer::IssuerId::kBnplKlarna:
      return is_linked ? Suggestion::Icon::kBnplKlarnaLinked
                       : Suggestion::Icon::kBnplKlarnaUnlinked;
    case BnplIssuer::IssuerId::kBnplZip:
      return is_linked ? Suggestion::Icon::kBnplZipLinked
                       : Suggestion::Icon::kBnplZipUnlinked;
  }
}

std::u16string GetBnplIssuerSelectionOptionText(
    BnplIssuer::IssuerId issuer_id,
    const std::string& app_locale,
    base::span<const BnplIssuerContext> issuer_contexts) {
  // TODO(crbug.com/403361321): Add a util function that returns all supported
  // locales.
  CHECK_EQ(app_locale, "en-US");
  ::payments::CurrencyFormatter formatter = ::payments::CurrencyFormatter(
      /*currency_code=*/"USD",
      /*locale_name=*/app_locale);
  formatter.SetMaxFractionalDigits(/*max_fractional_digits=*/2);

  BnplIssuerContext issuer_context;
  for (const BnplIssuerContext& current_issuer_context : issuer_contexts) {
    if (current_issuer_context.issuer.issuer_id() == issuer_id) {
      issuer_context = current_issuer_context;
      break;
    }
  }

  // Get the price range in USD as it's the only supported currency for now.
  base::optional_ref<const BnplIssuer::EligiblePriceRange>
      eligible_price_range =
          issuer_context.issuer.GetEligiblePriceRangeForCurrency("USD");
  CHECK(eligible_price_range);

  switch (issuer_context.eligibility) {
    case BnplIssuerEligibilityForPage::kUndefined:
      NOTREACHED();
    case BnplIssuerEligibilityForPage::kIsEligible:
    case BnplIssuerEligibilityForPage::
        kTemporarilyEligibleCheckoutAmountNotYetKnown:
      switch (issuer_id) {
        case BnplIssuer::IssuerId::kBnplAffirm:
        case BnplIssuer::IssuerId::kBnplAfterpay:
#if BUILDFLAG(IS_ANDROID)
          return l10n_util::GetStringUTF16(
              IDS_AUTOFILL_BNPL_ISSUER_SELECTION_TEXT_AFFIRM_BOTTOM_SHEET);
#else
          if (base::FeatureList::IsEnabled(
                  features::kAutofillEnablePayNowPayLaterTabs)) {
            return l10n_util::GetStringUTF16(
                IDS_AUTOFILL_CARD_BNPL_PAY_LATER_PAYMENT_OPTION_AFFIRM_AND_AFTERPAY);
          } else {
            return l10n_util::GetStringUTF16(
                IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_PAYMENT_OPTION_AFFIRM_AND_AFTERPAY);
          }
#endif  // BUILDFLAG(IS_ANDROID)
        case BnplIssuer::IssuerId::kBnplZip:
#if BUILDFLAG(IS_ANDROID)
          return l10n_util::GetStringUTF16(
              IDS_AUTOFILL_BNPL_ISSUER_SELECTION_TEXT_ZIP_BOTTOM_SHEET);
#else
          if (base::FeatureList::IsEnabled(
                  features::kAutofillEnablePayNowPayLaterTabs)) {
            return l10n_util::GetStringUTF16(
                IDS_AUTOFILL_CARD_BNPL_PAY_LATER_PAYMENT_OPTION_ZIP);
          } else {
            return l10n_util::GetStringUTF16(
                IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_PAYMENT_OPTION_ZIP);
          }

#endif  // BUILDFLAG(IS_ANDROID)
        case BnplIssuer::IssuerId::kBnplKlarna:
#if BUILDFLAG(IS_ANDROID)
          return l10n_util::GetStringUTF16(
              IDS_AUTOFILL_BNPL_ISSUER_SELECTION_TEXT_KLARNA_BOTTOM_SHEET);
#else
          if (base::FeatureList::IsEnabled(
                  features::kAutofillEnablePayNowPayLaterTabs)) {
            return l10n_util::GetStringUTF16(
                IDS_AUTOFILL_CARD_BNPL_PAY_LATER_PAYMENT_OPTION_KLARNA);
          } else {
            return l10n_util::GetStringUTF16(
                IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_PAYMENT_OPTION_KLARNA);
          }
#endif  // BUILDFLAG(IS_ANDROID)
      }
      NOTREACHED();
    case BnplIssuerEligibilityForPage::kNotEligibleIssuerDoesNotSupportMerchant:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_PAYMENT_OPTION_NOT_SUPPORTED_BY_MERCHANT);
    case BnplIssuerEligibilityForPage::kNotEligibleCheckoutAmountTooLow:
      // Divide displayed price by `1'000'000.0` to convert from micros and
      // retain decimals.
      return l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_PAYMENT_OPTION_CHECKOUT_AMOUNT_TOO_LOW,
          formatter.Format(base::NumberToString(
              eligible_price_range->price_lower_bound / 1'000'000.0)));
    case BnplIssuerEligibilityForPage::kNotEligibleCheckoutAmountTooHigh:
      // Divide displayed price by `1'000'000.0` to convert from micros and
      // retain decimals.
      return l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_PAYMENT_OPTION_CHECKOUT_AMOUNT_TOO_HIGH,
          formatter.Format(base::NumberToString(
              eligible_price_range->price_upper_bound / 1'000'000.0)));
  }
  NOTREACHED();
}

TextWithLink GetBnplUiFooterText() {
  TextWithLink text_with_link;
  std::u16string payments_settings_link_text = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_FOOTNOTE_HIDE_OPTION_PAYMENT_SETTINGS_LINK_TEXT);
  size_t offset = 0;
  text_with_link.text = l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_FOOTNOTE_HIDE_OPTION,
      payments_settings_link_text, &offset);

  text_with_link.offset =
      gfx::Range(offset, offset + payments_settings_link_text.length());

  return text_with_link;
}

TextWithLink GetBnplUiFooterTextForAi(
    const PaymentsDataManager& payments_data_manager) {
  TextWithLink text_with_link;
  std::u16string payments_settings_link_text = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_FOOTNOTE_HIDE_OPTION_PAYMENT_SETTINGS_LINK_TEXT);
  size_t offset = 0;
  text_with_link.text = l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_AI_FOOTNOTE,
      payments_settings_link_text, &offset);

  text_with_link.offset =
      gfx::Range(offset, offset + payments_settings_link_text.length());
  if (!payments_data_manager
           .IsAutofillAmountExtractionAiTermsSeenPrefEnabled()) {
    std::u16string ai_notice = l10n_util::GetStringUTF16(
        IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_FOOTNOTE_FOR_AI_AMOUNT_EXTRACTION_NOTE);
    text_with_link.bold_range = gfx::Range(0, ai_notice.length());
  }
  return text_with_link;
}

bool ShouldShowBnplSuggestions(const AutofillClient& client,
                               FieldType trigger_field_type) {
  // If this is called on Chrome Android, it must be called due to attempting to
  // add BNPL to the keyboard accessory suggestions, which is not supported.
  if constexpr (BUILDFLAG(IS_ANDROID)) {
    return false;
  }
  // BNPL suggestions should not be shown for CVC fields.
  if (kCvcFieldTypes.contains(trigger_field_type)) {
    return false;
  }
  // BNPL suggestions require that at least one BNPL issuer is present and the
  // domain is eligible for BNPL.
  if (!IsEligibleForBnpl(client)) {
    return false;
  }
  // This feature can only be enabled by the feature flag:
  // `kAutofillEnableAiBasedAmountExtraction`.
  return base::FeatureList::IsEnabled(
      features::kAutofillEnableAiBasedAmountExtraction);
}

bool IsEligibleForBnpl(const AutofillClient& client) {
  // BNPL is not supported in off-the-record (incognito) mode.
  if (client.IsOffTheRecord()) {
    return false;
  }

  AutofillOptimizationGuideDecider* autofill_optimization_guide_decider =
      client.GetAutofillOptimizationGuideDecider();
  if (!autofill_optimization_guide_decider) {
    return false;
  }

  const GURL& url = client.GetLastCommittedPrimaryMainFrameURL();

  return std::ranges::any_of(
      client.GetPaymentsAutofillClient()
          ->GetPaymentsDataManager()
          .GetBnplIssuers(),
      [&autofill_optimization_guide_decider,
       &url](const BnplIssuer& bnpl_issuer) {
        return autofill_optimization_guide_decider->IsUrlEligibleForBnplIssuer(
            bnpl_issuer.issuer_id(), url);
      });
}

bool ShouldStartPayLaterWithLoadingSpinner(
    const PaymentsDataManager& payments_data_manager) {
  return payments_data_manager
      .IsAutofillAmountExtractionAiTermsSeenPrefEnabled();
}

}  // namespace autofill::payments
