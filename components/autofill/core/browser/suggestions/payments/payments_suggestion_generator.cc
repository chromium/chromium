// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/payments/payments_suggestion_generator.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_browser_util.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/autofill_offer_data.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/data_model/payments/credit_card_benefit.h"
#include "components/autofill/core/browser/data_model/payments/iban.h"
#include "components/autofill/core/browser/data_quality/autofill_data_util.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/optimization_guide/autofill_optimization_guide_decider.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/form_events/credit_card_form_event_logger.h"
#include "components/autofill/core/browser/metrics/payments/card_metadata_metrics.h"
#include "components/autofill/core/browser/metrics/payments/save_and_fill_metrics.h"
#include "components/autofill/core/browser/metrics/suggestions_list_metrics.h"
#include "components/autofill/core/browser/payments/autofill_offer_manager.h"
#include "components/autofill/core/browser/payments/bnpl_manager.h"
#include "components/autofill/core/browser/payments/bnpl_util.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/save_and_fill_manager.h"
#include "components/autofill/core/browser/studies/autofill_experiments.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace autofill {

namespace {

constexpr int64_t kCentsPerDollar = 100;
constexpr char16_t kEllipsisDotSeparator[] = u"\u2022";

Suggestion CreateUndoOrClearFormSuggestion() {
#if BUILDFLAG(IS_IOS)
  std::u16string value =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_CLEAR_FORM_MENU_ITEM);
  // TODO(crbug.com/40266549): iOS still uses Clear Form logic, replace with
  // Undo.
  Suggestion suggestion(value, SuggestionType::kUndoOrClear);
  suggestion.icon = Suggestion::Icon::kClear;
#else
  std::u16string value = l10n_util::GetStringUTF16(IDS_AUTOFILL_UNDO_MENU_ITEM);
  if constexpr (BUILDFLAG(IS_ANDROID)) {
    value = base::i18n::ToUpper(value);
  }
  Suggestion suggestion(value, SuggestionType::kUndoOrClear);
  suggestion.icon = Suggestion::Icon::kUndo;
#endif
  // TODO(crbug.com/40266549): update "Clear Form" a11y announcement to "Undo"
  suggestion.acceptance_a11y_announcement =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_A11Y_ANNOUNCE_CLEARED_FORM);
  return suggestion;
}

// The priority ranking for deduplicating a duplicate card is:
// 1. RecordType::kMaskedServerCard
// 2. RecordType::kLocalCard
std::vector<const CreditCard*> DeduplicateCreditCardsForSuggestions(
    base::span<const CreditCard* const> cards_to_suggest) {
  std::vector<const CreditCard*> deduplicated_cards;
  for (const CreditCard* card : cards_to_suggest) {
    // Full server cards should never be suggestions, as they exist only as a
    // cached state post-fill.
    CHECK_NE(card->record_type(), CreditCard::RecordType::kFullServerCard);
    // Masked server cards are preferred over their local duplicates.
    if (!CreditCard::IsLocalCard(card) ||
        std::ranges::none_of(
            cards_to_suggest, [&card](const CreditCard* other_card) {
              return card != other_card &&
                     card->IsLocalOrServerDuplicateOf(*other_card);
            })) {
      deduplicated_cards.push_back(card);
    }
  }
  return deduplicated_cards;
}

int GetObfuscationLength() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  // On Android and iOS, the obfuscation length is 2.
  return 2;
#else
  return ShouldUseNewFopDisplay() ? 2 : 4;
#endif
}

bool ShouldSplitCardNameAndLastFourDigits() {
  return !BUILDFLAG(IS_IOS);
}

// Returns whether the `suggestion_canon` is a valid match given
// `field_contents_canon`.
bool IsValidPaymentsSuggestionForFieldContents(
    std::u16string suggestion_canon,
    std::u16string field_contents_canon,
    FieldType trigger_field_type) {
  // We do not apply prefix matching to credit card numbers or CVCs to avoid
  // leaking information to the renderer - see crbug.com/338932642.
  static constexpr FieldTypeSet kFieldTypesWithoutPrefixMatching = {
      CREDIT_CARD_NUMBER, CREDIT_CARD_VERIFICATION_CODE,
      CREDIT_CARD_STANDALONE_VERIFICATION_CODE};
  if (kFieldTypesWithoutPrefixMatching.contains(trigger_field_type)) {
    return true;
  }
  return suggestion_canon.starts_with(field_contents_canon);
}

// Removes expired local credit cards not used since `min_last_used` from
// `cards`. The relative ordering of `cards` is maintained.
void RemoveExpiredLocalCreditCardsNotUsedSinceTimestamp(
    base::Time min_last_used,
    std::vector<const CreditCard*>& cards) {
  const size_t original_size = cards.size();
  std::erase_if(cards, [comparison_time = base::Time::Now(),
                        min_last_used](const CreditCard* card) {
    return card->IsExpired(comparison_time) &&
           card->usage_history().use_date() < min_last_used &&
           card->record_type() == CreditCard::RecordType::kLocalCard;
  });
  const size_t num_cards_suppressed = original_size - cards.size();
  AutofillMetrics::LogNumberOfCreditCardsSuppressedForDisuse(
      num_cards_suppressed);
}

bool IsCreditCardExpiryData(FieldType trigger_field_type) {
  switch (trigger_field_type) {
    case CREDIT_CARD_EXP_MONTH:
    case CREDIT_CARD_EXP_2_DIGIT_YEAR:
    case CREDIT_CARD_EXP_4_DIGIT_YEAR:
    case CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
    case CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR:
      return true;
    default:
      return false;
  }
}

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
                                         FieldType trigger_field_type) {
  if (IsCreditCardExpiryData(trigger_field_type) &&
      client.ShouldFormatForLargeKeyboardAccessory()) {
    // For large keyboard accessories, always show the full date regardless of
    // which expiry data related field triggered the suggestion.
    trigger_field_type = CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR;
  }

  auto create_text =
      [](std::u16string main_text,
         std::u16string minor_text =
             u"") -> std::pair<Suggestion::Text, Suggestion::Text> {
    return {Suggestion::Text(main_text, Suggestion::Text::IsPrimary(true),
                             Suggestion::Text::ShouldTruncate(
                                 ShouldSplitCardNameAndLastFourDigits())),
            // minor_text should also be shown in primary style, since it is
            // also on the first line.
            Suggestion::Text(minor_text, Suggestion::Text::IsPrimary(true))};
  };

  std::u16string nickname = GetDisplayNicknameForCreditCard(
      credit_card, client.GetPersonalDataManager().payments_data_manager());
  if (credit_card.record_type() == CreditCard::RecordType::kVirtualCard &&
      client.ShouldFormatForLargeKeyboardAccessory()) {
    return create_text(credit_card.CardNameForAutofillDisplay(nickname));
  }

  if (trigger_field_type == CREDIT_CARD_NUMBER) {
    if (ShouldUseNewFopDisplay()) {
      std::optional<std::u16string> identifier =
          credit_card.CardIdentifierForAutofillDisplay(nickname);
      if (identifier.has_value()) {
        return create_text(*identifier);
      } else {
        return create_text(
            credit_card.NetworkAndLastFourDigits(GetObfuscationLength()),
            credit_card.AbbreviatedExpirationDateForDisplay(false));
      }
    }
    if (ShouldSplitCardNameAndLastFourDigits()) {
      return create_text(credit_card.CardNameForAutofillDisplay(nickname),
                         credit_card.ObfuscatedNumberWithVisibleLastFourDigits(
                             GetObfuscationLength()));
    }
    return create_text(credit_card.CardNameAndLastFourDigits(
        nickname, GetObfuscationLength()));
  }

  if (kCvcFieldTypes.contains(trigger_field_type)) {
    CHECK(!credit_card.cvc().empty());
#if BUILDFLAG(IS_ANDROID)
    return create_text(l10n_util::GetStringFUTF16(
        IDS_AUTOFILL_CVC_SUGGESTION_MAIN_TEXT,
        credit_card.CardNameForAutofillDisplay(GetDisplayNicknameForCreditCard(
            credit_card,
            client.GetPersonalDataManager().payments_data_manager()))));
#else
    return create_text(
        l10n_util::GetStringUTF16(IDS_AUTOFILL_CVC_SUGGESTION_MAIN_TEXT));
#endif
  }

  return create_text(credit_card.GetInfo(
      trigger_field_type,
      client.GetPersonalDataManager().payments_data_manager().app_locale()));
}

