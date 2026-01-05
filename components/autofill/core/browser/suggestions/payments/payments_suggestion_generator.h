// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_PAYMENTS_PAYMENTS_SUGGESTION_GENERATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_PAYMENTS_PAYMENTS_SUGGESTION_GENERATOR_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/check_deref.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/types/optional_ref.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/autofill_wallet_usage_data.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/metrics/log_event.h"
#include "components/autofill/core/browser/metrics/payments/card_metadata_metrics.h"
#include "components/autofill/core/browser/metrics/suggestions_list_metrics.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/common/aliases.h"

namespace autofill {

class AutofillClient;
class AutofillOfferData;
class BrowserAutofillManager;
class CreditCard;
class FormFieldData;
class Iban;

// Describes the suggestions returned by
// `GetCreditCardOrCvcFieldSuggestions()`.
struct CreditCardSuggestionSummary {
  // True if any card has a saved CVC.
  bool with_cvc = false;
  // True if any card is card info retrieval enrolled.
  bool with_card_info_retrieval_enrolled = false;
  // Contains card metadata related information used for metrics logging.
  autofill_metrics::CardMetadataLoggingContext metadata_logging_context;
};

// Holds the result of `MaybeUpdateDesktopSuggestionsWithBnpl`.
struct BnplSuggestionUpdateResult {
  BnplSuggestionUpdateResult();

  BnplSuggestionUpdateResult(const BnplSuggestionUpdateResult&);
  BnplSuggestionUpdateResult& operator=(const BnplSuggestionUpdateResult&);
  BnplSuggestionUpdateResult(BnplSuggestionUpdateResult&&);
  BnplSuggestionUpdateResult& operator=(BnplSuggestionUpdateResult&&);

  ~BnplSuggestionUpdateResult();

  std::vector<Suggestion> suggestions;
  bool is_bnpl_suggestion_added = false;
};

// Returns the credit cards to suggest to the user. Those have been deduped
// and ordered by frecency with the expired cards put at the end of the
// vector.
std::vector<const CreditCard*> GetCreditCardsToSuggest(
    const PaymentsDataManager& payments_data_manager);

// Generates suggestions for all available credit cards based on the
// `trigger_field_type`, `trigger_field` and `four_digit_combinations_in_dom`.
// `summary` contains metadata about the returned suggestions.
// `is_complete_form` indicates whether a credit card form is considered
// complete for the purposes of "Save and Fill".
// `autofilled_last_four_digits_in_form_for_filtering` are the last four digits
// of a card number that will be used for suggestion filtering. This is used to
// avoid showing suggestions that is unrelated to the cards that have already
// been autofilled in the form. Can be empty if no filtering should happen.
// `is_card_number_field_empty` indicates whether the card number field is empty
// after the value inside of it is sanitized. This is used to decide whether the
// BNPL suggestion should be appended together with the credit card suggestions.
// TODO(crbug.com/448688721): Consolidate the input parameters.
std::vector<Suggestion> GetSuggestionsForCreditCards(
    AutofillClient& client,
    const FormFieldData& trigger_field,
    FieldType trigger_field_type,
    CreditCardSuggestionSummary& summary,
    bool is_complete_form,
    bool should_show_scan_credit_card,
    const std::vector<std::string>& four_digit_combinations_in_dom,
    const std::u16string& autofilled_last_four_digits_in_form_for_filtering,
    bool is_card_number_field_empty);

// Generates suggestions for all available credit cards based on the
// `trigger_field_type` and `trigger_field`. `summary` contains metadata about
// the returned suggestions. `last_four_set_for_cvc_suggestion_filtering` is a
// set of card number last four that will be used for suggestion filtering. This
// is used to avoid showing suggestions that is unrelated to the cards that have
// already been autofilled in the form.
// `is_card_number_field_empty` indicates whether the card number field is empty
// after the value inside of it is sanitized. This is used to decide whether the
// BNPL suggestion should be appended together with the credit card suggestions.
// TODO(crbug.com/40916587): Implement last four extraction from the DOM.
// TODO(crbug.com/448688721): Consolidate the input parameters.
std::vector<Suggestion> GetCreditCardOrCvcFieldSuggestions(
    const AutofillClient& client,
    const FormFieldData& trigger_field,
    const std::vector<std::string>& four_digit_combinations_in_dom,
    const std::u16string& autofilled_last_four_digits_in_form_for_filtering,
    FieldType trigger_field_type,
    bool should_show_scan_credit_card,
    CreditCardSuggestionSummary& summary,
    bool is_card_number_field_empty);

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

// Returns a suggestion list with a separator and BNPL suggestion, initialized
// with `bnpl_issuers` and the BNPL amount `extracted_amount_in_micros`, added
// at the end (but before footer items) of the given suggestion list
// `current_suggestions`. `BnplSuggestionUpdateResult::is_bnpl_suggestion_added`
// is true if a BNPL suggestion is inserted successfully.
BnplSuggestionUpdateResult MaybeUpdateDesktopSuggestionsWithBnpl(
    const base::span<const Suggestion>& current_suggestions,
    std::vector<BnplIssuer> bnpl_issuers,
    int64_t extracted_amount_in_micros);

// Creates a suggestion for the BNPL issuer selection. `bnpl_issuers` is used to
// set various properties of the suggestion including but not limited to the
// second line of text, as well as what IPH bubble should be shown.
// `extracted_amount_in_micros` is used to set the extracted amount in the BNPL
// suggestion to be used where necessary for the BNPL flow.
Suggestion CreateBnplSuggestion(
    std::vector<BnplIssuer> bnpl_issuers,
    std::optional<int64_t> extracted_amount_in_micros);

// Generates touch-to-fill suggestions for all available credit cards to be
// used in the bottom sheet. Benefits information, containing instrument IDs and
// issuer IDs, will be added to the `metadata_logging_context` and assigned to
// the BrowserAutofillManager's `credit_card_form_event_logger`.
std::vector<Suggestion> GetCreditCardSuggestionsForTouchToFill(
    base::span<const CreditCard> credit_cards,
    BrowserAutofillManager& manager);

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

// Generates a "Save and Fill" suggestion for users who don't have any cards
// saved in Autofill. This suggestion is shown above the footer.
// `display_gpay_logo` is an  output parameter that is set to true if credit
// card upload is enabled, indicating that the GPay logo should be displayed
// with the suggestion.
Suggestion CreateSaveAndFillSuggestion(const AutofillClient& client,
                                       bool& display_gpay_logo);

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
                                const AutofillClient& client);

