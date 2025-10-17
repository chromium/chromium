// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/payments/credit_card_suggestion_generator.h"

#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/to_vector.h"
#include "base/functional/callback.h"
#include "base/functional/function_ref.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_browser_util.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_quality/autofill_data_util.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/metrics/form_events/address_form_event_logger.h"
#include "components/autofill/core/browser/payments/autofill_offer_manager.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/studies/autofill_experiments.h"
#include "components/autofill/core/browser/suggestions/payments/payments_suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/payments/save_and_fill_suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_generator.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/credit_card_number_validation.h"
#include "components/autofill/core/common/form_data.h"
namespace autofill {

using SuggestionDataSource = SuggestionGenerator::SuggestionDataSource;

CreditCardSuggestionGenerator::CreditCardSuggestionGenerator(
    AutofillClient* client,
    const std::vector<std::string>& four_digit_combinations_in_dom,
    autofill_metrics::CreditCardFormEventLogger* credit_card_form_event_logger,
    autofill_metrics::AddressFormEventLogger* address_form_event_logger)
    : client_(client),
      four_digit_combinations_in_dom_(four_digit_combinations_in_dom) {}

CreditCardSuggestionGenerator::~CreditCardSuggestionGenerator() = default;

void CreditCardSuggestionGenerator::FetchSuggestionData(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    const AutofillClient& client,
    base::OnceCallback<
        void(std::pair<SuggestionDataSource,
                       std::vector<SuggestionGenerator::SuggestionData>>)>
        callback) {
  FetchSuggestionData(
      form, trigger_field, form_structure, trigger_autofill_field, client,
      [&callback](std::pair<SuggestionDataSource,
                            std::vector<SuggestionGenerator::SuggestionData>>
                      suggestion_data) {
        std::move(callback).Run(std::move(suggestion_data));
      });
}

void CreditCardSuggestionGenerator::GenerateSuggestions(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    const base::flat_map<SuggestionDataSource, std::vector<SuggestionData>>&
        all_suggestion_data,
    base::OnceCallback<void(ReturnedSuggestions)> callback) {
  GenerateSuggestions(
      form, trigger_field, form_structure, trigger_autofill_field,
      all_suggestion_data,
      [&callback](ReturnedSuggestions returned_suggestions) {
        std::move(callback).Run(std::move(returned_suggestions));
      });
}

void CreditCardSuggestionGenerator::FetchSuggestionData(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    const AutofillClient& client,
    base::FunctionRef<
        void(std::pair<SuggestionDataSource,
                       std::vector<SuggestionGenerator::SuggestionData>>)>
        callback) {
  if (!client_->IsAutofillPaymentMethodsEnabled()) {
    callback({SuggestionDataSource::kCreditCard, {}});
    return;
  }

  if (!form_structure || !trigger_autofill_field) {
    callback({SuggestionDataSource::kCreditCard, {}});
    return;
  }

  if (trigger_autofill_field->Type().GetTypes().contains_any(
          FieldTypesOfGroup(FieldTypeGroup::kCreditCard))) {
    callback({SuggestionDataSource::kCreditCard, {}});
    return;
  }

  if (bool is_complete_form = form_structure->IsCompleteCreditCardForm(
          FormStructure::CreditCardFormCompleteness::
              kCompleteCreditCardFormIncludingCvcAndName);
      base::FeatureList::IsEnabled(features::kAutofillEnableSaveAndFill) &&
      ShouldShowCreditCardSaveAndFill(is_complete_form, trigger_field)) {
    callback({SuggestionDataSource::kSaveAndFillPromo,
              {SaveAndFillSuggestion(true)}});
    return;
  }

  std::u16string card_number_field_value = u"";
  bool is_card_number_autofilled = false;

  // Preprocess the form to extract info about card number field.
  for (const FormFieldData& local_field : form.fields()) {
    if (const AutofillField* autofill_field =
            form_structure->GetFieldById(local_field.global_id());
        autofill_field &&
        autofill_field->Type().GetCreditCardType() == CREDIT_CARD_NUMBER) {
      card_number_field_value +=
          SanitizeCreditCardFieldValue(local_field.value());
      is_card_number_autofilled |= local_field.is_autofilled();
    }
  }

  // Offer suggestion for expiration date field if the card number field is
  // empty or the card number field is autofilled.
  if (data_util::IsCreditCardExpirationType(
          trigger_autofill_field->Type().GetCreditCardType()) &&
      !SanitizedFieldIsEmpty(card_number_field_value) &&
      !is_card_number_autofilled) {
    callback({SuggestionDataSource::kCreditCard, {}});
    return;
  }

  if (IsInAutofillSuggestionsDisabledExperiment()) {
    callback({SuggestionDataSource::kCreditCard, {}});
    return;
  }

  std::vector<CreditCard> credit_cards;

  // Only trigger GetVirtualCreditCardsForStandaloneCvcField if it's standalone
  // CVC field.
  if (trigger_autofill_field->Type().GetCreditCardType() ==
      CREDIT_CARD_STANDALONE_VERIFICATION_CODE) {
    virtual_card_guid_to_last_four_map_ =
        GetVirtualCreditCardsForStandaloneCvcField(trigger_field.origin());
  }

  SuggestionDataSource suggestion_data_source;

  // Non-empty virtual_card_guid_to_last_four_map indicates this is a standalone
  // CVC form AND there is a matched VCN (based on the VCN usages and the last
  // four from the DOM).
  if (!virtual_card_guid_to_last_four_map_.empty()) {
    suggestion_data_source = SuggestionDataSource::kVirtualStandaloneCvc;
    credit_cards = FetchVirtualCardsForStandaloneCvcField(trigger_field);
  } else {
    // If no virtual cards available for standalone CVC field, fall back to
    // regular credit card suggestions.
    suggestion_data_source = SuggestionDataSource::kCreditCard;
    std::u16string autofilled_last_four_digits_in_form_for_filtering = u"";

    if (is_card_number_autofilled && card_number_field_value.size() >= 4) {
      card_number_field_value.substr(card_number_field_value.size() - 4);
    }

    credit_cards = FetchCreditCardsForCreditCardOrCvcField(
        client, trigger_field, four_digit_combinations_in_dom_,
        autofilled_last_four_digits_in_form_for_filtering,
        trigger_autofill_field->Type().GetCreditCardType(),
        ShouldShowScanCreditCard(form, trigger_field, trigger_autofill_field));
  }

  std::vector<SuggestionData> suggestion_data = base::ToVector(
      credit_cards, [](auto& card) { return SuggestionData(std::move(card)); });

  callback({suggestion_data_source, std::move(suggestion_data)});
  return;
}

void CreditCardSuggestionGenerator::GenerateSuggestions(
    const FormData& form,
    const FormFieldData& trigger_field,
    const FormStructure* form_structure,
    const AutofillField* trigger_autofill_field,
    const base::flat_map<SuggestionDataSource, std::vector<SuggestionData>>&
        all_suggestion_data,
    base::FunctionRef<void(ReturnedSuggestions)> callback) {
  // TODO(crbug.com/409962888): will appear in the next CL chain links.
}

bool CreditCardSuggestionGenerator::ShouldShowCreditCardSaveAndFill(
    bool is_complete_form,
    const FormFieldData& trigger_field) {
  if (!save_and_fill_manager()) {
    return false;
  }
  // Verify the user has no credit cards saved.
  if (!payments_data_manager()->GetCreditCards().empty()) {
    save_and_fill_manager()->MaybeLogSaveAndFillSuggestionNotShownReason(
        autofill_metrics::SaveAndFillSuggestionNotShownReason::kHasSavedCards);
    return false;
  }

  // Verify that the feature isn't blocked by the strike database. This can
  // happen when the maximum number of strikes is reached or the cooldown
  // period hasn't passed.
  if (save_and_fill_manager()->ShouldBlockFeature()) {
    save_and_fill_manager()->MaybeLogSaveAndFillSuggestionNotShownReason(
        autofill_metrics::SaveAndFillSuggestionNotShownReason::
            kBlockedByStrikeDatabase);
    return false;
  }

  // Verify the user is not in incognito mode.
  if (client_->IsOffTheRecord()) {
    save_and_fill_manager()->MaybeLogSaveAndFillSuggestionNotShownReason(
        autofill_metrics::SaveAndFillSuggestionNotShownReason::
            kUserInIncognito);
    return false;
  }

  // Verify the credit card form is complete for the purposes of "Save and
  // Fill".
  if (!is_complete_form) {
    save_and_fill_manager()->MaybeLogSaveAndFillSuggestionNotShownReason(
        autofill_metrics::SaveAndFillSuggestionNotShownReason::
            kIncompleteCreditCardForm);
    return false;
  }

  // Verify a field within the credit card form is clicked and has no more than
  // 3 characters entered.
  if (trigger_field.value().length() > 3) {
    return false;
  }

  return true;
}

bool CreditCardSuggestionGenerator::ShouldShowScanCreditCard(
    const FormData& form,
    const FormFieldData& trigger_field,
    const AutofillField* autofill_field) {
  if (!payments_autofill_client()->HasCreditCardScanFeature() ||
      !client_->IsAutofillPaymentMethodsEnabled()) {
    return false;
  }

  bool is_card_number_field =
      autofill_field->Type().GetCreditCardType() == CREDIT_CARD_NUMBER &&
      base::ContainsOnlyChars(StripCardNumberSeparators(trigger_field.value()),
                              u"0123456789");

  if (!is_card_number_field) {
    return false;
  }
  if (IsFormNonSecure(form)) {
    return false;
  }

  static const int kShowScanCreditCardMaxValueLength = 6;
  return trigger_field.value().size() <= kShowScanCreditCardMaxValueLength;
}

// TODO(crbug.com/409962888): right now it's copied from BAM. maybe move this
// to a separate util file, common for all modules that potentially use this?
// For example crrev.com/c/7013595
bool CreditCardSuggestionGenerator::IsFormNonSecure(
    const FormData& form) const {
  // Check if testing override applies.
  if (consider_form_as_secure_for_testing_.value_or(false)) {
    return false;
  }
  return IsFormOrClientNonSecure(*client_, form);
}

// TODO(crbug.com/409962888): rename to clarify that this returns a map
base::flat_map<std::string, VirtualCardUsageData::VirtualCardLastFour>
CreditCardSuggestionGenerator::GetVirtualCreditCardsForStandaloneCvcField(
    const url::Origin& origin) {
  base::flat_map<std::string, VirtualCardUsageData::VirtualCardLastFour>
      virtual_card_guid_to_last_four_map;

  base::span<const VirtualCardUsageData> usage_data =
      payments_data_manager()->GetVirtualCardUsageData();

  for (const CreditCard* card : payments_data_manager()->GetCreditCards()) {
    // As we only provide virtual card suggestions for standalone CVC fields,
    // check if the card is an enrolled virtual card.
    if (card->virtual_card_enrollment_state() !=
        CreditCard::VirtualCardEnrollmentState::kEnrolled) {
      continue;
    }

    auto matches_card_and_origin = [&](VirtualCardUsageData ud) {
      return ud.instrument_id().value() == card->instrument_id() &&
             ud.merchant_origin() == origin;
    };

    // If `card` has eligible usage data on `origin`, check if the last
    // four digits of `card`'s number occur in the DOM.
    if (auto it = std::ranges::find_if(usage_data, matches_card_and_origin);
        it != usage_data.end()) {
      VirtualCardUsageData::VirtualCardLastFour virtual_card_last_four =
          it->virtual_card_last_four();

      if (base::Contains(four_digit_combinations_in_dom_,
                         base::UTF16ToUTF8(virtual_card_last_four.value()))) {
        // Card has usage data on webpage and last four is present in DOM.
        virtual_card_guid_to_last_four_map[card->guid()] =
            virtual_card_last_four;
      }
    }
  }

  return virtual_card_guid_to_last_four_map;
}

std::vector<CreditCard>
CreditCardSuggestionGenerator::FetchCreditCardsForCreditCardOrCvcField(
    const AutofillClient& client,
    const FormFieldData& trigger_field,
    const std::vector<std::string>& four_digit_combinations_in_dom,
    const std::u16string& autofilled_last_four_digits_in_form_for_filtering,
    FieldType trigger_field_type,
    bool should_show_scan_credit_card) {
  // Early return if CVC suggestions are triggered but the client does not
  // support CVC saving (e.g., for iOS WebView). This avoids unnecessary
  // processing, which would ultimately result in an empty suggestion list
  // anyway.
  if (kCvcFieldTypes.contains(trigger_field_type) &&
      !client.IsCvcSavingSupported()) {
    return {};
  }

  if (trigger_field_type == CREDIT_CARD_STANDALONE_VERIFICATION_CODE &&
      !base::FeatureList::IsEnabled(
          features::
              kAutofillEnableCvcStorageAndFillingStandaloneFormEnhancement)) {
    return {};
  }

  const bool allow_payment_swapping =
      trigger_field.is_autofilled() && IsPaymentsFieldSwappingEnabled();

  std::map<std::string, const AutofillOfferData*> card_linked_offers_map =
      GetCardLinkedOffers();

  bool suppress_disused_cards =
      SanitizeCreditCardFieldValue(trigger_field.value()).empty();

  bool should_prefix_match = !allow_payment_swapping;

  bool require_non_empty_value_on_trigger_field =
      !allow_payment_swapping || kCvcFieldTypes.contains(trigger_field_type);

  std::vector<CreditCard> cards_to_suggest =
      GetOrderedCardsToSuggest(client, trigger_field, trigger_field_type,
                               /*suppress_disused_cards=*/
                               suppress_disused_cards,
                               /*prefix_match=*/should_prefix_match,
                               /*require_non_empty_value_on_trigger_field=*/
                               require_non_empty_value_on_trigger_field,
                               /*include_virtual_cards=*/true);

  if (kCvcFieldTypes.contains(trigger_field_type) &&
      base::FeatureList::IsEnabled(
          features::kAutofillEnableCvcStorageAndFillingEnhancement)) {
    FilterCardsToSuggestForCvcFields(
        trigger_field_type, autofilled_last_four_digits_in_form_for_filtering,
        cards_to_suggest);
  }
  return cards_to_suggest;
}

std::map<std::string, const AutofillOfferData*>
CreditCardSuggestionGenerator::GetCardLinkedOffers() {
  if (const AutofillOfferManager* offer_manager =
          payments_autofill_client()->GetAutofillOfferManager()) {
    return offer_manager->GetCardLinkedOffersMap(
        client_->GetLastCommittedPrimaryMainFrameURL());
  }
  return {};
}

void CreditCardSuggestionGenerator::FilterCardsToSuggestForCvcFields(
    FieldType trigger_field_type,
    const std::u16string& autofilled_last_four_digits_in_form_for_filtering,
    std::vector<CreditCard>& cards_to_suggest) {
  if (trigger_field_type ==
          FieldType::CREDIT_CARD_STANDALONE_VERIFICATION_CODE &&
      base::FeatureList::IsEnabled(
          features::
              kAutofillEnableCvcStorageAndFillingStandaloneFormEnhancement)) {
    // For standalone CVC fields, there is no form to fill and thus filter based
    // on, so the filtering mechanism used to show the correct suggestion(s) is
    // matching the last four digits in the DOM to the last four digits of the
    // cards that can be displayed.
    std::erase_if(cards_to_suggest, [this](const CreditCard& credit_card) {
      return !base::Contains(this->four_digit_combinations_in_dom_,
                             base::UTF16ToUTF8(credit_card.LastFourDigits()));
    });
  } else {
    // `autofilled_last_four_digits_in_form_for_filtering` being empty implies
    // no card was autofilled, show all suggestions.
    if (autofilled_last_four_digits_in_form_for_filtering.empty()) {
      return;
    }
    std::erase_if(cards_to_suggest,
                  [&autofilled_last_four_digits_in_form_for_filtering](
                      const CreditCard& credit_card) {
                    return autofilled_last_four_digits_in_form_for_filtering !=
                           credit_card.LastFourDigits();
                  });
  }
}

std::vector<CreditCard>
CreditCardSuggestionGenerator::FetchVirtualCardsForStandaloneCvcField(
    const FormFieldData& trigger_field) {
  std::vector<CreditCard> credit_cards = GetOrderedCardsToSuggest(
      *client_, trigger_field, CREDIT_CARD_VERIFICATION_CODE,
      /*suppress_disused_cards=*/true, /*prefix_match=*/false,
      /*require_non_empty_value_on_trigger_field=*/false,
      /*include_virtual_cards=*/false);

  // From the general list of cards, remove any that are not a virtual card
  // relevant to the current standalone CVC field.
  std::erase_if(credit_cards, [&](const CreditCard& credit_card) {
    return !virtual_card_guid_to_last_four_map_.contains(credit_card.guid());
  });

  return credit_cards;
}

}  // namespace autofill