#if !BUILDFLAG(IS_ANDROID)
Suggestion::Text GetBenefitTextWithTermsAppended(
    const std::u16string& benefit_text) {
  return Suggestion::Text(l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_CREDIT_CARD_BENEFIT_TEXT_FOR_SUGGESTIONS, benefit_text));
}
#endif  // !BUILDFLAG(IS_ANDROID)

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
    autofill_metrics::CardMetadataLoggingContext& metadata_logging_context,
    Suggestion& suggestion) {
  const std::string& app_locale =
      client.GetPersonalDataManager().payments_data_manager().app_locale();

  if (credit_card.record_type() == CreditCard::RecordType::kVirtualCard &&
      client.ShouldFormatForLargeKeyboardAccessory()) {
    suggestion.labels = {{Suggestion::Text(
        l10n_util::GetStringUTF16(
            IDS_AUTOFILL_VIRTUAL_CARD_SUGGESTION_OPTION_VALUE) +
        u" • " + credit_card.GetInfo(CREDIT_CARD_TYPE, app_locale) + u" " +
        credit_card.ObfuscatedNumberWithVisibleLastFourDigits(
            GetObfuscationLength()))}};
    return;
  }

  // If the focused field is a card number field.
  if (trigger_field_type == CREDIT_CARD_NUMBER) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    suggestion.labels = {{Suggestion::Text(
        credit_card.GetInfo(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, app_locale))}};
#else
    std::vector<std::vector<Suggestion::Text>> labels;

    // If the main text is the card's nickname or product description,
    // the network, last four digits, and expiration date must be displayed
    // separately in another row.
    if (ShouldUseNewFopDisplay() && !suggestion.main_text.value.empty() &&
        suggestion.minor_texts.empty()) {
      labels.push_back({Suggestion::Text(credit_card.NetworkAndLastFourDigits(
                            GetObfuscationLength())),
                        Suggestion::Text(kEllipsisDotSeparator),
                        Suggestion::Text(credit_card.GetInfo(
                            CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, app_locale))});
    }
    std::optional<Suggestion::Text> benefit_label =
        GetCreditCardBenefitSuggestionLabel(credit_card, client);
    if (benefit_label) {
      // Keep track of which cards had eligible benefits even if the
      // benefit is not displayed in the suggestion due to
      // IsCardEligibleForBenefits() == false. This is to denote a control group
      // of users with benefit-eligible cards and assess how actually
      // displaying the benefit in the experiment influences the users autofill
      // interactions.
      metadata_logging_context.instrument_ids_to_available_benefit_sources
          .insert({credit_card.instrument_id(), credit_card.benefit_source()});
      if (client.GetPersonalDataManager()
              .payments_data_manager()
              .IsCardEligibleForBenefits(credit_card)) {
        labels.push_back({*benefit_label});
        if (base::FeatureList::IsEnabled(
                features::kAutofillEnableCardBenefitsIph)) {
          suggestion.iph_metadata = Suggestion::IPHMetadata(
              &feature_engagement::kIPHAutofillCreditCardBenefitFeature);
        }
      }
    }
    if (!ShouldUseNewFopDisplay()) {
      labels.push_back({Suggestion::Text(
          ShouldSplitCardNameAndLastFourDigits()
              ? credit_card.GetInfo(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
                                    app_locale)
              : credit_card.DescriptiveExpiration(app_locale))});
    }
    suggestion.labels = std::move(labels);
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    return;
  }

  // If the focused field is not a card number field AND the card number is
  // empty (i.e. local cards added via settings page).
  std::u16string nickname = GetDisplayNicknameForCreditCard(
      credit_card, client.GetPersonalDataManager().payments_data_manager());
  if (credit_card.number().empty()) {
    DCHECK_EQ(credit_card.record_type(), CreditCard::RecordType::kLocalCard);

    if (credit_card.HasNonEmptyValidNickname()) {
      suggestion.labels = {{Suggestion::Text(nickname)}};
    } else if (trigger_field_type != CREDIT_CARD_NAME_FULL) {
      suggestion.labels = {{Suggestion::Text(
          credit_card.GetInfo(CREDIT_CARD_NAME_FULL, app_locale))}};
    }
    return;
  }

  // If the focused field is not a card number field AND the card number is NOT
  // empty.

  if constexpr (BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)) {
    if (client.ShouldFormatForLargeKeyboardAccessory()) {
      suggestion.labels = {
          {Suggestion::Text(credit_card.CardNameAndLastFourDigits(
              nickname, GetObfuscationLength()))}};
    } else {
      // On Mobile, the label is formatted as "••1234".
      suggestion.labels = {{Suggestion::Text(
          credit_card.ObfuscatedNumberWithVisibleLastFourDigits(
              GetObfuscationLength()))}};
    }
    return;
  }

  if (ShouldUseNewFopDisplay()) {
    suggestion.labels = {{Suggestion::Text(credit_card.NetworkAndLastFourDigits(
                              GetObfuscationLength())),
                          Suggestion::Text(kEllipsisDotSeparator),
                          Suggestion::Text(credit_card.GetInfo(
                              CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, app_locale))}};
    return;
  }

  if (ShouldSplitCardNameAndLastFourDigits()) {
    // Format the label as "Product Description/Nickname/Network  ••••1234".
    // If the card name is too long, it will be truncated from the tail.
    suggestion.labels = {
        {Suggestion::Text(credit_card.CardNameForAutofillDisplay(nickname),
                          Suggestion::Text::IsPrimary(false),
                          Suggestion::Text::ShouldTruncate(true)),
         Suggestion::Text(credit_card.ObfuscatedNumberWithVisibleLastFourDigits(
             GetObfuscationLength()))}};
    return;
  }

  // Format the label as
  // "Product Description/Nickname/Network  ••••1234, expires on 01/25".
  suggestion.labels = {{Suggestion::Text(
      credit_card.CardIdentifierStringAndDescriptiveExpiration(app_locale))}};
  return;
}

// Adjust the content of `suggestion` if it is a virtual card suggestion.
void AdjustVirtualCardSuggestionContent(Suggestion& suggestion,
                                        const CreditCard& credit_card,
                                        const AutofillClient& client,
                                        FieldType trigger_field_type) {
  if (credit_card.record_type() == CreditCard::RecordType::kLocalCard) {
    const CreditCard* server_duplicate_card =
        client.GetPersonalDataManager()
            .payments_data_manager()
            .GetServerCardForLocalCard(&credit_card);
    DCHECK(server_duplicate_card);
    suggestion.payload = Suggestion::Guid(server_duplicate_card->guid());
  }

  suggestion.type = SuggestionType::kVirtualCreditCardEntry;
  // If a virtual card is non-acceptable, it needs to be displayed in
  // grayed-out style.
  if (!suggestion.IsAcceptable()) {
    suggestion.acceptability =
        Suggestion::Acceptability::kUnacceptableWithDeactivatedStyle;
  }
  suggestion.iph_metadata = Suggestion::IPHMetadata(
      suggestion.HasDeactivatedStyle()
          ? &feature_engagement::
                kIPHAutofillDisabledVirtualCardSuggestionFeature
          : &feature_engagement::kIPHAutofillVirtualCardSuggestionFeature);

  // If ShouldFormatForLargeKeyboardAccessory() is true, `suggestion` has been
  // properly formatted by `SetSuggestionLabelsForCard` and does not need
  // further changes.
  if (client.ShouldFormatForLargeKeyboardAccessory()) {
    return;
  }

  // Add virtual card labelling to suggestions. For keyboard accessory, it is
  // prefixed to the suggestion, and for the dropdown, it is shown as a label on
  // a separate line.
  const std::u16string& virtual_card_label = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_VIRTUAL_CARD_SUGGESTION_OPTION_VALUE);
  const std::u16string& virtual_card_disabled_label = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_VIRTUAL_CARD_DISABLED_SUGGESTION_OPTION_VALUE);
#if BUILDFLAG(IS_IOS)
  suggestion.minor_texts = {};
  suggestion.minor_texts.emplace_back(suggestion.main_text.value);
  if (suggestion.IsAcceptable()) {
    suggestion.main_text.value = virtual_card_label;
  } else {
    suggestion.main_text.value = virtual_card_disabled_label;
  }

#elif BUILDFLAG(IS_ANDROID)
  // The keyboard accessory chips can only accommodate 2 strings which are
  // displayed on a single row. The minor_text and the labels are
  // concatenated, so we have: String 1 = main_text, String 2 = minor_text +
  // labels.
  // There is a limit on the size of the keyboard accessory chips. When the
  // suggestion content exceeds this limit, the card name or the cardholder
  // name can be truncated, the last 4 digits should never be truncated.
  // Contents in the main_text are automatically truncated from the right end
  // on the Android side when the size limit is exceeded, so the card name and
  // the cardholder name is appended to the main_text.
  // Here we modify the `Suggestion` members to make it suitable for showing
  // on the keyboard accessory.
  // Card number field:
  // Before: main_text = card name, minor_text = last 4 digits, labels =
  // expiration date.
  // After: main_text = virtual card label + card name, minor_text = last 4
  // digits, labels = null.
  // Cardholder name field:
  // Before: main_text = cardholder name, minor_text = null, labels = last 4
  // digits.
  // After: main_text = virtual card label + cardholder name, minor_text is
  // empty, labels = last 4 digits.
  if (ShouldSplitCardNameAndLastFourDigits()) {
    suggestion.main_text.value =
        base::StrCat({virtual_card_label, u"  ", suggestion.main_text.value});
  } else {
    suggestion.minor_texts = {};
    suggestion.minor_texts.emplace_back(suggestion.main_text.value);
    suggestion.main_text.value = virtual_card_label;
  }
  if (trigger_field_type == CREDIT_CARD_NUMBER) {
    // The expiration date is not shown for the card number field, so it is
    // removed.
    suggestion.labels = {};
  }