// Returns `true` if the item at `line_number` is a credit card footer
// suggestion. For separators, the result is that of the next item.
bool IsCreditCardFooterSuggestion(
    const base::span<const Suggestion>& suggestions,
    size_t line_number);

// Exposes `GetOrderedCardsToSuggest` in tests.
std::vector<CreditCard> GetOrderedCardsToSuggestForTest(
    const AutofillClient& client,
    const FormFieldData& trigger_field,
    FieldType trigger_field_type,
    bool suppress_disused_cards,
    bool prefix_match,
    bool require_non_empty_value_on_trigger_field,
    bool include_virtual_cards);

// Exposes `CreateCreditCardSuggestion` in tests.
Suggestion CreateCreditCardSuggestionForTest(
    const CreditCard& credit_card,
    const AutofillClient& client,
    FieldType trigger_field_type,
    bool virtual_card_option,
    bool card_linked_offer_available,
    base::optional_ref<autofill_metrics::CardMetadataLoggingContext>
        metadata_logging_context = std::nullopt);

// Exposes `GetCreditCardFooterSuggestions` in tests.
std::vector<Suggestion> GetCreditCardFooterSuggestionsForTest(
    const AutofillClient& client,
    bool should_show_bnpl_suggestion,
    bool should_show_scan_credit_card,
    bool is_autofilled,
    bool with_gpay_logo);

// Exposes `GetBnplPriceLowerBound` in tests.
std::u16string GetBnplPriceLowerBoundForTest(
    const std::vector<BnplIssuer>& bnpl_issuers);

void SetCreditCardUploadEnabledForTest(bool credit_card_upload_enabled);

// Exposes `ShouldShowVirtualCardOption` in tests.
bool ShouldShowVirtualCardOptionForTest(const CreditCard* candidate_card,
                                        const AutofillClient& client);

// Filter `cards_to_suggest` for CVC fields based on parameters such as field
// type, four digit combinations found in the DOM (if any were found), and the
// autofilled last four digits in the form.
void FilterCardsToSuggestForCvcFields(
    FieldType trigger_field_type,
    const base::flat_set<std::string>& four_digit_combinations_in_dom,
    const std::u16string& autofilled_last_four_digits_in_form_for_filtering,
    std::vector<CreditCard>& cards_to_suggest);

// Function, shared between android touch-to-fill feature
// and CCSG.
//
// Returns the local and server cards ordered by the Autofill ranking.
// If `suppress_disused_cards`, local expired disused cards are removed.
// If `prefix_match`, cards are matched with the contents of `trigger_field`.
// If `include_virtual_cards`, virtual cards will be added when possible.
std::vector<CreditCard> GetOrderedCardsToSuggest(
    const AutofillClient& client,
    const FormFieldData& trigger_field,
    FieldType trigger_field_type,
    bool suppress_disused_cards,
    bool prefix_match,
    bool require_non_empty_value_on_trigger_field,
    bool include_virtual_cards);

// Set the URL for the card art image to be shown in the `suggestion`.
void SetCardArtURL(Suggestion& suggestion,
                   const CreditCard& credit_card,
                   const AutofillClient& client,
                   bool virtual_card_option);

// Return a nickname for the |card| to display. This is generally the nickname
// stored in |card|, unless |card| exists as a local and a server copy. In
// this case, we prefer the nickname of the local if it is defined. If only
// one copy has a nickname, take that.
std::u16string GetDisplayNicknameForCreditCard(
    const CreditCard& card,
    const PaymentsDataManager& payments_data);

// Returns the benefit text to display in credit card suggestions if it is
// available.
std::optional<Suggestion::Text> GetCreditCardBenefitSuggestionLabel(
    const CreditCard& credit_card,
    const AutofillClient& client);

Suggestion CreateManagePaymentMethodsEntry(SuggestionType suggestion_type,
                                           bool with_gpay_logo);

// Returns true if the new FOP (form-of-payment) display is enabled.
bool ShouldUseNewFopDisplay();

// Returns the card-linked offers map with credit card guid as the key and the
// pointer to the linked AutofillOfferData as the value.
std::map<std::string, const AutofillOfferData*> GetCardLinkedOffers(
    const AutofillClient& autofill_client);
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_PAYMENTS_PAYMENTS_SUGGESTION_GENERATOR_H_
