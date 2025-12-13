// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_PAYMENTS_CREDIT_CARD_SUGGESTION_GENERATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_PAYMENTS_CREDIT_CARD_SUGGESTION_GENERATOR_H_

#include "base/functional/function_ref.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/autofill_wallet_usage_data.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/metrics/form_events/address_form_event_logger.h"
#include "components/autofill/core/browser/metrics/form_events/credit_card_form_event_logger.h"
#include "components/autofill/core/browser/metrics/payments/card_metadata_metrics.h"
#include "components/autofill/core/browser/metrics/suggestions_list_metrics.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/save_and_fill_manager.h"
#include "components/autofill/core/browser/suggestions/payments/payments_suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_generator.h"
#include "components/autofill/core/common/form_data.h"
#include "components/prefs/pref_service.h"
#include "components/sync/service/sync_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

class AutofillClient;
class FormFieldData;

// A `SuggestionGenerator` for `FillingProduct::kCreditCard`.
//
// This class encapsulates logic used exclusively for generating credit card
// suggestions. Free functions, that are also used in TouchToFill feature,
// are still shared in payments_suggestion_generator.h file.
class CreditCardSuggestionGenerator : public SuggestionGenerator {
 public:
  explicit CreditCardSuggestionGenerator(
      payments::SaveAndFillManager* save_and_fill_manager,
      autofill_metrics::CreditCardFormEventLogger* event_logger,
      AutofillMetrics::PaymentsSigninState signin_state_for_metrics_,
      const std::vector<std::string>& four_digit_combinations_in_dom);
  ~CreditCardSuggestionGenerator() override;

  void FetchSuggestionData(
      const FormData& form,
      const FormFieldData& trigger_field,
      const FormStructure* form_structure,
      const AutofillField* trigger_autofill_field,
      const AutofillClient& client,
      base::OnceCallback<
          void(std::pair<SuggestionDataSource,
                         std::vector<SuggestionGenerator::SuggestionData>>)>
          callback) override;

  void GenerateSuggestions(
      const FormData& form,
      const FormFieldData& trigger_field,
      const FormStructure* form_structure,
      const AutofillField* trigger_autofill_field,
      const AutofillClient& client,
      const base::flat_map<SuggestionDataSource, std::vector<SuggestionData>>&
          all_suggestion_data,
      base::OnceCallback<void(ReturnedSuggestions)> callback) override;

  // Like SuggestionGenerator override, but takes a base::FunctionRef instead of
  // a base::OnceCallback. Calls that callback exactly once.
  // TODO(crbug.com/409962888): Clean up after launch.
  void FetchSuggestionData(
      const FormData& form,
      const FormFieldData& trigger_field,
      const FormStructure* form_structure,
      const AutofillField* trigger_autofill_field,
      const AutofillClient& client,
      base::FunctionRef<void(std::pair<SuggestionDataSource,
                                       std::vector<SuggestionData>>)> callback);

  // Like SuggestionGenerator override, but takes a base::FunctionRef instead of
  // a base::OnceCallback. Calls that callback exactly once.
  // TODO(crbug.com/409962888): Clean up after launch.
  void GenerateSuggestions(
      const FormData& form,
      const FormFieldData& trigger_field,
      const FormStructure* form_structure,
      const AutofillField* trigger_autofill_field,
      const AutofillClient& client,
      const base::flat_map<SuggestionDataSource, std::vector<SuggestionData>>&
          all_suggestion_data,
      base::FunctionRef<void(ReturnedSuggestions)> callback);
 private:
  friend class CreditCardSuggestionGeneratorTestApi;
  // Determines whether the "Save and Fill" suggestion should be shown in the
  // credit card autofill dropdown.
  bool ShouldShowCreditCardSaveAndFill(const AutofillClient& client,
                                       bool is_complete_form,
                                       const FormFieldData& trigger_field);

  // Determines whether the `trigger_field` should show an entry to scan a
  // credit card.
  bool ShouldShowScanCreditCard(const AutofillClient& client,
                                const FormData& form,
                                const FormFieldData& trigger_field,
                                const AutofillField* autofill_field);

  // Returns a mapping of credit card guid values to virtual card last fours for
  // standalone CVC field. Cards will only be added to the returned map if they
  // have usage data on the webpage and the VCN last four was found on webpage
  // DOM.
  base::flat_map<std::string, VirtualCardUsageData::VirtualCardLastFour>
  GetVirtualCreditCardsForStandaloneCvcField(const AutofillClient& client,
                                             const url::Origin&);

  // Fetches and filters the credit cards to suggest for a `credit card` or
  // `CVC` field, based on the `trigger_field` and other form context.
  std::vector<CreditCard> FetchCreditCardsForCreditCardOrCvcField(
      const AutofillClient& client,
      const FormFieldData& trigger_field,
      const std::u16string& autofilled_last_four_digits_in_form_for_filtering,
      FieldType trigger_field_type,
      bool should_show_scan_credit_card);

  // Filter `cards_to_suggest` for CVC fields based on parameters such as field
  // type, four digit combinations found in the DOM (if any were found), and the
  // autofilled last four digits in the form.
  void FilterCardsToSuggestForCvcFields(
      FieldType trigger_field_type,
      const std::u16string& autofilled_last_four_digits_in_form_for_filtering,
      std::vector<CreditCard>& cards_to_suggest);

  // Fetches and filters virtual cards to suggest for a standalone CVC field.
  std::vector<CreditCard> FetchVirtualCardsForStandaloneCvcField(
      const AutofillClient& client,
      const FormFieldData& trigger_field,
      base::flat_map<std::string, VirtualCardUsageData::VirtualCardLastFour>&
          virtual_card_guid_to_last_four_map);