#else   // Desktop dropdown.
  // The label fields will be consistent regardless of focused field.
  if (ShouldUseNewFopDisplay() || trigger_field_type == CREDIT_CARD_NUMBER) {
    // Reset the labels as we only show benefit and virtual card label to
    // conserve space.
    suggestion.labels = {};
    std::optional<Suggestion::Text> benefit_label =
        GetCreditCardBenefitSuggestionLabel(credit_card, client);
    if (ShouldUseNewFopDisplay() && suggestion.minor_texts.empty()) {
      // minor_texts empty means that the card has either nickname or
      // product description, so add network and last four digits as a
      // separate label.
      suggestion.labels = {{Suggestion::Text(
          credit_card.NetworkAndLastFourDigits(GetObfuscationLength()))}};
    }
    if (benefit_label && client.GetPersonalDataManager()
                             .payments_data_manager()
                             .IsCardEligibleForBenefits(credit_card)) {
      // For the new-FOP display feature, when the merchant opts out
      // (that is, the suggestion is not acceptable), the benefit is not
      // shown because a merchant opt-out message will be displayed instead.
      if (!ShouldUseNewFopDisplay() || suggestion.IsAcceptable()) {
        suggestion.labels.push_back({*benefit_label});
      }
    }
  }
  // For the new-FOP display feature, a virtual card label will not be added
  // as it will be shown as a badge.
  if (!ShouldUseNewFopDisplay() && suggestion.IsAcceptable()) {
    suggestion.labels.push_back(
        std::vector<Suggestion::Text>{Suggestion::Text(virtual_card_label)});
  }
  if (!suggestion.IsAcceptable()) {
    suggestion.labels.push_back(std::vector<Suggestion::Text>{
        Suggestion::Text(virtual_card_disabled_label)});
  }
#endif  // BUILDFLAG(IS_IOS)
}

// Returns non credit card suggestions which are displayed below credit card
// suggestions in the Autofill popup. `should_show_scan_credit_card` is used
// to conditionally add scan credit card suggestion. `is_autofilled` is used to
// conditionally add suggestion for clearing all autofilled fields.
// `should_show_bnpl_suggestion` is used to conditionally append a BNPL
// suggestion to the end of the payment methods suggestions.
// `with_gpay_logo` is used to conditionally add GPay logo icon to the manage
// payment methods suggestion.
std::vector<Suggestion> GetCreditCardFooterSuggestions(
    const AutofillClient& client,
    bool should_show_bnpl_suggestion,
    bool should_show_scan_credit_card,
    bool is_autofilled,
    bool with_gpay_logo) {
  std::vector<Suggestion> footer_suggestions;

  // TODO(crbug.com/444684996): Add another check to not show BNPL chip anymore
  // for this transaction if the previous amount extraction is timeout.
  if (should_show_bnpl_suggestion) {
    if (base::FeatureList::IsEnabled(
            features::
                kAutofillEnableBuyNowPayLaterUpdatedSuggestionSecondLineString)) {
      footer_suggestions.emplace_back(SuggestionType::kSeparator);
    }

    footer_suggestions.push_back(
        CreateBnplSuggestion(client.GetPersonalDataManager()
                                 .payments_data_manager()
                                 .GetBnplIssuers(),
                             /*extracted_amount_in_micros=*/std::nullopt));
  }

  if (should_show_scan_credit_card) {
    Suggestion scan_credit_card(
        l10n_util::GetStringUTF16(IDS_AUTOFILL_SCAN_CREDIT_CARD),
        SuggestionType::kScanCreditCard);
    scan_credit_card.icon = Suggestion::Icon::kScanCreditCard;
    footer_suggestions.push_back(scan_credit_card);
  }
  footer_suggestions.emplace_back(SuggestionType::kSeparator);
  if (is_autofilled) {
    footer_suggestions.push_back(CreateUndoOrClearFormSuggestion());
  }
  footer_suggestions.push_back(
      CreateManageCreditCardsSuggestion(with_gpay_logo));
  return footer_suggestions;
}

// Returns a mapping of credit card guid values to virtual card last fours for
// standalone CVC field. Cards will only be added to the returned map if they
// have usage data on the webpage and the VCN last four was found on webpage
// DOM.
base::flat_map<std::string, VirtualCardUsageData::VirtualCardLastFour>
GetVirtualCreditCardsForStandaloneCvcField(
    const PaymentsDataManager& data_manager,
    const url::Origin& origin,
    const std::vector<std::string>& four_digit_combinations_in_dom) {
  base::flat_map<std::string, VirtualCardUsageData::VirtualCardLastFour>
      virtual_card_guid_to_last_four_map;

  base::span<const VirtualCardUsageData> usage_data =
      data_manager.GetVirtualCardUsageData();
  for (const CreditCard* credit_card : data_manager.GetCreditCards()) {
    // As we only provide virtual card suggestions for standalone CVC fields,
    // check if the card is an enrolled virtual card.
    if (credit_card->virtual_card_enrollment_state() !=
        CreditCard::VirtualCardEnrollmentState::kEnrolled) {
      continue;
    }

    auto matches_card_and_origin = [&](VirtualCardUsageData ud) {
      return ud.instrument_id().value() == credit_card->instrument_id() &&
             ud.merchant_origin() == origin;
    };

    // If `credit_card` has eligible usage data on `origin`, check if the last
    // four digits of `credit_card`'s number occur in the DOM.
    if (auto it = std::ranges::find_if(usage_data, matches_card_and_origin);
        it != usage_data.end()) {
      VirtualCardUsageData::VirtualCardLastFour virtual_card_last_four =
          it->virtual_card_last_four();
      if (base::Contains(four_digit_combinations_in_dom,
                         base::UTF16ToUTF8(virtual_card_last_four.value()))) {
        // Card has usage data on webpage and last four is present in DOM.
        virtual_card_guid_to_last_four_map[credit_card->guid()] =
            virtual_card_last_four;
      }
    }
  }
  return virtual_card_guid_to_last_four_map;
}

// Returns display name based on `issuer_id` in a vector.
std::u16string GetDisplayNameForIssuerId(const std::string& issuer_id) {
  if (issuer_id == "paypay") {
    return u"PayPay";
  }
  return u"";
}

#if BUILDFLAG(IS_ANDROID)
std::u16string CreateCardInfoRetrievalIphDescriptionText(
    Suggestion suggestion) {
  std::u16string description_text;
  if (!suggestion.iph_metadata.iph_params.empty() &&
      !suggestion.iph_metadata.iph_params.front().empty()) {
    description_text = l10n_util::GetStringFUTF16(
        IDS_AUTOFILL_CARD_INFO_RETRIEVAL_SUGGESTION_IPH_BUBBLE_LABEL,
        suggestion.iph_metadata.iph_params.front());
  } else {
    description_text = l10n_util::GetStringUTF16(
        IDS_AUTOFILL_CARD_INFO_RETRIEVAL_SUGGESTION_IPH_BUBBLE_FALLBACK_LABEL);
  }

  return description_text;
}
#endif  // BUILDFLAG(IS_ANDROID)

