// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_PAYMENTS_PAYMENTS_SUGGESTION_GENERATOR_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_PAYMENTS_PAYMENTS_SUGGESTION_GENERATOR_UTIL_H_

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
#include "components/autofill/core/browser/payments/amount_extraction_manager.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/common/aliases.h"

namespace autofill {

class AutofillClient;
class AutofillOfferData;
class BrowserAutofillManager;
class CreditCard;
class FormFieldData;

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
// `has_timed_out_for_page_load` indicates whether the AI amount extraction
// request timed out. If true, the returned BNPL suggestion is deactivated
// for the remainder of this page load.
// `seen_unsupported_currency_for_page_load` indicates whether the AI amount
// extraction has seen an unsupported currency. If true, the returned BNPL
// suggestion is deactivated for the remainder of this page load.
Suggestion CreateBnplSuggestion(
    std::vector<BnplIssuer> bnpl_issuers,
    std::optional<int64_t> extracted_amount_in_micros,
    const payments::AmountExtractionStatus& amount_extraction_status = {});

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

// Generates a "Save and Fill" suggestion for users who don't have any cards
// saved in Autofill. This suggestion is shown above the footer.
// `display_gpay_logo` is an  output parameter that is set to true if credit
// card upload is enabled, indicating that the GPay logo should be displayed
// with the suggestion.
Suggestion CreateSaveAndFillSuggestion(const AutofillClient& client,
                                       bool& display_gpay_logo);

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

// Helper function to decide whether to show the virtual card option for
// `candidate_card`.
bool ShouldShowVirtualCardOption(const CreditCard& candidate_card,
                                 const AutofillClient& client);

// Determines whether the "Save and Fill" suggestion should be shown in the
// credit card autofill dropdown. The suggestion is shown if all of the
// conditions are met.
bool ShouldShowCreditCardSaveAndFill(AutofillClient& client,
                                     bool is_complete_form,
                                     const FormFieldData& trigger_field);

// Returns non credit card suggestions which are displayed below credit card
// suggestions in the Autofill popup. `should_show_scan_credit_card` is used
// to conditionally add scan credit card suggestion. `is_autofilled` is used to
// conditionally add suggestion for clearing all autofilled fields.
// `should_show_pay_later_tab_suggestions` is used to append the Pay Later tab
// footnote suggestion.
// `should_append_bnpl_suggestion` is used to append a generic BNPL suggestion
// to the end of the payment methods suggestions.
// `with_gpay_logo` is used to conditionally add GPay logo icon to the manage
// payment methods suggestion.
// `has_timed_out_for_page_load` indicates whether the AI amount extraction
// request timed out. If true, the returned BNPL suggestion is deactivated for
// the remainder of this page load.
// `seen_unsupported_currency_for_page_load` indicates whether the AI amount
// extraction has seen an unsupported currency. If true, the returned BNPL
// suggestion is deactivated for the remainder of this page load.
std::vector<Suggestion> GetCreditCardFooterSuggestions(
    const AutofillClient& client,
    bool should_show_pay_later_tab_suggestions,
    bool should_append_bnpl_suggestion,
    bool should_show_scan_credit_card,
    bool is_autofilled,
    bool with_gpay_logo,
    const payments::AmountExtractionStatus& amount_extraction_status);

// Creates a suggestion for the given `credit_card`. `virtual_card_option`
// suggests whether the suggestion is a virtual card option.
// `card_linked_offer_available` indicates whether a card-linked offer is
// attached to the `credit_card`. `metadata_logging_context` contains card
// metadata related information used for metrics logging.
// TODO(crbug.com/40232456): Separate logic for desktop, Android dropdown, and
// Keyboard Accessory.
Suggestion CreateCreditCardSuggestion(
    const CreditCard& credit_card,
    const AutofillClient& client,
    FieldType trigger_field_type,
    bool virtual_card_option,
    bool card_linked_offer_available,
    autofill_metrics::CardMetadataLoggingContext& metadata_logging_context);

// Returns a mapping of credit card guid values to virtual card last fours for
// standalone CVC field. Cards will only be added to the returned map if they
// have usage data on the webpage and the VCN last four was found on webpage
// DOM.
base::flat_map<std::string, VirtualCardUsageData::VirtualCardLastFour>
GetVirtualCreditCardsForStandaloneCvcField(
    const PaymentsDataManager& data_manager,
    const url::Origin& origin,
    const std::vector<std::string>& four_digit_combinations_in_dom);

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
    bool should_show_pay_later_tab_suggestions,
    bool should_append_bnpl_suggestion,
    bool should_show_scan_credit_card,
    bool is_autofilled,
    bool with_gpay_logo,
    const payments::AmountExtractionStatus& amount_extraction_status);

// Exposes `GetBnplPriceLowerBound` in tests.
std::u16string GetBnplPriceLowerBoundForTest(
    const std::vector<BnplIssuer>& bnpl_issuers);

void SetCreditCardUploadEnabledForTest(bool credit_card_upload_enabled);

// Exposes `ShouldShowVirtualCardOption` in tests.
bool ShouldShowVirtualCardOptionForTest(const CreditCard& candidate_card,
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

// Returns the obfuscation length to be used for credit cards during suggestion
// generation.
int GetCreditCardObfuscationLength();

// Whether the `trigger_field` should show an entry to scan a credit card.
bool ShouldShowScanCreditCard(const FormStructure& form,
                              const AutofillField& trigger_field,
                              const AutofillClient& client);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_PAYMENTS_PAYMENTS_SUGGESTION_GENERATOR_UTIL_H_
