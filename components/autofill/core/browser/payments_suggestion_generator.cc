// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments_suggestion_generator.h"

#include <functional>
#include <string>
#include <vector>

#include "base/check_deref.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_browser_util.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_optimization_guide.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/credit_card_benefit.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/payments/card_metadata_metrics.h"
#include "components/autofill/core/browser/metrics/suggestions_list_metrics.h"
#include "components/autofill/core/browser/payments/autofill_offer_manager.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/common/autofill_clock.h"
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

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
#include "ui/native_theme/native_theme.h"  // nogncheck
#endif

namespace autofill {

namespace {

constexpr FieldTypeSet kCvcFieldTypes = {
    FieldType::CREDIT_CARD_VERIFICATION_CODE,
    FieldType::CREDIT_CARD_STANDALONE_VERIFICATION_CODE};
constexpr FieldTypeGroupSet kCreditCardAndCvcGroups = {
    FieldTypeGroup::kCreditCard, FieldTypeGroup::kStandaloneCvcField};

Suggestion CreateSeparator() {
  Suggestion suggestion;
  suggestion.type = SuggestionType::kSeparator;
  return suggestion;
}

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

// Returns the card-linked offers map with credit card guid as the key and the
// pointer to the linked AutofillOfferData as the value.
std::map<std::string, AutofillOfferData*> GetCardLinkedOffers(
    const AutofillClient& autofill_client) {
  if (const AutofillOfferManager* offer_manager =
          autofill_client.GetPaymentsAutofillClient()
              ->GetAutofillOfferManager()) {
    return offer_manager->GetCardLinkedOffersMap(
        autofill_client.GetLastCommittedPrimaryMainFrameURL());
  }
  return {};
}

int GetObfuscationLength() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  // On Android and iOS, the obfuscation length is 2.
  return 2;
#else
  return 4;
#endif
}

bool ShouldSplitCardNameAndLastFourDigits() {
#if BUILDFLAG(IS_IOS)
  return false;
#else
  return base::FeatureList::IsEnabled(
             features::kAutofillEnableVirtualCardMetadata) &&
         base::FeatureList::IsEnabled(features::kAutofillEnableCardProductName);
#endif
}

// Adds nested entry to the `suggestion` for filling credit card cardholder name
// if the `credit_card` has the corresponding info is set.
bool AddCreditCardNameChildSuggestion(const CreditCard& credit_card,
                                      const std::string& app_locale,
                                      Suggestion& suggestion) {
  if (!credit_card.HasInfo(CREDIT_CARD_NAME_FULL)) {
    return false;
  }
  Suggestion cc_name(credit_card.GetInfo(CREDIT_CARD_NAME_FULL, app_locale),
                     SuggestionType::kCreditCardFieldByFieldFilling);
  // TODO(crbug.com/40146355): Use instrument ID for server credit cards.
  cc_name.payload = Suggestion::Guid(credit_card.guid());
  cc_name.field_by_field_filling_type_used = CREDIT_CARD_NAME_FULL;
  suggestion.children.push_back(std::move(cc_name));
  return true;
}

// Adds nested entry to the `suggestion` for filling credit card number if the
// `credit_card` has the corresponding info is set.
bool AddCreditCardNumberChildSuggestion(const CreditCard& credit_card,
                                        const std::string& app_locale,
                                        Suggestion& suggestion) {
  if (!credit_card.HasInfo(CREDIT_CARD_NUMBER)) {
    return false;
  }
  static constexpr int kFieldByFieldObfuscationLength = 12;
  Suggestion cc_number(credit_card.ObfuscatedNumberWithVisibleLastFourDigits(
                           kFieldByFieldObfuscationLength),
                       SuggestionType::kCreditCardFieldByFieldFilling);
  // TODO(crbug.com/40146355): Use instrument ID for server credit cards.
  cc_number.payload = Suggestion::Guid(credit_card.guid());
  cc_number.field_by_field_filling_type_used = CREDIT_CARD_NUMBER;
  cc_number.labels.push_back({Suggestion::Text(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_PAYMENTS_MANUAL_FALLBACK_AUTOFILL_POPUP_CC_NUMBER_SUGGESTION_LABEL))});
  suggestion.children.push_back(std::move(cc_number));
  return true;
}