// Helper function to decide whether to show the virtual card option for
// `candidate_card`.
// TODO(crbug.com/326950201): Pass the argument by reference.
bool ShouldShowVirtualCardOption(const CreditCard* candidate_card,
                                 const AutofillClient& client) {
  const CreditCard* candidate_server_card = nullptr;
  switch (candidate_card->record_type()) {
    case CreditCard::RecordType::kLocalCard:
      candidate_server_card = client.GetPersonalDataManager()
                                  .payments_data_manager()
                                  .GetServerCardForLocalCard(candidate_card);
      break;
    case CreditCard::RecordType::kMaskedServerCard:
      candidate_server_card = candidate_card;
      break;
    case CreditCard::RecordType::kFullServerCard:
    case CreditCard::RecordType::kVirtualCard:
      // Should not happen since virtual cards and full server cards are not
      // persisted.
      NOTREACHED();
  }
  if (!candidate_server_card) {
    return false;
  }
  candidate_card = candidate_server_card;

  // Virtual card suggestion is shown only when the card is enrolled into
  // virtual cards.
  return candidate_card->virtual_card_enrollment_state() ==
         CreditCard::VirtualCardEnrollmentState::kEnrolled;
}


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
    autofill_metrics::CardMetadataLoggingContext& metadata_logging_context) {
  Suggestion suggestion(SuggestionType::kCreditCardEntry);
  suggestion.icon = credit_card.CardIconForAutofillSuggestion();
  suggestion.acceptability = IsCardSuggestionAcceptable(credit_card, client)
                                 ? Suggestion::Acceptability::kAcceptable
                                 : Suggestion::Acceptability::kUnacceptable;
  suggestion.payload = Suggestion::Guid(credit_card.guid());

  // Manual fallback suggestions labels are computed as if the triggering field
  // type was the credit card number.
  auto [main_text, minor_text] = GetSuggestionMainTextAndMinorTextForCard(
      credit_card, client, trigger_field_type);
  suggestion.main_text = std::move(main_text);
  if (!minor_text.value.empty()) {
    if (ShouldUseNewFopDisplay()) {
      suggestion.minor_texts.emplace_back(kEllipsisDotSeparator,
                                          Suggestion::Text::IsPrimary(true));
    }
    suggestion.minor_texts.emplace_back(std::move(minor_text));
  }
  SetSuggestionLabelsForCard(credit_card, client, trigger_field_type,
                             metadata_logging_context, suggestion);
  SetCardArtURL(suggestion, credit_card,
                client.GetPersonalDataManager().payments_data_manager(),
                virtual_card_option);

  // For server card, show card info retrieval enrolled suggestion for card info
  // retrieval enrolled card.
  if (credit_card.card_info_retrieval_enrollment_state() ==
      CreditCard::CardInfoRetrievalEnrollmentState::kRetrievalEnrolled) {
    suggestion.iph_metadata = Suggestion::IPHMetadata(
        &feature_engagement::kIPHAutofillCardInfoRetrievalSuggestionFeature);
    suggestion.iph_metadata.iph_params = {
        GetDisplayNameForIssuerId(credit_card.issuer_id())};
#if BUILDFLAG(IS_ANDROID)
    suggestion.iph_description_text =
        CreateCardInfoRetrievalIphDescriptionText(suggestion);
#endif  // BUILDFLAG(IS_ANDROID)
  }

  // For virtual cards, make some adjustments for the suggestion contents.
  if (virtual_card_option) {
    // We don't show card linked offers for virtual card options.
    AdjustVirtualCardSuggestionContent(suggestion, credit_card, client,
                                       trigger_field_type);
  } else if (card_linked_offer_available) {
#if BUILDFLAG(IS_ANDROID)
    // For Keyboard Accessory, set Suggestion::iph_metadata and change the
    // suggestion icon only if card linked offers are also enabled.
    if (base::FeatureList::IsEnabled(
            features::kAutofillEnableOffersInClankKeyboardAccessory)) {
      suggestion.iph_metadata = Suggestion::IPHMetadata(
          &feature_engagement::kIPHKeyboardAccessoryPaymentOfferFeature);
      suggestion.icon = Suggestion::Icon::kOfferTag;
    } else {
#else   // Add the offer label on Desktop unconditionally.
    {
#endif  // BUILDFLAG(IS_ANDROID)
      suggestion.labels.push_back(
          std::vector<Suggestion::Text>{Suggestion::Text(
              l10n_util::GetStringUTF16(IDS_AUTOFILL_OFFERS_CASHBACK))});
    }
  }

  if (virtual_card_option) {
    suggestion.acceptance_a11y_announcement = l10n_util::GetStringUTF16(
        IDS_AUTOFILL_A11Y_ANNOUNCE_FILLED_CARD_INFORMATION_ENTRY);
  } else {
    suggestion.acceptance_a11y_announcement =
        l10n_util::GetStringUTF16(IDS_AUTOFILL_A11Y_ANNOUNCE_FILLED_FORM);
  }

  return suggestion;
}

// Returns the lowest eligible price in all `bnpl_issuers`.
std::u16string GetBnplPriceLowerBound(
    const std::vector<BnplIssuer>& bnpl_issuers) {
  int64_t lower_bound = INT64_MAX;

  // Get the lowest eligible price in USD as it's the only supported currency
  // for now.
  for (const BnplIssuer& bnpl_issuer : bnpl_issuers) {
    const base::optional_ref<const BnplIssuer::EligiblePriceRange>
        eligible_price_range =
            bnpl_issuer.GetEligiblePriceRangeForCurrency("USD");
    if (eligible_price_range.has_value()) {
      lower_bound =
          std::min(lower_bound, eligible_price_range.value().price_lower_bound);
    }
  }

  if (lower_bound < 0) {
    lower_bound = 0;
  }

  // Suggestion update shouldn't be triggered if there is no matching
  // `eligible_price_range`.
  CHECK(lower_bound != INT64_MAX);

  // Round the lower_bound to the nearest higher cents.
  if (int64_t remainder = lower_bound % (kMicrosPerDollar / kCentsPerDollar);
      remainder != 0) {
    lower_bound = lower_bound - remainder + kMicrosPerDollar / kCentsPerDollar;
  }

  // Convert the `lower_bound` to dollars and cents format.
  // TODO(crbug.com/391699709): Add multi-currency formatter.
  int dollars = lower_bound / kMicrosPerDollar;
  int cents =
      lower_bound % kMicrosPerDollar * kCentsPerDollar / kMicrosPerDollar;
  if (cents == 0) {
    return base::StrCat({u"$", base::NumberToString16(dollars)});
  } else {
    std::u16string divider = cents < 10 ? u".0" : u".";
    return base::StrCat({u"$", base::NumberToString16(dollars), divider,
                         base::NumberToString16(cents)});
  }
}

// Determines whether the "Save and Fill" suggestion should be shown in the
// credit card autofill dropdown. The suggestion is shown if all of the
// conditions are met.
bool ShouldShowCreditCardSaveAndFill(AutofillClient& client,
                                     bool is_complete_form,
                                     const FormFieldData& trigger_field) {
  payments::SaveAndFillManager* save_and_fill_manager =
      client.GetPaymentsAutofillClient()->GetSaveAndFillManager();
  if (!save_and_fill_manager) {
    return false;
  }
  // Verify the user has no credit cards saved.
  if (!client.GetPersonalDataManager()
           .payments_data_manager()
           .GetCreditCards()
           .empty()) {
    save_and_fill_manager->MaybeLogSaveAndFillSuggestionNotShownReason(
        autofill_metrics::SaveAndFillSuggestionNotShownReason::kHasSavedCards);
    return false;
  }
  // Verify that the feature isn't blocked by the strike database. This can
  // happen when the maximum number of strikes is reached or the cooldown
  // period hasn't passed.
  if (save_and_fill_manager->ShouldBlockFeature()) {
    save_and_fill_manager->MaybeLogSaveAndFillSuggestionNotShownReason(
        autofill_metrics::SaveAndFillSuggestionNotShownReason::
            kBlockedByStrikeDatabase);
    return false;
  }
  // Verify the user is not in incognito mode.
  if (client.IsOffTheRecord()) {
    save_and_fill_manager->MaybeLogSaveAndFillSuggestionNotShownReason(
        autofill_metrics::SaveAndFillSuggestionNotShownReason::
            kUserInIncognito);
    return false;
  }
  // Verify the credit card form is complete for the purposes of "Save and
  // Fill".
  if (!is_complete_form) {
    save_and_fill_manager->MaybeLogSaveAndFillSuggestionNotShownReason(
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
}  // namespace

BnplSuggestionUpdateResult::BnplSuggestionUpdateResult() = default;
BnplSuggestionUpdateResult::BnplSuggestionUpdateResult(
    const BnplSuggestionUpdateResult&) = default;
BnplSuggestionUpdateResult::BnplSuggestionUpdateResult(
    BnplSuggestionUpdateResult&&) = default;

BnplSuggestionUpdateResult& BnplSuggestionUpdateResult::operator=(
    const BnplSuggestionUpdateResult&) = default;
BnplSuggestionUpdateResult& BnplSuggestionUpdateResult::operator=(
    BnplSuggestionUpdateResult&&) = default;

BnplSuggestionUpdateResult::~BnplSuggestionUpdateResult() = default;

std::vector<const CreditCard*> GetCreditCardsToSuggest(
    const PaymentsDataManager& payments_data_manager) {
  if (!payments_data_manager.IsAutofillPaymentMethodsEnabled()) {
    return {};
  }

  std::vector<const CreditCard*> cards_to_suggest =
      DeduplicateCreditCardsForSuggestions(
          payments_data_manager.ShouldSuggestServerPaymentMethods()
              ? payments_data_manager.GetCreditCards()
              : payments_data_manager.GetLocalCreditCards());
  // Rank the cards by ranking score (see UsageHistoryInformation for details).
  // All expired cards should be suggested last, also by ranking score.
  std::ranges::sort(
      cards_to_suggest, [comparison_time = base::Time::Now()](
                            const CreditCard* a, const CreditCard* b) {
        if (const bool a_is_expired = a->IsExpired(comparison_time);
            a_is_expired != b->IsExpired(comparison_time)) {
          return !a_is_expired;
        }
        return a->HasGreaterRankingThan(*b, comparison_time);
      });
  return cards_to_suggest;
}

// Set the URL for the card art image to be shown in the `suggestion`.
void SetCardArtURL(Suggestion& suggestion,
                   const CreditCard& credit_card,
                   const PaymentsDataManager& payments_data,
                   bool virtual_card_option) {
  const GURL card_art_url = payments_data.GetCardArtURL(credit_card);
  // The Capital One icon for virtual cards is not card metadata, it only helps
  // distinguish FPAN from virtual cards when metadata is unavailable. FPANs
  // should only ever use the network logo or rich card art. The Capital One
  // logo is reserved for virtual cards only.
  if (!virtual_card_option && card_art_url == kCapitalOneCardArtUrl) {
    return;
  }

  if constexpr (BUILDFLAG(IS_ANDROID)) {
    suggestion.custom_icon = Suggestion::CustomIconUrl(card_art_url);
  } else {
    const gfx::Image* image =
        payments_data.GetCachedCardArtImageForUrl(card_art_url);
    if (image) {
      suggestion.custom_icon = *image;
    }
  }
}

// Return a nickname for the |card| to display. This is generally the nickname
// stored in |card|, unless |card| exists as a local and a server copy. In
// this case, we prefer the nickname of the local if it is defined. If only
// one copy has a nickname, take that.
std::u16string GetDisplayNicknameForCreditCard(
    const CreditCard& card,
    const PaymentsDataManager& payments_data) {
  // Always prefer a local nickname if available.
  if (card.HasNonEmptyValidNickname() &&
      card.record_type() == CreditCard::RecordType::kLocalCard) {
    return card.nickname();
  }
  // Either the card a) has no nickname or b) is a server card and we would
  // prefer to use the nickname of a local card.
  for (const CreditCard* candidate : payments_data.GetCreditCards()) {
    if (candidate->guid() != card.guid() &&
        candidate->MatchingCardDetails(card) &&
        candidate->HasNonEmptyValidNickname()) {
      return candidate->nickname();
    }
  }
  // Fall back to nickname of |card|, which may be empty.
  return card.nickname();
}

// Returns the benefit text to display in credit card suggestions if it is
// available.
std::optional<Suggestion::Text> GetCreditCardBenefitSuggestionLabel(
    const CreditCard& credit_card,
    const AutofillClient& client) {
  const std::u16string& benefit_description =
      client.GetPersonalDataManager()
          .payments_data_manager()
          .GetApplicableBenefitDescriptionForCardAndOrigin(
              credit_card, client.GetLastCommittedPrimaryMainFrameOrigin(),
              client.GetAutofillOptimizationGuideDecider());
  if (benefit_description.empty()) {
    return std::nullopt;
  }
#if BUILDFLAG(IS_ANDROID)
  // The TTF bottom sheet displays a separate `Terms apply for card benefits`
  // message after listing all card suggestion, so it should not be appended
  // to each one like on Desktop.
  return std::optional<Suggestion::Text>(benefit_description);
#else
  return std::optional<Suggestion::Text>(
      GetBenefitTextWithTermsAppended(benefit_description));
#endif  // BUILDFLAG(IS_ANDROID)
}

std::vector<Suggestion> GetSuggestionsForCreditCards(
    AutofillClient& client,
    const FormFieldData& trigger_field,
    FieldType trigger_field_type,
    CreditCardSuggestionSummary& summary,
    bool is_complete_form,
    bool should_show_scan_credit_card,
    const std::vector<std::string>& four_digit_combinations_in_dom,
    const std::u16string& autofilled_last_four_digits_in_form_for_filtering,
    bool is_card_number_field_empty) {
  std::vector<Suggestion> suggestions;
  if (base::FeatureList::IsEnabled(features::kAutofillEnableSaveAndFill) &&
      ShouldShowCreditCardSaveAndFill(client, is_complete_form,
                                      trigger_field)) {
    bool display_gpay_logo = false;
    suggestions.push_back(
        CreateSaveAndFillSuggestion(client, display_gpay_logo));
    std::ranges::move(GetCreditCardFooterSuggestions(
                          client, /*should_show_bnpl_suggestion=*/false,
                          should_show_scan_credit_card,
                          trigger_field.is_autofilled(), display_gpay_logo),
                      std::back_inserter(suggestions));
    return suggestions;
  }
  // Only trigger GetVirtualCreditCardsForStandaloneCvcField if it's standalone
  // CVC field.
  base::flat_map<std::string, VirtualCardUsageData::VirtualCardLastFour>
      virtual_card_guid_to_last_four_map;
  if (trigger_field_type == CREDIT_CARD_STANDALONE_VERIFICATION_CODE) {
    virtual_card_guid_to_last_four_map =
        GetVirtualCreditCardsForStandaloneCvcField(
            client.GetPersonalDataManager().payments_data_manager(),
            trigger_field.origin(), four_digit_combinations_in_dom);
  }
  // Non-empty virtual_card_guid_to_last_four_map indicates this is standalone
  // CVC form AND there is matched VCN (based on the VCN usages and last four
  // from the DOM).
  if (!virtual_card_guid_to_last_four_map.empty()) {
    suggestions = GetVirtualCardStandaloneCvcFieldSuggestions(
        client, trigger_field, summary.metadata_logging_context,
        virtual_card_guid_to_last_four_map);
  } else {
    // If no virtual cards available for standalone CVC field, fall back to
    // regular credit card suggestions.
    suggestions = GetCreditCardOrCvcFieldSuggestions(
        client, trigger_field, four_digit_combinations_in_dom,
        autofilled_last_four_digits_in_form_for_filtering, trigger_field_type,
        should_show_scan_credit_card, summary, is_card_number_field_empty);
  }

  return suggestions;
}

std::vector<Suggestion> GetCreditCardOrCvcFieldSuggestions(
    const AutofillClient& client,
    const FormFieldData& trigger_field,
    const std::vector<std::string>& four_digit_combinations_in_dom,
    const std::u16string& autofilled_last_four_digits_in_form_for_filtering,
    FieldType trigger_field_type,
    bool should_show_scan_credit_card,
    CreditCardSuggestionSummary& summary,
    bool is_card_number_field_empty) {
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
      GetCardLinkedOffers(client);
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
        trigger_field_type,
        base::flat_set<std::string>(std::move(four_digit_combinations_in_dom)),
        autofilled_last_four_digits_in_form_for_filtering, cards_to_suggest);
  }

  summary.metadata_logging_context =
      autofill_metrics::GetMetadataLoggingContext(cards_to_suggest);
  std::vector<Suggestion> suggestions;
  for (const CreditCard& credit_card : cards_to_suggest) {
    Suggestion suggestion = CreateCreditCardSuggestion(
        credit_card, client, trigger_field_type,
        credit_card.record_type() == CreditCard::RecordType::kVirtualCard,
        base::Contains(card_linked_offers_map, credit_card.guid()),
        summary.metadata_logging_context);
    suggestions.push_back(suggestion);
  }

  summary.with_cvc = !std::ranges::all_of(
      cards_to_suggest, &std::u16string::empty, &CreditCard::cvc);
  summary.with_card_info_retrieval_enrolled =
      std::ranges::any_of(cards_to_suggest, [](const CreditCard& card) {
        return card.card_info_retrieval_enrollment_state() ==
               CreditCard::CardInfoRetrievalEnrollmentState::kRetrievalEnrolled;
      });
  if (suggestions.empty()) {
    return suggestions;
  }
  const bool display_gpay_logo = std::ranges::none_of(
      cards_to_suggest,
      [](const CreditCard& card) { return CreditCard::IsLocalCard(&card); });
  const bool should_show_bnpl_suggestion = payments::ShouldAppendBnplSuggestion(
      client, is_card_number_field_empty, trigger_field_type);
  std::ranges::move(
      GetCreditCardFooterSuggestions(
          client, should_show_bnpl_suggestion, should_show_scan_credit_card,
          trigger_field.is_autofilled(), display_gpay_logo),
      std::back_inserter(suggestions));
  return suggestions;
}

std::vector<Suggestion> GetVirtualCardStandaloneCvcFieldSuggestions(
    const AutofillClient& client,
    const FormFieldData& trigger_field,
    autofill_metrics::CardMetadataLoggingContext& metadata_logging_context,
    base::flat_map<std::string, VirtualCardUsageData::VirtualCardLastFour>&
        virtual_card_guid_to_last_four_map) {
  // TODO(crbug.com/40916587): Refactor credit card suggestion code by moving
  // duplicate logic to helper functions.
  std::vector<Suggestion> suggestions;
  std::vector<CreditCard> cards_to_suggest = GetOrderedCardsToSuggest(
      client, trigger_field, CREDIT_CARD_VERIFICATION_CODE,
      /*suppress_disused_cards=*/true, /*prefix_match=*/false,
      /*require_non_empty_value_on_trigger_field=*/false,
      /*include_virtual_cards=*/false);
  metadata_logging_context =
      autofill_metrics::GetMetadataLoggingContext(cards_to_suggest);

  for (const CreditCard& credit_card : cards_to_suggest) {
    auto it = virtual_card_guid_to_last_four_map.find(credit_card.guid());
    if (it == virtual_card_guid_to_last_four_map.end()) {
      continue;
    }
    const std::u16string& virtual_card_last_four = *it->second;

    Suggestion suggestion(SuggestionType::kVirtualCreditCardEntry);
    suggestion.icon = credit_card.CardIconForAutofillSuggestion();
    suggestion.payload = Suggestion::Guid(credit_card.guid());
    suggestion.iph_metadata = Suggestion::IPHMetadata(
        &feature_engagement::kIPHAutofillVirtualCardCVCSuggestionFeature);
    SetCardArtURL(suggestion, credit_card,
                  client.GetPersonalDataManager().payments_data_manager(),
                  /*virtual_card_option=*/true);
    // TODO(crbug.com/41483863): Create translation string for standalone CVC
    // suggestion which includes spacing.
    const std::u16string main_text =
        l10n_util::GetStringUTF16(
            IDS_AUTOFILL_VIRTUAL_CARD_STANDALONE_CVC_SUGGESTION_TITLE) +
        u" " +
        CreditCard::GetObfuscatedStringForCardDigits(GetObfuscationLength(),
                                                     virtual_card_last_four);
    if constexpr (BUILDFLAG(IS_ANDROID)) {
      // For Android keyboard accessory, we concatenate all the content to the
      // `main_text` to prevent the suggestion descriptor from being cut off.
      suggestion.main_text.value = base::StrCat(
          {main_text, u"  ", credit_card.CardNameForAutofillDisplay()});
    } else {
      suggestion.main_text.value = main_text;
      suggestion.labels = {
          {Suggestion::Text(credit_card.CardNameForAutofillDisplay())}};
    }
    suggestions.push_back(suggestion);
  }

  if (suggestions.empty()) {
    return suggestions;
  }

  std::ranges::move(
      GetCreditCardFooterSuggestions(
          client, /*should_show_bnpl_suggestion=*/false,
          /*should_show_scan_credit_card=*/false, trigger_field.is_autofilled(),
          /*with_gpay_logo=*/true),
      std::back_inserter(suggestions));

  return suggestions;
}

BnplSuggestionUpdateResult MaybeUpdateDesktopSuggestionsWithBnpl(
    const base::span<const Suggestion>& current_suggestions,
    std::vector<BnplIssuer> bnpl_issuers,
    int64_t extracted_amount_in_micros) {
  // No need to add BNPL suggestion if the current suggestion list is empty.
  if (current_suggestions.empty()) {
    return BnplSuggestionUpdateResult();
  }

  BnplSuggestionUpdateResult suggestion_update_result;
  suggestion_update_result.suggestions.reserve(
      current_suggestions.size() +
      (base::FeatureList::IsEnabled(
           features::
               kAutofillEnableBuyNowPayLaterUpdatedSuggestionSecondLineString)
           ? 2
           : 1));
  // Insert BNPL suggestion before the first footer item.
  for (size_t index = 0; index < current_suggestions.size(); index++) {
    // No need to add new BNPL suggestion if there is already one.
    if (current_suggestions[index].type == SuggestionType::kBnplEntry) {
      return BnplSuggestionUpdateResult();
    }

    if (IsCreditCardFooterSuggestion(current_suggestions, index)) {
      if (base::FeatureList::IsEnabled(
              features::
                  kAutofillEnableBuyNowPayLaterUpdatedSuggestionSecondLineString)) {
        suggestion_update_result.suggestions.emplace_back(
            SuggestionType::kSeparator);
      }
      suggestion_update_result.suggestions.push_back(CreateBnplSuggestion(
          std::move(bnpl_issuers), extracted_amount_in_micros));
      suggestion_update_result.suggestions.insert(
          suggestion_update_result.suggestions.end(),
          current_suggestions.begin() + index, current_suggestions.end());
      suggestion_update_result.is_bnpl_suggestion_added = true;
      return suggestion_update_result;
    }

    suggestion_update_result.suggestions.push_back(current_suggestions[index]);
  }

  // For the use cases of the BNPL flow, the `current_suggestions` should
  // always include at least one footer item if not empty.
  // Therefore, the end of the loop should never be reached.
  NOTREACHED();
}

Suggestion CreateBnplSuggestion(
    std::vector<BnplIssuer> bnpl_issuers,
    std::optional<int64_t> extracted_amount_in_micros) {
  Suggestion bnpl_suggestion(SuggestionType::kBnplEntry);
  bnpl_suggestion.icon = Suggestion::Icon::kBnpl;
  bnpl_suggestion.main_text = Suggestion::Text(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_BNPL_PAY_LATER_OPTIONS_TEXT),
      Suggestion::Text::IsPrimary(true));

  CHECK(!bnpl_issuers.empty());

  if (base::FeatureList::IsEnabled(
          features::
              kAutofillEnableBuyNowPayLaterUpdatedSuggestionSecondLineString)) {
    // Calculates the display order in the BNPL chip main text based on rank.
    // Higher rank will come before lower rank in the text.
    auto get_rank = [](BnplIssuer::IssuerId id) {
      switch (id) {
        case BnplIssuer::IssuerId::kBnplAffirm:
          return 4;
        case BnplIssuer::IssuerId::kBnplKlarna:
          return 3;
        case BnplIssuer::IssuerId::kBnplZip:
          return 2;
        case BnplIssuer::IssuerId::kBnplAfterpay:
          NOTREACHED();
      }
    };
    std::ranges::sort(
        bnpl_issuers, [&](const BnplIssuer& a, const BnplIssuer& b) {
          return get_rank(a.issuer_id()) > get_rank(b.issuer_id());
        });

    if (bnpl_issuers.size() == 1) {
      bnpl_suggestion.labels = {
          {Suggestion::Text(bnpl_issuers[0].GetDisplayName())}};
    } else if (bnpl_issuers.size() == 2) {
      bnpl_suggestion.labels = {{Suggestion::Text(l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_BNPL_CREDIT_CARD_SUGGESTION_LABEL_TWO_ISSUERS,
          bnpl_issuers[0].GetDisplayName(),
          bnpl_issuers[1].GetDisplayName()))}};
    } else if (bnpl_issuers.size() == 3) {
      bnpl_suggestion.labels = {{Suggestion::Text(l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_BNPL_CREDIT_CARD_SUGGESTION_LABEL_THREE_ISSUERS,
          bnpl_issuers[0].GetDisplayName(), bnpl_issuers[1].GetDisplayName(),
          bnpl_issuers[2].GetDisplayName()))}};
    } else {
      NOTREACHED();
    }
  } else {
    bnpl_suggestion.labels = {{Suggestion::Text(l10n_util::GetStringFUTF16(
        IDS_AUTOFILL_BNPL_CREDIT_CARD_SUGGESTION_LABEL,
        GetBnplPriceLowerBound(bnpl_issuers)))}};
  }

