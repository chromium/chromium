// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_SUGGESTION_GENERATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_SUGGESTION_GENERATOR_H_

#include <string>
#include <vector>

#include "base/check_deref.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/data_model/autofill_wallet_usage_data.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/metrics/log_event.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/common/aliases.h"

namespace base {
class Time;
}  // namespace base

namespace autofill {

namespace autofill_metrics {
struct CardMetadataLoggingContext;
}

class AutofillClient;
class AutofillOfferData;
class CreditCard;
class FormFieldData;
class Iban;
class PaymentsDataManager;

// Helper class to generate Autofill suggestions, such as for credit card and
// address profile Autofill.
class PaymentsSuggestionGenerator {
 public:
  explicit PaymentsSuggestionGenerator(AutofillClient& autofill_client);
  ~PaymentsSuggestionGenerator();
  PaymentsSuggestionGenerator(const PaymentsSuggestionGenerator&) = delete;
  PaymentsSuggestionGenerator& operator=(const PaymentsSuggestionGenerator&) =
      delete;

  // Generates suggestions for all available credit cards based on the
  // `trigger_field_type`, `trigger_field` and `trigger_source`.
  // `with_offer` is set to true if ANY card has card-linked offers.
  // `with_cvc` is set to true if ANY card has cvc saved.
  // `metadata_logging_context` contains card metadata related information used
  // for metrics logging.
  // TODO(crbug.com/41492160): Merging out-parameters into a struct.
  std::vector<Suggestion> GetSuggestionsForCreditCards(
      const FormFieldData& trigger_field,
      FieldType trigger_field_type,
      AutofillSuggestionTriggerSource trigger_source,
      bool should_show_scan_credit_card,
      bool should_show_cards_from_account,
      bool& with_offer,
      bool& with_cvc,
      autofill_metrics::CardMetadataLoggingContext& metadata_logging_context);

  // Generates suggestions for standalone CVC fields. These only apply to
  // virtual cards that are saved on file to a merchant. In these cases,
  // we only display the virtual card option and do not show FPAN option.
  std::vector<Suggestion> GetSuggestionsForVirtualCardStandaloneCvc(
      const FormFieldData& trigger_field,
      autofill_metrics::CardMetadataLoggingContext& metadata_logging_context,
      base::flat_map<std::string, VirtualCardUsageData::VirtualCardLastFour>&
          virtual_card_guid_to_last_four_map);

  // Returns the credit cards to be shown in touch to fill suggestions.
  std::vector<CreditCard> GetTouchToFillCardsToSuggest(
      const FormFieldData& trigger_field,
      FieldType trigger_field_type);

  // Generates a separator suggestion.
  // TODO(b/41484171): Remove.
  static Suggestion CreateSeparator();

  // Generates a footer suggestion "Manage payment methods..." menu item which
  // will redirect to Chrome payment settings page. `with_gpay_logo` is used to
  // conditionally add GPay logo icon to the manage payment methods suggestion.
  //
  // The difference between `CreateManageCreditCardsEntry()` and
  // `CreateManageIbansEntry()` is that they use a different `SuggestionType`.
  // This distinction is needed for metrics recording.
  static Suggestion CreateManageCreditCardsEntry(bool with_gpay_logo);

  // Generates a footer suggestion "Manage payment methods..." menu item which
  // will redirect to Chrome payment settings page.
  //
  // The difference between `CreateManageCreditCardsEntry()` and
  // `CreateManageIbansEntry()` is that they use a different `SuggestionType`.
  // This distinction is needed for metrics recording.
  static Suggestion CreateManageIbansEntry();

  // Generate "Clear form" suggestion.
  // TODO(b/41484171): Remove.
  static Suggestion CreateClearFormSuggestion();

  // Generates suggestions for all available IBANs.
  static std::vector<Suggestion> GetSuggestionsForIbans(
      const std::vector<Iban>& ibans);

  // Converts the vector of promo code offers that is passed in to a vector of
  // suggestions that can be displayed to the user for a promo code field.
  static std::vector<Suggestion> GetPromoCodeSuggestionsFromPromoCodeOffers(
      const std::vector<const AutofillOfferData*>& promo_code_offers);

  // Return a nickname for the |card| to display. This is generally the nickname
  // stored in |card|, unless |card| exists as a local and a server copy. In
  // this case, we prefer the nickname of the local if it is defined. If only
  // one copy has a nickname, take that.
  std::u16string GetDisplayNicknameForCreditCard(const CreditCard& card) const;

  // Returns true if the suggestion created from the card is acceptable, returns
  // false when merchant does not accept the given card for example when
  // merchants opt-out of VCNs.
  bool IsCardAcceptable(const CreditCard& card, bool is_manual_fallback) const;

 private:
  friend class PaymentsSuggestionGeneratorTestApi;