// Adds nested entry to the `suggestion` for filling credit card number expiry
// date. The added entry has 2 nested entries for filling credit card expiry
// year and month.
void AddCreditCardExpiryDateChildSuggestion(const CreditCard& credit_card,
                                            const std::string& app_locale,
                                            Suggestion& suggestion) {
  Suggestion cc_expiration(
      credit_card.GetInfo(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, app_locale),
      SuggestionType::kCreditCardFieldByFieldFilling);
  // TODO(crbug.com/40146355): Use instrument ID for server credit cards.
  cc_expiration.payload = Suggestion::Guid(credit_card.guid());
  cc_expiration.field_by_field_filling_type_used =
      CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR;
  cc_expiration.labels.push_back({Suggestion::Text(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_PAYMENTS_MANUAL_FALLBACK_AUTOFILL_POPUP_CC_EXPIRY_DATE_SUGGESTION_LABEL))});

  Suggestion cc_expiration_month(
      credit_card.GetInfo(CREDIT_CARD_EXP_MONTH, app_locale),
      SuggestionType::kCreditCardFieldByFieldFilling);
  // TODO(crbug.com/40146355): Use instrument ID for server credit cards.
  cc_expiration_month.payload = Suggestion::Guid(credit_card.guid());
  cc_expiration_month.field_by_field_filling_type_used = CREDIT_CARD_EXP_MONTH;
  cc_expiration_month.labels.push_back({Suggestion::Text(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_PAYMENTS_MANUAL_FALLBACK_AUTOFILL_POPUP_CC_EXPIRY_MONTH_SUGGESTION_LABEL))});

  Suggestion cc_expiration_year(
      credit_card.GetInfo(CREDIT_CARD_EXP_2_DIGIT_YEAR, app_locale),
      SuggestionType::kCreditCardFieldByFieldFilling);
  // TODO(crbug.com/40146355): Use instrument ID for server credit cards.
  cc_expiration_year.payload = Suggestion::Guid(credit_card.guid());
  cc_expiration_year.field_by_field_filling_type_used =
      CREDIT_CARD_EXP_2_DIGIT_YEAR;
  cc_expiration_year.labels.push_back({Suggestion::Text(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_PAYMENTS_MANUAL_FALLBACK_AUTOFILL_POPUP_CC_EXPIRY_YEAR_SUGGESTION_LABEL))});

  cc_expiration.children.push_back(std::move(cc_expiration_month));
  cc_expiration.children.push_back(std::move(cc_expiration_year));
  suggestion.children.push_back(std::move(cc_expiration));
}

// Returns whether the `suggestion_canon` is a valid match given
// `field_contents_canon`. To be used for payments suggestions.
bool IsValidPaymentsSuggestionForFieldContents(
    std::u16string suggestion_canon,
    std::u16string field_contents_canon,
    FieldType trigger_field_type,
    bool is_masked_server_card,
    bool field_is_autofilled) {
  // If `kAutofillDontPrefixMatchCreditCardNumbersOrCvcs` is enabled, we do not
  // apply prefix matching to credit card numbers or CVCs.
  static constexpr FieldTypeSet kFieldTypesWithoutPrefixMatching = {
      CREDIT_CARD_NUMBER, CREDIT_CARD_VERIFICATION_CODE,
      CREDIT_CARD_STANDALONE_VERIFICATION_CODE};
  if (kFieldTypesWithoutPrefixMatching.contains(trigger_field_type) &&
      base::FeatureList::IsEnabled(
          features::kAutofillDontPrefixMatchCreditCardNumbersOrCvcs)) {
    return true;
  }

  if (trigger_field_type != CREDIT_CARD_NUMBER) {
    return suggestion_canon.starts_with(field_contents_canon);
  }

  // If `kAutofillDontPrefixMatchCreditCardNumbersOrCvcs` is disabled, we
  // suggest a card iff
  // - the number matches any part of the card, or
  // - it's a masked card and there are 6 or fewer typed so far.
  // - it's a masked card, field is autofilled, and the last 4 digits of the
  //   field match the last 4 digits of the card.
  if (suggestion_canon.find(field_contents_canon) != std::u16string::npos) {
    return true;
  }
  if (!is_masked_server_card) {
    return false;
  }
  return field_contents_canon.size() < 6 ||
         (field_is_autofilled &&
          suggestion_canon.find(field_contents_canon.substr(
              field_contents_canon.size() - 4, field_contents_canon.size())) !=
              std::u16string::npos);
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
    suggestion.trailing_icon =
        ui::NativeTheme::GetInstanceForNativeUi()->ShouldUseDarkColors()
            ? Suggestion::Icon::kGooglePayDark
            : Suggestion::Icon::kGooglePay;
#endif
  } else {
    suggestion.icon = Suggestion::Icon::kSettings;
  }
  return suggestion;
}

// Removes expired local credit cards not used since `min_last_used` from
// `cards`. The relative ordering of `cards` is maintained.
void RemoveExpiredLocalCreditCardsNotUsedSinceTimestamp(
    base::Time min_last_used,
    std::vector<CreditCard*>& cards) {
  const size_t original_size = cards.size();
  std::erase_if(cards, [comparison_time = AutofillClock::Now(),
                        min_last_used](const CreditCard* card) {
    return card->IsExpired(comparison_time) &&
           card->use_date() < min_last_used &&
           card->record_type() == CreditCard::RecordType::kLocalCard;
  });
  const size_t num_cards_suppressed = original_size - cards.size();
  AutofillMetrics::LogNumberOfCreditCardsSuppressedForDisuse(
      num_cards_suppressed);
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

// Creates nested/child suggestions for `suggestion` with the `credit_card`
// information. The number of nested suggestions added depends on the
// information present in the `credit_card`.
void AddPaymentsGranularFillingChildSuggestions(const CreditCard& credit_card,
                                                Suggestion& suggestion,
                                                const std::string& app_locale) {
  bool has_content_above =
      AddCreditCardNameChildSuggestion(credit_card, app_locale, suggestion);
  has_content_above |=
      AddCreditCardNumberChildSuggestion(credit_card, app_locale, suggestion);

  if (credit_card.HasInfo(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR)) {
    if (has_content_above) {
      suggestion.children.push_back(CreateSeparator());
    }
    AddCreditCardExpiryDateChildSuggestion(credit_card, app_locale, suggestion);
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
      credit_card, client.GetPersonalDataManager()->payments_data_manager());
  if (credit_card.record_type() == CreditCard::RecordType::kVirtualCard &&
      client.ShouldFormatForLargeKeyboardAccessory()) {
    return create_text(credit_card.CardNameForAutofillDisplay(nickname));
  }

  if (trigger_field_type == CREDIT_CARD_NUMBER) {
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
            client.GetPersonalDataManager()->payments_data_manager()))));