#if !BUILDFLAG(IS_ANDROID)
  using IssuerId = BnplIssuer::IssuerId;
  auto issuer_present = [&bnpl_issuers](IssuerId issuer_id) {
    return base::Contains(bnpl_issuers, issuer_id, &BnplIssuer::issuer_id);
  };
  bool affirm_present = issuer_present(IssuerId::kBnplAffirm);
  bool zip_present = issuer_present(IssuerId::kBnplZip);
  bool klarna_present = issuer_present(IssuerId::kBnplKlarna);

  if (affirm_present && zip_present && klarna_present &&
      base::FeatureList::IsEnabled(
          features::kAutofillEnableBuyNowPayLaterForKlarna)) {
    bnpl_suggestion.iph_metadata = Suggestion::IPHMetadata(
        &feature_engagement::
            kIPHAutofillBnplAffirmZipOrKlarnaSuggestionFeature);
  } else if (affirm_present && zip_present) {
    bnpl_suggestion.iph_metadata = Suggestion::IPHMetadata(
        &feature_engagement::kIPHAutofillBnplAffirmOrZipSuggestionFeature);
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  Suggestion::PaymentsPayload payments_payload;
  payments_payload.extracted_amount_in_micros =
      std::move(extracted_amount_in_micros);
  bnpl_suggestion.payload = std::move(payments_payload);

  return bnpl_suggestion;
}

