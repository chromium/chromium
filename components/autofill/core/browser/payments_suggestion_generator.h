// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_SUGGESTION_GENERATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_SUGGESTION_GENERATOR_H_

#include <optional>
#include <string>
#include <vector>

#include "base/check_deref.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/types/optional_ref.h"
#include "components/autofill/core/browser/data_model/autofill_wallet_usage_data.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/metrics/log_event.h"
#include "components/autofill/core/browser/metrics/payments/card_metadata_metrics.h"
#include "components/autofill/core/browser/metrics/suggestions_list_metrics.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/common/aliases.h"

namespace autofill {

class AutofillClient;
class AutofillOfferData;
class CreditCard;
class FormFieldData;
class Iban;

// Describes the suggestions returned by
// `GetCreditCardOrCvcFieldSuggestions()`.
struct CreditCardSuggestionSummary {
  // Whether any card has card-linked offers.
  bool with_offer = false;
  // True if any card has a saved CVC.
  bool with_cvc = false;
  // Contains card metadata related information used for metrics logging.
  autofill_metrics::CardMetadataLoggingContext metadata_logging_context;
  // Contains information regarding the ranking of suggestions and is used for
  // metrics logging.
  autofill_metrics::SuggestionRankingContext ranking_context;
};

// Generates suggestions for all available credit cards based on the
// `trigger_field_type`, `trigger_field`, `trigger_source`,
// `four_digit_combinations_in_dom`. `summary` contains metadata about the
// returned suggestions.
// `autofilled_last_four_digits_in_form_for_suggestion_filtering` is a list of
// card number last four that will be used for suggestion filtering. This is
// used to avoid showing suggestions that is unrelated to the cards that have
// already been autofilled in the form.
std::vector<Suggestion> GetSuggestionsForCreditCards(
    const AutofillClient& client,
    const FormFieldData& trigger_field,
    FieldType trigger_field_type,
    AutofillSuggestionTriggerSource trigger_source,
    CreditCardSuggestionSummary& summary,
    bool should_show_scan_credit_card,
    bool should_show_cards_from_account,
    const std::vector<std::string>& four_digit_combinations_in_dom,
    const std::vector<std::u16string>&
        autofilled_last_four_digits_in_form_for_suggestion_filtering);

// Generates suggestions for all available credit cards based on the
// `trigger_field_type`, `trigger_field` and `trigger_source`.
// `summary` contains metadata about the returned suggestions.
// `last_four_set_for_cvc_suggestion_filtering` is a set of card number last
// four that will be used for suggestion filtering. This is used to avoid
// showing suggestions that is unrelated to the cards that have already been
// autofilled in the form.
// TODO(crbug.com/40916587): Implement last four extraction from the DOM.
std::vector<Suggestion> GetCreditCardOrCvcFieldSuggestions(
    const AutofillClient& client,
    const FormFieldData& trigger_field,
    const std::vector<std::string>& four_digit_combinations_in_dom,
    const std::vector<std::u16string>&
        autofilled_last_four_digits_in_form_for_suggestion_filtering,
    FieldType trigger_field_type,
    AutofillSuggestionTriggerSource trigger_source,
    bool should_show_scan_credit_card,
    bool should_show_cards_from_account,
    CreditCardSuggestionSummary& summary);

// Generates suggestions for standalone CVC fields. These only apply to
// virtual cards that are saved on file to a merchant. In these cases,
// we only display the virtual card option and do not show FPAN option.
std::vector<Suggestion> GetVirtualCardStandaloneCvcFieldSuggestions(
    const AutofillClient& client,
    const FormFieldData& trigger_field,
    autofill_metrics::CardMetadataLoggingContext& metadata_logging_context,
    base::flat_map<std::string, VirtualCardUsageData::VirtualCardLastFour>&
        virtual_card_guid_to_last_four_map);

// Returns the credit cards to be shown in touch to fill suggestions.
std::vector<CreditCard> GetTouchToFillCardsToSuggest(
    const AutofillClient& client,
    const FormFieldData& trigger_field,
    FieldType trigger_field_type);

// Generates touch-to-fill suggestions for all available credit cards to be
// used in the bottom sheet.
std::vector<Suggestion> GetCreditCardSuggestionsForTouchToFill(
    base::span<const CreditCard> credit_cards,
    const AutofillClient& client);

// Generates a footer suggestion "Manage payment methods..." menu item which
// will redirect to Chrome payment settings page. `with_gpay_logo` is used to
// conditionally add GPay logo icon to the manage payment methods suggestion.
//
// The difference between `CreateManageCreditCardsSuggestion()` and
// `CreateManageIbansSuggestion()` is that they use a different
// `SuggestionType`. This distinction is needed for metrics recording.
Suggestion CreateManageCreditCardsSuggestion(bool with_gpay_logo);

// Generates a footer suggestion "Manage payment methods..." menu item which
// will redirect to Chrome payment settings page.
//
// The difference between `CreateManageCreditCardsSuggestion()` and
// `CreateManageIbansSuggestion()` is that they use a different
// `SuggestionType`. This distinction is needed for metrics recording.
Suggestion CreateManageIbansSuggestion();

// Generates suggestions for all available IBANs.
std::vector<Suggestion> GetSuggestionsForIbans(const std::vector<Iban>& ibans);

// Converts the vector of promo code offers that is passed in to a vector of
// suggestions that can be displayed to the user for a promo code field.
std::vector<Suggestion> GetPromoCodeSuggestionsFromPromoCodeOffers(
    const std::vector<const AutofillOfferData*>& promo_code_offers);

// Returns true if the suggestion created from the card can be accepted by the
// user. Returns false when merchant does not accept the given card for example
// when merchants opt-out of VCNs.
bool IsCardSuggestionAcceptable(const CreditCard& card,
                                const AutofillClient& client,
                                bool is_manual_fallback);

// Exposes `GetOrderedCardsToSuggest` in tests.
std::vector<CreditCard> GetOrderedCardsToSuggestForTest(
    const AutofillClient& client,
    const FormFieldData& trigger_field,
    FieldType trigger_field_type,
    bool suppress_disused_cards,
    bool prefix_match,
    bool require_non_empty_value_on_trigger_field,
    bool include_virtual_cards,
    bool use_legacy_algorithm = false);

// Exposes `CreateCreditCardSuggestion` in tests.
Suggestion CreateCreditCardSuggestionForTest(
    const CreditCard& credit_card,
    const AutofillClient& client,
    FieldType trigger_field_type,
    bool virtual_card_option,
    bool card_linked_offer_available,
    base::optional_ref<autofill_metrics::CardMetadataLoggingContext>
        metadata_logging_context = std::nullopt);

// Exposes `ShouldShowVirtualCardOption` in tests.
bool ShouldShowVirtualCardOptionForTest(const CreditCard* candidate_card,
                                        const AutofillClient& client);

// Filter `cards_to_suggest` for CVC fields based on parameters such as field
// type, four digit combinations found in the DOM (if any were found), and the
// autofilled last four digits in the form.
void FilterCardsToSuggestForCvcFields(
    FieldType trigger_field_type,
    const base::flat_set<std::string>& four_digit_combinations_in_dom,
    const base::flat_set<std::u16string>&
        autofilled_last_four_digits_in_form_for_suggestion_filtering,
    std::vector<CreditCard>& cards_to_suggest);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_SUGGESTION_GENERATOR_H_