#else
    return create_text(
        l10n_util::GetStringUTF16(IDS_AUTOFILL_CVC_SUGGESTION_MAIN_TEXT));
#endif
  }

  return create_text(credit_card.GetInfo(
      trigger_field_type,
      client.GetPersonalDataManager()->payments_data_manager().app_locale()));
}

#if !BUILDFLAG(IS_ANDROID)
Suggestion::Text GetBenefitTextWithTermsAppended(
    const std::u16string& benefit_text) {
  return Suggestion::Text(l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_CREDIT_CARD_BENEFIT_TEXT_FOR_SUGGESTIONS, benefit_text));
}
#endif  // !BUILDFLAG(IS_ANDROID)

// Returns the benefit text to display in credit card suggestions if it is
// available.
std::optional<Suggestion::Text> GetCreditCardBenefitSuggestionLabel(
    const CreditCard& credit_card,
    const AutofillClient& client) {
  const std::u16string& benefit_description =
      client.GetPersonalDataManager()
          ->payments_data_manager()
          .GetApplicableBenefitDescriptionForCardAndOrigin(
              credit_card, client.GetLastCommittedPrimaryMainFrameOrigin(),
              client.GetAutofillOptimizationGuide());
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
      client.GetPersonalDataManager()->payments_data_manager().app_locale();

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
    std::optional<Suggestion::Text> benefit_label =
        GetCreditCardBenefitSuggestionLabel(credit_card, client);
    if (benefit_label) {
      // Keep track of which cards had eligible benefits even if the
      // benefit is not displayed in the suggestion due to
      // IsCardEligibleForBenefits() == false. This is to denote a control group
      // of users with benefit-eligible cards and assess how actually
      // displaying the benefit in the experiment influences the users autofill
      // interactions.
      metadata_logging_context
          .instrument_ids_to_issuer_ids_with_benefits_available.insert(
              {credit_card.instrument_id(), credit_card.issuer_id()});
      if (client.GetPersonalDataManager()
              ->payments_data_manager()
              .IsCardEligibleForBenefits(credit_card)) {
        labels.push_back({*benefit_label});
      }
      if (base::FeatureList::IsEnabled(
              features::kAutofillEnableCardBenefitsIph)) {
        suggestion.feature_for_iph =
            &feature_engagement::kIPHAutofillCreditCardBenefitFeature;
      }
    }
    labels.push_back({Suggestion::Text(
        ShouldSplitCardNameAndLastFourDigits()
            ? credit_card.GetInfo(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, app_locale)
            : credit_card.DescriptiveExpiration(app_locale))});
    suggestion.labels = std::move(labels);
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    return;
  }

  // If the focused field is not a card number field AND the card number is
  // empty (i.e. local cards added via settings page).
  std::u16string nickname = GetDisplayNicknameForCreditCard(
      credit_card, client.GetPersonalDataManager()->payments_data_manager());
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
            ->payments_data_manager()
            .GetServerCardForLocalCard(&credit_card);
    DCHECK(server_duplicate_card);
    suggestion.payload = Suggestion::Guid(server_duplicate_card->guid());
  }

  suggestion.type = SuggestionType::kVirtualCreditCardEntry;
  // If a virtual card is non-acceptable, it needs to be displayed in
  // grayed-out style.
  suggestion.apply_deactivated_style = !suggestion.is_acceptable;
  suggestion.feature_for_iph =
      suggestion.apply_deactivated_style &&
              base::FeatureList::IsEnabled(
                  features::kAutofillEnableVcnGrayOutForMerchantOptOut)
          ? &feature_engagement::
                kIPHAutofillDisabledVirtualCardSuggestionFeature
          : &feature_engagement::kIPHAutofillVirtualCardSuggestionFeature;

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
  if (!base::FeatureList::IsEnabled(
          features::kAutofillEnableVirtualCardMetadata)) {
    suggestion.minor_text.value = suggestion.main_text.value;
    if (suggestion.is_acceptable) {
      suggestion.main_text.value = virtual_card_label;
    } else {
      suggestion.main_text.value = virtual_card_disabled_label;
    }
  } else {
#if BUILDFLAG(IS_ANDROID)
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
    // After: main_text = virtual card label + cardholder name, minor_text =
    // null, labels = last 4 digits.
    if (ShouldSplitCardNameAndLastFourDigits()) {
      suggestion.main_text.value =
          base::StrCat({virtual_card_label, u"  ", suggestion.main_text.value});
    } else {
      suggestion.minor_text.value = suggestion.main_text.value;
      suggestion.main_text.value = virtual_card_label;
    }
    if (trigger_field_type == CREDIT_CARD_NUMBER) {
      // The expiration date is not shown for the card number field, so it is
      // removed.
      suggestion.labels = {};
    }
#else   // Desktop/Android dropdown.
    if (trigger_field_type == CREDIT_CARD_NUMBER) {
      // Reset the labels as we only show benefit and virtual card label to
      // conserve space.
      suggestion.labels = {};
      std::optional<Suggestion::Text> benefit_label =
          GetCreditCardBenefitSuggestionLabel(credit_card, client);
      if (benefit_label && client.GetPersonalDataManager()
                               ->payments_data_manager()
                               .IsCardEligibleForBenefits(credit_card)) {
        suggestion.labels.push_back({*benefit_label});
      }
    }
    if (suggestion.is_acceptable) {
      suggestion.labels.push_back(
          std::vector<Suggestion::Text>{Suggestion::Text(virtual_card_label)});
    } else {
      suggestion.labels.push_back(std::vector<Suggestion::Text>{
          Suggestion::Text(virtual_card_disabled_label)});
    }
#endif  // BUILDFLAG(IS_ANDROID)
  }
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
  // Only show card art if the experiment is enabled or if it is the Capital One
  // virtual card icon.
  if (base::FeatureList::IsEnabled(features::kAutofillEnableCardArtImage) ||
      card_art_url == kCapitalOneCardArtUrl) {
    if constexpr (BUILDFLAG(IS_ANDROID)) {
      suggestion.custom_icon = Suggestion::CustomIconUrl(card_art_url);
    } else {
      gfx::Image* image =
          payments_data.GetCreditCardArtImageForUrl(card_art_url);
      if (image) {
        suggestion.custom_icon = *image;
      }
    }
  }
}

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
    bool with_gpay_logo) {
  std::vector<Suggestion> footer_suggestions;
  if (should_show_scan_credit_card) {
    Suggestion scan_credit_card(
        l10n_util::GetStringUTF16(IDS_AUTOFILL_SCAN_CREDIT_CARD),
        SuggestionType::kScanCreditCard);
    scan_credit_card.icon = Suggestion::Icon::kScanCreditCard;
    footer_suggestions.push_back(scan_credit_card);
  }
  if (should_show_cards_from_account) {
    Suggestion show_card_from_account(
        l10n_util::GetStringUTF16(IDS_AUTOFILL_SHOW_ACCOUNT_CARDS),
        SuggestionType::kShowAccountCards);
    show_card_from_account.icon = Suggestion::Icon::kGoogle;
    footer_suggestions.push_back(show_card_from_account);
  }
  footer_suggestions.push_back(CreateSeparator());
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

// Returns true if we should show a virtual card option for the server card
// `card`, false otherwise.
bool ShouldShowVirtualCardOptionForServerCard(const CreditCard& card,
                                              const AutofillClient& client) {
  // If the card is not enrolled into virtual cards, we should not show a
  // virtual card suggestion for it.
  if (card.virtual_card_enrollment_state() !=
      CreditCard::VirtualCardEnrollmentState::kEnrolled) {
    return false;
  }
  // We should not show a suggestion for this card if the autofill
  // optimization guide returns that this suggestion should be blocked.
  if (auto* autofill_optimization_guide =
          client.GetAutofillOptimizationGuide()) {
    return !autofill_optimization_guide->ShouldBlockFormFieldSuggestion(
               client.GetLastCommittedPrimaryMainFrameOrigin().GetURL(),
               card) ||
           base::FeatureList::IsEnabled(
               features::kAutofillEnableVcnGrayOutForMerchantOptOut);
  }
  // No conditions to prevent displaying a virtual card suggestion were
  // found, so return true.
  return true;
}

// Helper function to decide whether to show the virtual card option for
// `candidate_card`.
// TODO(crbug.com/326950201): Pass the argument by reference.
bool ShouldShowVirtualCardOption(const CreditCard* candidate_card,
                                 const AutofillClient& client) {
  const CreditCard* candidate_server_card = nullptr;
  switch (candidate_card->record_type()) {
    case CreditCard::RecordType::kLocalCard:
      candidate_server_card = client.GetPersonalDataManager()
                                  ->payments_data_manager()
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
  return ShouldShowVirtualCardOptionForServerCard(*candidate_server_card,
                                                  client);
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
    bool include_virtual_cards,
    bool use_legacy_algorithm = false) {
  std::vector<CreditCard*> available_cards =
      client.GetPersonalDataManager()
          ->payments_data_manager()
          .GetCreditCardsToSuggest(use_legacy_algorithm);
  // If a card has available card linked offers on the last committed url, rank
  // it to the top.
  if (std::map<std::string, AutofillOfferData*> card_linked_offers_map =
          GetCardLinkedOffers(client);
      !card_linked_offers_map.empty()) {
    base::ranges::stable_sort(
        available_cards,
        [&card_linked_offers_map](const CreditCard* a, const CreditCard* b) {
          return base::Contains(card_linked_offers_map, a->guid()) &&
                 !base::Contains(card_linked_offers_map, b->guid());
        });
  }
  // Suppress disused credit cards when triggered from an empty field.
  if (suppress_disused_cards) {
    const base::Time min_last_used =
        AutofillClock::Now() - kDisusedDataModelTimeDelta;
    RemoveExpiredLocalCreditCardsNotUsedSinceTimestamp(min_last_used,
                                                       available_cards);
  }
  std::vector<CreditCard> cards_to_suggest;
  std::u16string field_contents =
      base::i18n::ToLower(SanitizeCreditCardFieldValue(trigger_field.value()));
  for (const CreditCard* credit_card : available_cards) {
    std::u16string suggested_value = credit_card->GetInfo(
        trigger_field_type,
        client.GetPersonalDataManager()->payments_data_manager().app_locale());
    if (require_non_empty_value_on_trigger_field && suggested_value.empty()) {
      continue;
    }
    if (prefix_match &&
        !IsValidPaymentsSuggestionForFieldContents(
            /*suggestion_canon=*/base::i18n::ToLower(suggested_value),
            field_contents, trigger_field_type,
            credit_card->record_type() ==
                CreditCard::RecordType::kMaskedServerCard,
            trigger_field.is_autofilled())) {
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
  // Manual fallback entries are shown for all non credit card fields.
  const bool is_manual_fallback = !kCreditCardAndCvcGroups.contains(
      GroupTypeOfFieldType(trigger_field_type));

  Suggestion suggestion;
  suggestion.icon = credit_card.CardIconForAutofillSuggestion();
  // First layer manual fallback entries can't fill forms and thus can't be
  // selected by the user.
  suggestion.type = SuggestionType::kCreditCardEntry;
  suggestion.is_acceptable =
      IsCardSuggestionAcceptable(credit_card, client, is_manual_fallback);
  suggestion.payload = Suggestion::Guid(credit_card.guid());
#if BUILDFLAG(IS_ANDROID)
  // The card art icon should always be shown at the start of the suggestion.
  suggestion.is_icon_at_start = true;
#endif  // BUILDFLAG(IS_ANDROID)

  // Manual fallback suggestions labels are computed as if the triggering field
  // type was the credit card number.
  auto [main_text, minor_text] = GetSuggestionMainTextAndMinorTextForCard(
      credit_card, client,
      is_manual_fallback ? CREDIT_CARD_NUMBER : trigger_field_type);
  suggestion.main_text = std::move(main_text);
  suggestion.minor_text = std::move(minor_text);
  SetSuggestionLabelsForCard(
      credit_card, client,
      is_manual_fallback ? CREDIT_CARD_NUMBER : trigger_field_type,
      metadata_logging_context, suggestion);
  SetCardArtURL(suggestion, credit_card,
                client.GetPersonalDataManager()->payments_data_manager(),
                virtual_card_option);

  // For virtual cards, make some adjustments for the suggestion contents.
  if (virtual_card_option) {
    // We don't show card linked offers for virtual card options.
    AdjustVirtualCardSuggestionContent(suggestion, credit_card, client,
                                       trigger_field_type);
  } else if (card_linked_offer_available) {
#if BUILDFLAG(IS_ANDROID)
    // For Keyboard Accessory, set Suggestion::feature_for_iph and change the
    // suggestion icon only if card linked offers are also enabled.
    if (base::FeatureList::IsEnabled(
            features::kAutofillEnableOffersInClankKeyboardAccessory)) {
      suggestion.feature_for_iph =
          &feature_engagement::kIPHKeyboardAccessoryPaymentOfferFeature;
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
        IDS_AUTOFILL_A11Y_ANNOUNCE_VIRTUAL_CARD_MANUAL_FALLBACK_ENTRY);
  } else if (is_manual_fallback) {
    AddPaymentsGranularFillingChildSuggestions(
        credit_card, suggestion,
        client.GetPersonalDataManager()->payments_data_manager().app_locale());
    suggestion.acceptance_a11y_announcement = l10n_util::GetStringUTF16(
        IDS_AUTOFILL_A11Y_ANNOUNCE_EXPANDABLE_ONLY_ENTRY);
  } else {
    suggestion.acceptance_a11y_announcement =
        l10n_util::GetStringUTF16(IDS_AUTOFILL_A11Y_ANNOUNCE_FILLED_FORM);
  }

  return suggestion;
}

}  // namespace

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
        autofilled_last_four_digits_in_form_for_suggestion_filtering) {
  // Only trigger GetVirtualCreditCardsForStandaloneCvcField if it's standalone
  // CVC field.
  base::flat_map<std::string, VirtualCardUsageData::VirtualCardLastFour>
      virtual_card_guid_to_last_four_map;
  if (trigger_field_type == CREDIT_CARD_STANDALONE_VERIFICATION_CODE) {
    virtual_card_guid_to_last_four_map =
        GetVirtualCreditCardsForStandaloneCvcField(
            client.GetPersonalDataManager()->payments_data_manager(),
            trigger_field.origin(), four_digit_combinations_in_dom);
  }
  // Non-empty virtual_card_guid_to_last_four_map indicates this is standalone
  // CVC form AND there is matched VCN (based on the VCN usages and last four
  // from the DOM).
  std::vector<Suggestion> suggestions;
  if (!virtual_card_guid_to_last_four_map.empty()) {
    suggestions = GetVirtualCardStandaloneCvcFieldSuggestions(
        client, trigger_field, summary.metadata_logging_context,
        virtual_card_guid_to_last_four_map);
  } else {
    // If no virtual cards available for standalone CVC field, fall back to
    // regular credit card suggestions.
    suggestions = GetCreditCardOrCvcFieldSuggestions(
        client, trigger_field, four_digit_combinations_in_dom,
        autofilled_last_four_digits_in_form_for_suggestion_filtering,
        trigger_field_type, trigger_source, should_show_scan_credit_card,
        should_show_cards_from_account, summary);
  }

  return suggestions;
}

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
    CreditCardSuggestionSummary& summary) {
  if (trigger_field_type == CREDIT_CARD_STANDALONE_VERIFICATION_CODE &&
      !base::FeatureList::IsEnabled(
          features::
              kAutofillEnableCvcStorageAndFillingStandaloneFormEnhancement)) {
    return {};
  }
  const bool is_trigger_field_a_credit_card_field =
      kCreditCardAndCvcGroups.contains(
          GroupTypeOfFieldType(trigger_field_type));

  const bool allow_payment_swapping =
      base::FeatureList::IsEnabled(
          features::kAutofillEnablePaymentsFieldSwapping) &&
      trigger_field.is_autofilled();

  std::map<std::string, AutofillOfferData*> card_linked_offers_map =
      GetCardLinkedOffers(client);
  summary.with_offer = !card_linked_offers_map.empty();
  bool suppress_disused_cards =
      SanitizeCreditCardFieldValue(trigger_field.value()).empty() &&
      trigger_source !=
          AutofillSuggestionTriggerSource::kManualFallbackPayments;
  bool should_prefix_match =
      is_trigger_field_a_credit_card_field && !allow_payment_swapping;
  bool require_non_empty_value_on_trigger_field =
      (is_trigger_field_a_credit_card_field && !allow_payment_swapping) ||
      kCvcFieldTypes.contains(trigger_field_type);
  std::vector<CreditCard> cards_to_suggest =
      GetOrderedCardsToSuggest(client, trigger_field, trigger_field_type,
                               /*suppress_disused_cards=*/
                               suppress_disused_cards,
                               /*prefix_match=*/should_prefix_match,
                               /*require_non_empty_value_on_trigger_field=*/
                               require_non_empty_value_on_trigger_field,
                               /*include_virtual_cards=*/true);

  if (kCvcFieldTypes.contains(trigger_field_type) &&
      trigger_source !=
          AutofillSuggestionTriggerSource::kManualFallbackPayments &&
      base::FeatureList::IsEnabled(
          features::kAutofillEnableCvcStorageAndFillingEnhancement)) {
    FilterCardsToSuggestForCvcFields(
        trigger_field_type,
        base::flat_set<std::string>(std::move(four_digit_combinations_in_dom)),
        base::flat_set<std::u16string>(std::move(
            autofilled_last_four_digits_in_form_for_suggestion_filtering)),
        cards_to_suggest);
  }

  // If autofill for cards is triggered from the context menu on a credit card
  // field and no suggestions can be shown (i.e. if a user has only cards
  // without names and then triggers autofill from the context menu on a card
  // name field), then default to the same behaviour as if the user triggers
  // autofill for card on a non-payments field. This is done to avoid a
  // situation when the user would trigger autofill from the context menu, and
  // then no suggestions appear.
  // The "if condition" is satisfied only if `trigger_field_type` is a credit
  // card field. Then, `GetCreditCardOrCvcFieldSuggestions()` is called with
  // `UNKOWN_TYPE` for the `trigger_field_type`. This guarantees no infinite
  // recursion occurs.
  if (cards_to_suggest.empty() && is_trigger_field_a_credit_card_field &&
      trigger_source ==
          AutofillSuggestionTriggerSource::kManualFallbackPayments &&
      base::FeatureList::IsEnabled(
          features::kAutofillForUnclassifiedFieldsAvailable)) {
    return GetCreditCardOrCvcFieldSuggestions(
        client, trigger_field, four_digit_combinations_in_dom,
        autofilled_last_four_digits_in_form_for_suggestion_filtering,
        UNKNOWN_TYPE, trigger_source, should_show_scan_credit_card,
        should_show_cards_from_account, summary);
  }
  bool new_ranking_experiment_enabled = base::FeatureList::IsEnabled(
      features::kAutofillEnableRankingFormulaCreditCards);
  std::vector<CreditCard> cards_ranked_by_legacy_algorithm;
  if (new_ranking_experiment_enabled) {
    // Get credit cards ranked by legacy algorithm to use for comparison with
    // the new algorithm's rankings inside the loop below.
    cards_ranked_by_legacy_algorithm = GetOrderedCardsToSuggest(
        client, trigger_field, trigger_field_type, suppress_disused_cards,
        /*prefix_match=*/should_prefix_match,
        /*require_non_empty_value_on_trigger_field=*/
        require_non_empty_value_on_trigger_field,
        /*include_virtual_cards=*/true, /*use_legacy_algorithm=*/true);
  }
  summary.metadata_logging_context =
      autofill_metrics::GetMetadataLoggingContext(cards_to_suggest);
  std::vector<Suggestion> suggestions;
  for (size_t current_card_index = 0;
       current_card_index < cards_to_suggest.size(); current_card_index++) {
    const CreditCard& credit_card = cards_to_suggest[current_card_index];
    Suggestion suggestion = CreateCreditCardSuggestion(
        credit_card, client, trigger_field_type,
        credit_card.record_type() == CreditCard::RecordType::kVirtualCard,
        base::Contains(card_linked_offers_map, credit_card.guid()),
        summary.metadata_logging_context);
    suggestions.push_back(suggestion);

    if (new_ranking_experiment_enabled) {
      // Find the ranking of the card in the old and new algorithm and
      // mark if they are ranked higher, lower, or the same.
      size_t ranking_legacy_algorithm =
          base::ranges::find(cards_ranked_by_legacy_algorithm, credit_card) -
          cards_ranked_by_legacy_algorithm.begin();
      autofill_metrics::SuggestionRankingContext::RelativePosition
          ranking_difference = autofill_metrics::SuggestionRankingContext::
              GetRelativePositionEnum(ranking_legacy_algorithm,
                                      current_card_index);

      summary.ranking_context.suggestion_rankings_difference_map.insert(
          {suggestion.GetBackendId<Suggestion::Guid>(), ranking_difference});
    }
  }
  summary.with_cvc = !std::ranges::all_of(
      cards_to_suggest, &std::u16string::empty, &CreditCard::cvc);
  if (suggestions.empty()) {
    return suggestions;
  }
  const bool display_gpay_logo = std::ranges::none_of(
      cards_to_suggest,
      [](const CreditCard& card) { return CreditCard::IsLocalCard(&card); });
  base::ranges::move(
      GetCreditCardFooterSuggestions(
          should_show_scan_credit_card, should_show_cards_from_account,
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

    Suggestion suggestion;
    suggestion.icon = credit_card.CardIconForAutofillSuggestion();
    suggestion.type = SuggestionType::kVirtualCreditCardEntry;
    suggestion.payload = Suggestion::Guid(credit_card.guid());
    suggestion.feature_for_iph =
        &feature_engagement::kIPHAutofillVirtualCardCVCSuggestionFeature;
    SetCardArtURL(suggestion, credit_card,
                  client.GetPersonalDataManager()->payments_data_manager(),
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

  base::ranges::move(
      GetCreditCardFooterSuggestions(/*should_show_scan_credit_card=*/false,
                                     /*should_show_cards_from_account=*/false,
                                     trigger_field.is_autofilled(),
                                     /*with_gpay_logo=*/true),
      std::back_inserter(suggestions));

  return suggestions;
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
    const AutofillClient& client) {
  std::vector<Suggestion> suggestions;
  suggestions.reserve(credit_cards.size());
  for (const CreditCard& credit_card : credit_cards) {
    Suggestion suggestion;
    std::u16string nickname = GetDisplayNicknameForCreditCard(
        credit_card, client.GetPersonalDataManager()->payments_data_manager());
    suggestion.main_text.value =
        credit_card.CardNameForAutofillDisplay(nickname);
    suggestion.minor_text.value =
        credit_card.ObfuscatedNumberWithVisibleLastFourDigits();
    std::optional<Suggestion::Text> benefit_label =
        GetCreditCardBenefitSuggestionLabel(credit_card, client);
    if (benefit_label && client.GetPersonalDataManager()
                             ->payments_data_manager()
                             .IsCardEligibleForBenefits(credit_card)) {
      suggestion.labels.push_back({*benefit_label});
      suggestion.payload = Suggestion::PaymentsPayload(
          /* should_display_terms_available= */ true);
    }
    if (credit_card.record_type() == CreditCard::RecordType::kVirtualCard) {
      suggestion.type = SuggestionType::kVirtualCreditCardEntry;
      suggestion.apply_deactivated_style = !IsCardSuggestionAcceptable(
          credit_card, client, /*is_manual_fallback= */ false);
      suggestion.labels.push_back(std::vector<Suggestion::Text>{
          Suggestion::Text(l10n_util::GetStringUTF16(
              suggestion.apply_deactivated_style
                  ? IDS_AUTOFILL_VIRTUAL_CARD_DISABLED_SUGGESTION_OPTION_VALUE
                  : IDS_AUTOFILL_VIRTUAL_CARD_SUGGESTION_OPTION_VALUE))});
    } else {
      suggestion.type = SuggestionType::kCreditCardEntry;
      suggestion.labels.push_back(
          std::vector<Suggestion::Text>{Suggestion::Text(
              credit_card.GetInfo(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
                                  client.GetPersonalDataManager()
                                      ->payments_data_manager()
                                      .app_locale()))});
    }
    suggestions.push_back(suggestion);
  }
  return suggestions;
}

// static
Suggestion CreateManageCreditCardsSuggestion(bool with_gpay_logo) {
  return CreateManagePaymentMethodsEntry(SuggestionType::kManageCreditCard,
                                         with_gpay_logo);
}

// static
Suggestion CreateManageIbansSuggestion() {
  return CreateManagePaymentMethodsEntry(SuggestionType::kManageIban,
                                         /*with_gpay_logo=*/false);
}

// static
std::vector<Suggestion> GetSuggestionsForIbans(const std::vector<Iban>& ibans) {
  if (ibans.empty()) {
    return {};
  }
  std::vector<Suggestion> suggestions;
  suggestions.reserve(ibans.size() + 2);
  for (const Iban& iban : ibans) {
    Suggestion suggestion;
    suggestion.custom_icon =
        ui::ResourceBundle::GetSharedInstance().GetImageNamed(
            IDR_AUTOFILL_IBAN);
    suggestion.icon = Suggestion::Icon::kIban;
    suggestion.type = SuggestionType::kIbanEntry;
    if (iban.record_type() == Iban::kLocalIban) {
      suggestion.payload = Suggestion::BackendId(Suggestion::Guid(iban.guid()));
    } else {
      CHECK(iban.record_type() == Iban::kServerIban);
      suggestion.payload =
          Suggestion::BackendId(Suggestion::InstrumentId(iban.instrument_id()));
    }

    std::u16string iban_identifier =
        iban.GetIdentifierStringForAutofillDisplay();
    if constexpr (BUILDFLAG(IS_ANDROID)) {
      // For Android keyboard accessory, the displayed value will be nickname +
      // identifier string, if the nickname is too long to fit due to bubble
      // width limitation, it will be truncated.
      if (!iban.nickname().empty()) {
        suggestion.main_text.value = iban.nickname();
        suggestion.minor_text.value = std::move(iban_identifier);
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

  suggestions.push_back(CreateSeparator());
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
        base::ASCIIToUTF16(promo_code_offer->GetPromoCode()));
    Suggestion& suggestion = suggestions.back();
    if (!promo_code_offer->GetDisplayStrings().value_prop_text.empty()) {
      suggestion.labels = {{Suggestion::Text(base::ASCIIToUTF16(
          promo_code_offer->GetDisplayStrings().value_prop_text))}};
    }
    suggestion.payload = Suggestion::BackendId(
        Suggestion::Guid(base::NumberToString(promo_code_offer->GetOfferId())));
    suggestion.type = SuggestionType::kMerchantPromoCodeEntry;

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
    suggestions.push_back(CreateSeparator());

    // Add the footer suggestion that navigates the user to the promo code
    // details page in the offers suggestions popup.
    suggestions.emplace_back(l10n_util::GetStringUTF16(
        IDS_AUTOFILL_PROMO_CODE_SUGGESTIONS_FOOTER_TEXT));
    Suggestion& suggestion = suggestions.back();
    suggestion.type = SuggestionType::kSeePromoCodeDetails;

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
                                const AutofillClient& client,
                                bool is_manual_fallback) {
  if (card.record_type() == CreditCard::RecordType::kVirtualCard) {
    auto* optimization_guide = client.GetAutofillOptimizationGuide();
    return !(
        optimization_guide &&
        optimization_guide->ShouldBlockFormFieldSuggestion(
            client.GetLastCommittedPrimaryMainFrameOrigin().GetURL(), card));
  }

  return !is_manual_fallback;
}

std::vector<CreditCard> GetOrderedCardsToSuggestForTest(
    const AutofillClient& client,
    const FormFieldData& trigger_field,
    FieldType trigger_field_type,
    bool suppress_disused_cards,
    bool prefix_match,
    bool require_non_empty_value_on_trigger_field,
    bool include_virtual_cards,
    bool use_legacy_algorithm) {
  return GetOrderedCardsToSuggest(client, trigger_field, trigger_field_type,
                                  suppress_disused_cards, prefix_match,
                                  require_non_empty_value_on_trigger_field,
                                  include_virtual_cards, use_legacy_algorithm);
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

bool ShouldShowVirtualCardOptionForTest(const CreditCard* candidate_card,
                                        const AutofillClient& client) {
  return ShouldShowVirtualCardOption(candidate_card, client);
}

void FilterCardsToSuggestForCvcFields(
    FieldType trigger_field_type,
    const base::flat_set<std::string>& four_digit_combinations_in_dom,
    const base::flat_set<std::u16string>&
        autofilled_last_four_digits_in_form_for_suggestion_filtering,
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
    // `autofilled_last_four_digits_in_form_for_suggestion_filtering` being
    // empty implies no card was autofilled, show all suggestions.
    if (autofilled_last_four_digits_in_form_for_suggestion_filtering.empty()) {
      return;
    }
    std::erase_if(
        cards_to_suggest,
        [&autofilled_last_four_digits_in_form_for_suggestion_filtering](
            const CreditCard& credit_card) {
          return !autofilled_last_four_digits_in_form_for_suggestion_filtering
                      .contains(credit_card.LastFourDigits());
        });
  }
}

}  // namespace autofill