std::vector<CreditCard> GetTouchToFillCardsToSuggest(
    const AutofillClient& client,
    const FormFieldData& trigger_field,
    FieldType trigger_field_type) {
  // TouchToFill actually has a trigger field which must be classified in some
  // way, but we intentionally fetch suggestions irrelevant of them.
  std::vector<CreditCard> cards_to_suggest = GetOrderedCardsToSuggest(
      client, trigger_field, trigger_field_type,
      /*suppress_disused_cards=*/true, /*prefix_match=*/false,
      /*require_non_empty_value_on_trigger_field=*/false,
      /*include_virtual_cards=*/true);
  return std::ranges::any_of(cards_to_suggest, &CreditCard::IsCompleteValidCard)
             ? cards_to_suggest
             : std::vector<CreditCard>();
}

std::vector<Suggestion> GetCreditCardSuggestionsForTouchToFill(
    base::span<const CreditCard> credit_cards,
    BrowserAutofillManager& manager) {
  std::vector<Suggestion> suggestions;
  suggestions.reserve(credit_cards.size());
  autofill_metrics::CardMetadataLoggingContext metadata_logging_context =
      autofill_metrics::GetMetadataLoggingContext(credit_cards);
  for (const CreditCard& credit_card : credit_cards) {
    Suggestion suggestion(credit_card.record_type() ==
                                  CreditCard::RecordType::kVirtualCard
                              ? SuggestionType::kVirtualCreditCardEntry
                              : SuggestionType::kCreditCardEntry);
    bool should_display_terms_available = false;
    std::u16string display_name = GetDisplayNicknameForCreditCard(
        credit_card,
        manager.client().GetPersonalDataManager().payments_data_manager());
    std::u16string card_name =
        credit_card.CardNameForAutofillDisplay(display_name);
    std::u16string network = base::UTF8ToUTF16(
        data_util::GetPaymentRequestData(credit_card.network())
            .basic_card_issuer_network);
    // If a card has a nickname, the network name should also be announced,
    // otherwise the name of the card will be the network name and it will be
    // announced.
    std::u16string main_text_content_description =
        base::i18n::ToLower(card_name) == base::i18n::ToLower(network)
            ? card_name
            : base::StrCat({card_name, u" ", network});
    suggestion.main_text.value = card_name;
    suggestion.minor_texts.emplace_back(
        credit_card.ObfuscatedNumberWithVisibleLastFourDigits());
    SetCardArtURL(
        suggestion, credit_card,
        manager.client().GetPersonalDataManager().payments_data_manager(),
        credit_card.record_type() == CreditCard::RecordType::kVirtualCard);
    suggestion.icon = credit_card.CardIconForAutofillSuggestion();
    std::optional<Suggestion::Text> benefit_label =
        GetCreditCardBenefitSuggestionLabel(credit_card, manager.client());
    if (benefit_label) {
      // Keep track of which cards had eligible benefits even if the
      // benefit is not displayed in the suggestion due to
      // IsCardEligibleForBenefits() == false. This helps denote a control
      // group of users with benefit-eligible cards to help determine how
      // benefit availability affects autofill usage.
      metadata_logging_context.instrument_ids_to_available_benefit_sources
          .insert({credit_card.instrument_id(), credit_card.benefit_source()});
      if (manager.client()
              .GetPersonalDataManager()
              .payments_data_manager()
              .IsCardEligibleForBenefits(credit_card)) {
        suggestion.labels.push_back({*benefit_label});
        should_display_terms_available = true;
      }
    }
    suggestion.payload = Suggestion::PaymentsPayload(
        main_text_content_description, should_display_terms_available,
        Suggestion::Guid(credit_card.guid()),
        credit_card.record_type() == CreditCard::RecordType::kLocalCard);
    if (credit_card.record_type() == CreditCard::RecordType::kVirtualCard) {
      bool acceptable =
          IsCardSuggestionAcceptable(credit_card, manager.client());
      suggestion.acceptability =
          acceptable
              ? Suggestion::Acceptability::kAcceptable
              : Suggestion::Acceptability::kUnacceptableWithDeactivatedStyle;
      suggestion.labels.push_back(std::vector<Suggestion::Text>{
          Suggestion::Text(l10n_util::GetStringUTF16(
              acceptable
                  ? IDS_AUTOFILL_VIRTUAL_CARD_SUGGESTION_OPTION_VALUE
                  : IDS_AUTOFILL_VIRTUAL_CARD_DISABLED_SUGGESTION_OPTION_VALUE))});
    } else {
      suggestion.labels.push_back(
          std::vector<Suggestion::Text>{Suggestion::Text(credit_card.GetInfo(
              CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, manager.client()
                                                     .GetPersonalDataManager()
                                                     .payments_data_manager()
                                                     .app_locale()))});
    }
    suggestions.push_back(suggestion);
  }
  if (manager.GetPaymentsBnplManager() &&
      payments::IsEligibleForBnpl(manager.client()) &&
      base::FeatureList::IsEnabled(features::kAutofillEnableAmountExtraction) &&
      base::FeatureList::IsEnabled(features::kAutofillEnableBuyNowPayLater)) {
    suggestions.reserve(suggestions.size() + 1);
    suggestions.push_back(
        CreateBnplSuggestion(/*bnpl_issuers=*/manager.client()
                                 .GetPersonalDataManager()
                                 .payments_data_manager()
                                 .GetBnplIssuers(),
                             /*extracted_amount_in_micros=*/std::nullopt));
    manager.GetCreditCardFormEventLogger().OnBnplSuggestionShown();
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
    manager.client()
        .GetPersonalDataManager()
        .payments_data_manager()
        .SetAutofillHasSeenBnpl();
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  }
  manager.GetCreditCardFormEventLogger().OnMetadataLoggingContextReceived(
      std::move(metadata_logging_context));
  return suggestions;
}