  // Generates suggestions for standalone CVC fields from the given
  // `credit_cards`.
  std::vector<Suggestion> GenerateSuggestionsForStandaloneCvcField(
      const FormFieldData& trigger_field,
      const AutofillClient& client,
      const std::vector<VirtualCardSuggestionData>& credit_cards,
      autofill_metrics::CardMetadataLoggingContext& metadata_logging_context);

  // Returns non credit card suggestions which are displayed below credit card
  // suggestions in the Autofill popup. `should_show_scan_credit_card` is used
  // to conditionally add scan credit card suggestion. `is_autofilled` is used
  // to conditionally add suggestion for clearing all autofilled fields.
  // `with_gpay_logo` is used to conditionally add GPay logo icon to the manage
  // payment methods suggestion.
  std::vector<Suggestion> GetCreditCardFooterSuggestions(
      const AutofillClient& client,
      bool should_show_bnpl_suggestion,
      bool should_show_scan_credit_card,
      bool is_autofilled,
      bool with_gpay_logo);

  // Generates suggestions for credit card or CVC fields from the given
  // `credit_cards`.
  std::vector<Suggestion> GenerateSuggestionsForCreditCardOrCvcField(
      const FormFieldData& trigger_field,
      const FieldType& trigger_field_type,
      const AutofillClient& client,
      const std::vector<CreditCard>& credit_cards,
      CreditCardSuggestionSummary& summary,
      bool should_show_scan_credit_card,
      bool is_card_number_field_empty);

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

  // Return the texts shown as the first line of the suggestion, based on the
  // `credit_card` and the `trigger_field_type`. The first index in the pair
  // represents the main text, and the second index represents the minor text.
  // The minor text can be empty, in which case the main text should be rendered
  // as the entire first line. If the minor text is not empty, they should be
  // combined. This splitting is implemented for situations where the first part
  // of the first line of the suggestion should be truncated.
  std::pair<Suggestion::Text, Suggestion::Text>
  GetSuggestionMainTextAndMinorTextForCard(const CreditCard& credit_card,
                                           const AutofillClient& client,
                                           FieldType trigger_field_type);

  // Returns true if `trigger_field_type` is a credit card expiration date
  // field.
  bool IsCreditCardExpiryData(FieldType trigger_field_type);

  // Creates a suggestion to undo the last Autofill operation or clear the form.
  // TODO(crbug.com/409962888): There are duplicates of this function all over
  // the SG logic. Move it to the util file.
  Suggestion CreateUndoOrClearFormSuggestion();

  // Returns the number of dots to use for obfuscating a credit card number in a
  // suggestion.
  int GetObfuscationLength();

  // Returns true if the card name and last four digits should be displayed as
  // separate components in the suggestion.
  bool ShouldSplitCardNameAndLastFourDigits();

  // Set the labels to be shown in the suggestion. Note that this does not
  // account for virtual cards or card-linked offers.
  // `metadata_logging_context` the instrument ids of credit cards for which
  // benefits data is available. When displaying card benefits is disabled,
  // `metadata_logging_context` will be populated but a benefit label will not
  // be shown.
  void SetSuggestionLabelsForCard(
      const CreditCard& credit_card,
      const AutofillClient& client,
      FieldType trigger_field_type,
      Suggestion& suggestion,
      autofill_metrics::CardMetadataLoggingContext& metadata_logging_context);

  // Extract card number value from the form (std::u16string) and whether that
  // value was autofilled (bool)
  std::pair<std::u16string, bool> ExtractInfoFromCardForm(
      const FormData& form,
      const FormStructure* form_structure);

  // Generates a "Save and Fill" suggestion and appends footer suggestions.
  std::vector<Suggestion> GenerateSuggestionsForSaveAndFill(
      const AutofillClient& client,
      const FormData& form,
      const FormFieldData& trigger_field,
      const AutofillField* autofill_field);

  // Adjust the content of `suggestion` if it is a virtual card suggestion.
  void AdjustVirtualCardSuggestionContent(Suggestion& suggestion,
                                          const CreditCard& credit_card,
                                          const AutofillClient& client,
                                          FieldType trigger_field_type);

  // Returns display name based on `issuer_id` in a vector.
  std::u16string GetDisplayNameForIssuerId(const std::string& issuer_id);

  // Creates a "Save and Fill" suggestion and sets `display_gpay_logo` if the
  // suggestion is for a server card.
  Suggestion CreateSaveAndFillSuggestion(const AutofillClient& client,
                                         bool& display_gpay_logo);

  // Creates and returns the IPH bubble description text for a card info
  // retrieval suggestion.
  std::u16string CreateCardInfoRetrievalIphDescriptionText(
      Suggestion suggestion);

  // This could be fetched from AutofillClient, but problems with
  // const-correctness would make us use const_cast in that case.
  // Storing SaveAndFillManager in the state is a workaround for that.
  raw_ptr<payments::SaveAndFillManager> save_and_fill_manager_;

  raw_ref<autofill_metrics::CreditCardFormEventLogger>
      credit_card_form_event_logger_;

  raw_ref<AutofillMetrics::PaymentsSigninState> signin_state_for_metrics_;

  raw_ref<const std::vector<std::string>> four_digit_combinations_in_dom_;

  base::WeakPtrFactory<CreditCardSuggestionGenerator> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_PAYMENTS_CREDIT_CARD_SUGGESTION_GENERATOR_H_