  // Returns the local and server cards ordered by the Autofill ranking.
  // If `suppress_disused_cards`, local expired disused cards are removed.
  // If `prefix_match`, cards are matched with the contents of `trigger_field`.
  // If `include_virtual_cards`, virtual cards will be added when possible.
  std::vector<CreditCard> GetOrderedCardsToSuggest(
      const FormFieldData& trigger_field,
      FieldType trigger_field_type,
      bool suppress_disused_cards,
      bool prefix_match,
      bool include_virtual_cards);

  // Creates a suggestion for the given `credit_card`. `virtual_card_option`
  // suggests whether the suggestion is a virtual card option.
  // `card_linked_offer_available` indicates whether a card-linked offer is
  // attached to the `credit_card`. `metadata_logging_context` contains card
  // metadata related information used for metrics logging.
  Suggestion CreateCreditCardSuggestion(
      const CreditCard& credit_card,
      FieldType trigger_field_type,
      bool virtual_card_option,
      bool card_linked_offer_available,
      autofill_metrics::CardMetadataLoggingContext& metadata_logging_context)
      const;

  // Removes expired local credit cards not used since `min_last_used` from
  // `cards`. The relative ordering of `cards` is maintained.
  void RemoveExpiredLocalCreditCardsNotUsedSinceTimestamp(
      base::Time min_last_used,
      std::vector<CreditCard*>& cards);

  // Creates nested/child suggestions for `suggestion` with the `credit_card`
  // information. The number of nested suggestions added depends on the
  // information present in the `credit_card`.
  void AddPaymentsGranularFillingChildSuggestions(const CreditCard& credit_card,
                                                  Suggestion& suggestion) const;

  // Return the texts shown as the first line of the suggestion, based on the
  // `credit_card` and the `trigger_field_type`. The first index in the pair
  // represents the main text, and the second index represents the minor text.
  // The minor text can be empty, in which case the main text should be rendered
  // as the entire first line. If the minor text is not empty, they should be
  // combined. This splitting is implemented for situations where the first part
  // of the first line of the suggestion should be truncated.
  std::pair<Suggestion::Text, Suggestion::Text>
  GetSuggestionMainTextAndMinorTextForCard(const CreditCard& credit_card,
                                           FieldType trigger_field_type) const;

  // Set the labels to be shown in the suggestion. Note that this does not
  // account for virtual cards or card-linked offers.
  // `metadata_logging_context` the instrument ids of credit cards for which
  // benefits data is available. When displaying card benefits is disabled,
  // `metadata_logging_context` will be populated but a benefit label will not
  // be shown.
  void SetSuggestionLabelsForCard(
      const CreditCard& credit_card,
      FieldType trigger_field_type,
      autofill_metrics::CardMetadataLoggingContext& metadata_logging_context,
      Suggestion& suggestion) const;

  // Returns the benefit text to display in credit card suggestions if it is
  // available.
  std::optional<Suggestion::Text> GetCreditCardBenefitSuggestionLabel(
      const CreditCard& credit_card) const;

  // Adjust the content of `suggestion` if it is a virtual card suggestion.
  void AdjustVirtualCardSuggestionContent(Suggestion& suggestion,
                                          const CreditCard& credit_card,
                                          FieldType trigger_field_type) const;

  // Set the URL for the card art image to be shown in the `suggestion`.
  void SetCardArtURL(Suggestion& suggestion,
                     const CreditCard& credit_card,
                     bool virtual_card_option) const;

  // Returns non credit card suggestions which are displayed below credit card
  // suggestions in the Autofill popup. `should_show_scan_credit_card` is used
  // to conditionally add scan credit card suggestion,
  // `should_show_cards_from_account` - conditionally add suggestions for
  // showing cards from account. `is_autofilled` is used to conditionally add
  // suggestion for clearing all autofilled fields. `with_gpay_logo` is used to
  // conditionally add GPay logo icon to the manage payment methods suggestion.
  std::vector<Suggestion> GetCreditCardFooterSuggestions(
      bool should_show_scan_credit_card,
      bool should_show_cards_from_account,
      bool is_autofilled,
      bool with_gpay_logo) const;

  // Helper function to decide whether to show the virtual card option for
  // `candidate_card`.
  // TODO(b/326950201): Pass the argument by reference.
  bool ShouldShowVirtualCardOption(const CreditCard* candidate_card) const;

  // Returns true if we should show a virtual card option for the server card
  // `card`, false otherwise.
  bool ShouldShowVirtualCardOptionForServerCard(const CreditCard& card) const;

  const PaymentsDataManager& payments_data() const {
    // The PDM outlives the PaymentsSuggestionGenerator, hence this is safe.
    return autofill_client_->GetPersonalDataManager()->payments_data_manager();
  }

  // autofill_client_ and the generator are both one per tab, and have the same
  // lifecycle.
  raw_ref<AutofillClient> autofill_client_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_SUGGESTION_GENERATOR_H_