Suggestion CreateManagePaymentMethodsEntry(SuggestionType suggestion_type,
                                           bool with_gpay_logo) {
  Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_PAYMENT_METHODS),
      suggestion_type);
  // On Android and Desktop, Google Pay branding is shown along with Settings.
  // So Google Pay Icon is just attached to an existing menu item.
  if (with_gpay_logo) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    suggestion.icon = Suggestion::Icon::kGooglePay;
#else
    suggestion.icon = Suggestion::Icon::kSettings;
    suggestion.trailing_icon = Suggestion::Icon::kGooglePay;
#endif
  } else {
    suggestion.icon = Suggestion::Icon::kSettings;
  }
  return suggestion;
}

Suggestion CreateManageCreditCardsSuggestion(bool with_gpay_logo) {
  return CreateManagePaymentMethodsEntry(SuggestionType::kManageCreditCard,
                                         with_gpay_logo);
}

Suggestion CreateManageIbansSuggestion() {
  return CreateManagePaymentMethodsEntry(SuggestionType::kManageIban,
                                         /*with_gpay_logo=*/false);
}

Suggestion CreateSaveAndFillSuggestion(const AutofillClient& client,
                                       bool& display_gpay_logo) {
  Suggestion save_and_fill(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_AND_FILL_SUGGESTION_TITLE),
      SuggestionType::kSaveAndFillCreditCardEntry);
  if (client.IsCreditCardUploadEnabled()) {
    save_and_fill.labels = {{Suggestion::Text(l10n_util::GetStringUTF16(
        IDS_AUTOFILL_SERVER_SAVE_AND_FILL_SUGGESTION_DESCRIPTION))}};
    display_gpay_logo = true;
  } else {
    save_and_fill.labels = {{Suggestion::Text(l10n_util::GetStringUTF16(
        IDS_AUTOFILL_LOCAL_SAVE_AND_FILL_SUGGESTION_DESCRIPTION))}};
  }
  save_and_fill.icon = Suggestion::Icon::kSaveAndFill;
  return save_and_fill;
}

std::vector<Suggestion> GetSuggestionsForIbans(const std::vector<Iban>& ibans) {
  if (ibans.empty()) {
    return {};
  }
  std::vector<Suggestion> suggestions;
  suggestions.reserve(ibans.size() + 2);
  for (const Iban& iban : ibans) {
    Suggestion suggestion(SuggestionType::kIbanEntry);
    suggestion.custom_icon =
        ui::ResourceBundle::GetSharedInstance().GetImageNamed(
            ShouldUseNewFopDisplay() ? IDR_AUTOFILL_IBAN
                                     : IDR_AUTOFILL_IBAN_OLD);
    suggestion.icon = Suggestion::Icon::kIban;
    if (iban.record_type() == Iban::kLocalIban) {
      suggestion.payload = Suggestion::Guid(iban.guid());
    } else {
      CHECK(iban.record_type() == Iban::kServerIban);
      suggestion.payload = Suggestion::InstrumentId(iban.instrument_id());
    }

    std::u16string iban_identifier =
        iban.GetIdentifierStringForAutofillDisplay();
    if constexpr (BUILDFLAG(IS_ANDROID)) {
      // For Android keyboard accessory, the displayed value will be nickname +
      // identifier string, if the nickname is too long to fit due to bubble
      // width limitation, it will be truncated.
      if (!iban.nickname().empty()) {
        suggestion.main_text.value = iban.nickname();
        suggestion.minor_texts.emplace_back(iban_identifier);
      } else {
        suggestion.main_text.value = std::move(iban_identifier);
      }
    } else {
      if (iban.nickname().empty()) {
        suggestion.main_text = Suggestion::Text(
            iban_identifier, Suggestion::Text::IsPrimary(true));
      } else {
        suggestion.main_text = Suggestion::Text(
            iban.nickname(), Suggestion::Text::IsPrimary(true));
        suggestion.labels = {{Suggestion::Text(iban_identifier)}};
      }
    }
    suggestions.push_back(suggestion);
  }

  suggestions.emplace_back(SuggestionType::kSeparator);
  suggestions.push_back(CreateManageIbansSuggestion());
  return suggestions;
}

// static
std::vector<Suggestion> GetPromoCodeSuggestionsFromPromoCodeOffers(
    const std::vector<const AutofillOfferData*>& promo_code_offers) {
  std::vector<Suggestion> suggestions;
  GURL footer_offer_details_url;
  for (const AutofillOfferData* promo_code_offer : promo_code_offers) {
    // For each promo code, create a suggestion.
    suggestions.emplace_back(
        base::ASCIIToUTF16(promo_code_offer->GetPromoCode()),
        SuggestionType::kMerchantPromoCodeEntry);
    Suggestion& suggestion = suggestions.back();
    if (!promo_code_offer->GetDisplayStrings().value_prop_text.empty()) {
      suggestion.labels = {{Suggestion::Text(base::ASCIIToUTF16(
          promo_code_offer->GetDisplayStrings().value_prop_text))}};
    }
    suggestion.payload =
        Suggestion::Guid(base::NumberToString(promo_code_offer->GetOfferId()));

    // Every offer for a given merchant leads to the same GURL, so we grab the
    // first offer's offer details url as the payload for the footer to set
    // later.
    if (footer_offer_details_url.is_empty() &&
        !promo_code_offer->GetOfferDetailsUrl().is_empty() &&
        promo_code_offer->GetOfferDetailsUrl().is_valid()) {
      footer_offer_details_url = promo_code_offer->GetOfferDetailsUrl();
    }
  }

  // Ensure that there are suggestions and that we were able to find at least
  // one suggestion with a valid offer details url before adding the footer.
  DCHECK(suggestions.size() > 0);
  if (!footer_offer_details_url.is_empty()) {
    // Add the footer separator since we will now have a footer in the offers
    // suggestions popup.
    suggestions.emplace_back(SuggestionType::kSeparator);

    // Add the footer suggestion that navigates the user to the promo code
    // details page in the offers suggestions popup.
    suggestions.emplace_back(
        l10n_util::GetStringUTF16(
            IDS_AUTOFILL_PROMO_CODE_SUGGESTIONS_FOOTER_TEXT),
        SuggestionType::kSeePromoCodeDetails);
    Suggestion& suggestion = suggestions.back();

    // We set the payload for the footer as |footer_offer_details_url|, which is
    // the offer details url of the first offer we had for this merchant. We
    // will navigate to the url in |footer_offer_details_url| if the footer is
    // selected in AutofillExternalDelegate::DidAcceptSuggestion().
    suggestion.payload = std::move(footer_offer_details_url);
    suggestion.trailing_icon = Suggestion::Icon::kGoogle;
  }
  return suggestions;
}

bool IsCardSuggestionAcceptable(const CreditCard& card,
                                const AutofillClient& client) {
  if (card.record_type() == CreditCard::RecordType::kVirtualCard) {
    auto* optimization_guide = client.GetAutofillOptimizationGuideDecider();
    return !(
        optimization_guide &&
        optimization_guide->ShouldBlockFormFieldSuggestion(
            client.GetLastCommittedPrimaryMainFrameOrigin().GetURL(), card));
  }
  return true;
}

bool IsCreditCardFooterSuggestion(
    const base::span<const Suggestion>& suggestions,
    size_t line_number) {
  if (line_number >= suggestions.size()) {
    return false;
  }

  switch (suggestions[line_number].type) {
    case SuggestionType::kSeparator:
      // Separators are a special case: They belong into the footer iff the
      // next item exists and is a footer item.
      // Index will be checked at the beginning of every
      // IsCreditCardFooterSuggestion() call to avoid infinite recursion.
      return IsCreditCardFooterSuggestion(suggestions, line_number + 1);
    case SuggestionType::kManageCreditCard:
    case SuggestionType::kScanCreditCard:
    case SuggestionType::kUndoOrClear:
      return true;
    case SuggestionType::kAllLoyaltyCardsEntry:
    case SuggestionType::kAllSavedPasswordsEntry:
    case SuggestionType::kManageAddress:
    case SuggestionType::kManageAutofillAi:
    case SuggestionType::kManageIban:
    case SuggestionType::kManageLoyaltyCard:
    case SuggestionType::kManagePlusAddress:
    case SuggestionType::kSeePromoCodeDetails:
    case SuggestionType::kViewPasswordDetails:
    case SuggestionType::kAccountStoragePasswordEntry:
    case SuggestionType::kAddressEntry:
    case SuggestionType::kAddressEntryOnTyping:
    case SuggestionType::kAddressFieldByFieldFilling:
    case SuggestionType::kAutocompleteEntry:
    case SuggestionType::kComposeResumeNudge:
    case SuggestionType::kComposeProactiveNudge:
    case SuggestionType::kComposeDisable:
    case SuggestionType::kComposeGoToSettings:
    case SuggestionType::kComposeNeverShowOnThisSiteAgain:
    case SuggestionType::kComposeSavedStateNotification:
    case SuggestionType::kCreditCardEntry:
    case SuggestionType::kSaveAndFillCreditCardEntry:
    case SuggestionType::kDatalistEntry:
    case SuggestionType::kDevtoolsTestAddressByCountry:
    case SuggestionType::kDevtoolsTestAddressEntry:
    case SuggestionType::kDevtoolsTestAddresses:
    case SuggestionType::kFillExistingPlusAddress:
    case SuggestionType::kFillPassword:
    case SuggestionType::kGeneratePasswordEntry:
    case SuggestionType::kIbanEntry:
    case SuggestionType::kInsecureContextPaymentDisabledMessage:
    case SuggestionType::kMerchantPromoCodeEntry:
    case SuggestionType::kMixedFormMessage:
    case SuggestionType::kPasswordEntry:
    case SuggestionType::kBackupPasswordEntry:
    case SuggestionType::kTroubleSigningInEntry:
    case SuggestionType::kFreeformFooter:
    case SuggestionType::kPasswordFieldByFieldFilling:
    case SuggestionType::kTitle:
    case SuggestionType::kVirtualCreditCardEntry:
    case SuggestionType::kWebauthnCredential:
    case SuggestionType::kWebauthnSignInWithAnotherDevice:
    case SuggestionType::kIdentityCredential:
    case SuggestionType::kFillAutofillAi:
    case SuggestionType::kBnplEntry:
    case SuggestionType::kPendingStateSignin:
    case SuggestionType::kLoyaltyCardEntry:
    case SuggestionType::kOneTimePasswordEntry:
      return false;
  }
}

std::vector<CreditCard> GetOrderedCardsToSuggestForTest(
    const AutofillClient& client,
    const FormFieldData& trigger_field,
    FieldType trigger_field_type,
    bool suppress_disused_cards,
    bool prefix_match,
    bool require_non_empty_value_on_trigger_field,
    bool include_virtual_cards) {
  return GetOrderedCardsToSuggest(client, trigger_field, trigger_field_type,
                                  suppress_disused_cards, prefix_match,
                                  require_non_empty_value_on_trigger_field,
                                  include_virtual_cards);
}

Suggestion CreateCreditCardSuggestionForTest(
    const CreditCard& credit_card,
    const AutofillClient& client,
    FieldType trigger_field_type,
    bool virtual_card_option,
    bool card_linked_offer_available,
    base::optional_ref<autofill_metrics::CardMetadataLoggingContext>
        metadata_logging_context) {
  autofill_metrics::CardMetadataLoggingContext dummy_context;
  return CreateCreditCardSuggestion(
      credit_card, client, trigger_field_type, virtual_card_option,
      card_linked_offer_available,
      metadata_logging_context.has_value() ? *metadata_logging_context
                                           : dummy_context);
}

std::vector<Suggestion> GetCreditCardFooterSuggestionsForTest(
    const AutofillClient& client,
    bool should_show_bnpl_suggestion,
    bool should_show_scan_credit_card,
    bool is_autofilled,
    bool with_gpay_logo) {
  return GetCreditCardFooterSuggestions(client, should_show_bnpl_suggestion,
                                        should_show_scan_credit_card,
                                        is_autofilled, with_gpay_logo);
}

std::u16string GetBnplPriceLowerBoundForTest(
    const std::vector<BnplIssuer>& bnpl_issuers) {
  return GetBnplPriceLowerBound(bnpl_issuers);
}

bool ShouldShowVirtualCardOptionForTest(const CreditCard* candidate_card,
                                        const AutofillClient& client) {
  return ShouldShowVirtualCardOption(candidate_card, client);
}

void FilterCardsToSuggestForCvcFields(
    FieldType trigger_field_type,
    const base::flat_set<std::string>& four_digit_combinations_in_dom,
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
    std::erase_if(cards_to_suggest, [&four_digit_combinations_in_dom](
                                        const CreditCard& credit_card) {
      return !four_digit_combinations_in_dom.contains(
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
    bool include_virtual_cards) {
  std::vector<const CreditCard*> available_cards = GetCreditCardsToSuggest(
      client.GetPersonalDataManager().payments_data_manager());
  // If a card has available card linked offers on the last committed url, rank
  // it to the top.
  if (std::map<std::string, const AutofillOfferData*> card_linked_offers_map =
          GetCardLinkedOffers(client);
      !card_linked_offers_map.empty()) {
    std::ranges::stable_sort(
        available_cards,
        [&card_linked_offers_map](const CreditCard* a, const CreditCard* b) {
          return base::Contains(card_linked_offers_map, a->guid()) &&
                 !base::Contains(card_linked_offers_map, b->guid());
        });
  }
  // Suppress disused credit cards when triggered from an empty field.
  if (suppress_disused_cards) {
    const base::Time min_last_used =
        base::Time::Now() - kDisusedDataModelTimeDelta;
    RemoveExpiredLocalCreditCardsNotUsedSinceTimestamp(min_last_used,
                                                       available_cards);
  }
  std::vector<CreditCard> cards_to_suggest;
  std::u16string field_contents =
      base::i18n::ToLower(SanitizeCreditCardFieldValue(trigger_field.value()));
  for (const CreditCard* credit_card : available_cards) {
    std::u16string suggested_value = credit_card->GetInfo(
        trigger_field_type,
        client.GetPersonalDataManager().payments_data_manager().app_locale());
    if (require_non_empty_value_on_trigger_field && suggested_value.empty()) {
      continue;
    }
    if (prefix_match &&
        !IsValidPaymentsSuggestionForFieldContents(
            /*suggestion_canon=*/base::i18n::ToLower(suggested_value),
            field_contents, trigger_field_type)) {
      continue;
    }
    if (include_virtual_cards &&
        ShouldShowVirtualCardOption(credit_card, client)) {
      cards_to_suggest.push_back(CreditCard::CreateVirtualCard(*credit_card));
    }
    cards_to_suggest.push_back(*credit_card);
  }
  return cards_to_suggest;
}

// Returns the card-linked offers map with credit card guid as the key and the
// pointer to the linked AutofillOfferData as the value.
std::map<std::string, const AutofillOfferData*> GetCardLinkedOffers(
    const AutofillClient& autofill_client) {
  if (const AutofillOfferManager* offer_manager =
          autofill_client.GetPaymentsAutofillClient()
              ->GetAutofillOfferManager()) {
    return offer_manager->GetCardLinkedOffersMap(
        autofill_client.GetLastCommittedPrimaryMainFrameURL());
  }
  return {};
}

bool ShouldUseNewFopDisplay() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  return false;
#else
  return base::FeatureList::IsEnabled(
      features::kAutofillEnableNewFopDisplayDesktop);
#endif
}

}  // namespace autofill
