// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_suggestion_generator.h"

#include <functional>
#include <string>

#include "base/check_deref.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_browser_util.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_granular_filling_utils.h"
#include "components/autofill/core/browser/autofill_optimization_guide.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/data_model/borrowed_transliterator.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/credit_card_benefit.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/field_filling_address_util.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/address_field_parser.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/geo/address_i18n.h"
#include "components/autofill/core/browser/geo/phone_number_i18n.h"
#include "components/autofill/core/browser/metrics/address_rewriter_in_profile_subset_metrics.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/payments/card_metadata_metrics.h"
#include "components/autofill/core/browser/payments/autofill_offer_manager.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_formatter.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
#include "ui/native_theme/native_theme.h"  // nogncheck
#endif

namespace autofill {

namespace {

// Returns the credit card field |value| trimmed from whitespace and with stop
// characters removed.
std::u16string SanitizeCreditCardFieldValue(const std::u16string& value) {
  std::u16string sanitized;
  // We remove whitespace as well as some invisible unicode characters.
  base::TrimWhitespace(value, base::TRIM_ALL, &sanitized);
  base::TrimString(sanitized,
                   std::u16string({base::i18n::kRightToLeftMark,
                                   base::i18n::kLeftToRightMark}),
                   &sanitized);
  // Some sites have ____-____-____-____ in their credit card number fields, for
  // example.
  base::RemoveChars(sanitized, u"-_", &sanitized);
  return sanitized;
}

// Returns the card-linked offers map with credit card guid as the key and the
// pointer to the linked AutofillOfferData as the value.
std::map<std::string, AutofillOfferData*> GetCardLinkedOffers(
    AutofillClient& autofill_client) {
  if (AutofillOfferManager* offer_manager =
          autofill_client.GetAutofillOfferManager()) {
    return offer_manager->GetCardLinkedOffersMap(
        autofill_client.GetLastCommittedPrimaryMainFrameURL());
  }
  return {};
}

bool ShouldUseNationalFormatPhoneNumber(FieldType trigger_field_type) {
  return GroupTypeOfFieldType(trigger_field_type) == FieldTypeGroup::kPhone &&
         trigger_field_type != PHONE_HOME_WHOLE_NUMBER &&
         trigger_field_type != PHONE_HOME_COUNTRY_CODE;
}

std::u16string GetFormattedPhoneNumber(const AutofillProfile& profile,
                                       const std::string& app_locale,
                                       bool should_use_national_format) {
  const std::string phone_home_whole_number =
      base::UTF16ToUTF8(profile.GetInfo(PHONE_HOME_WHOLE_NUMBER, app_locale));
  const std::string address_home_country =
      base::UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_COUNTRY));

  const std::string formatted_phone_number =
      should_use_national_format
          ? i18n::FormatPhoneNationallyForDisplay(phone_home_whole_number,
                                                  address_home_country)
          : i18n::FormatPhoneForDisplay(phone_home_whole_number,
                                        address_home_country);
  return base::UTF8ToUTF16(formatted_phone_number);
}

int GetObfuscationLength() {
#if BUILDFLAG(IS_ANDROID)
  // On Android, the obfuscation length is 2.
  return 2;
#elif BUILDFLAG(IS_IOS)
  return base::FeatureList::IsEnabled(
             features::kAutofillUseTwoDotsForLastFourDigits)
             ? 2
             : 4;
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

// For a profile containing a full address, the main text is the name, and
// the label is the address. The problem arises when a profile isn't complete
// (aka it doesn't have a name or an address etc.).
//
// `AutofillProfile::CreateDifferentiatingLabels` generates the a text which
// contains 2 address fields.
//
// Example for a full autofill profile:
// "Full Name, Address"
//
// Examples where autofill profiles are incomplete:
// "City, Country"
// "Country, Email"
//
// Note: the separator isn't actually ", ", it is
// IDS_AUTOFILL_ADDRESS_SUMMARY_SEPARATOR
std::u16string GetProfileSuggestionMainTextForNonAddressField(
    const AutofillProfile& profile,
    const std::string& app_locale) {
  std::vector<std::u16string> suggestion_text_array;
  AutofillProfile::CreateDifferentiatingLabels({&profile}, app_locale,
                                               &suggestion_text_array);
  CHECK_EQ(suggestion_text_array.size(), 1u);

  const std::u16string separator =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_ADDRESS_SUMMARY_SEPARATOR);
  // The first part contains the main text.
  std::vector<std::u16string> text_pieces =
      base::SplitStringUsingSubstr(suggestion_text_array[0], separator,
                                   base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  return text_pieces[0];
}

// Check comment of method above:
// `GetProfileSuggestionMainTextForNonAddressField`.
std::vector<std::u16string> GetProfileSuggestionLabelForNonAddressField(
    const std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>&
        profiles,
    const std::string& app_locale) {
  std::vector<std::u16string> labels;
  AutofillProfile::CreateDifferentiatingLabels(profiles, app_locale, &labels);
  CHECK_EQ(labels.size(), profiles.size());

  for (std::u16string& label : labels) {
    const std::u16string separator =
        l10n_util::GetStringUTF16(IDS_AUTOFILL_ADDRESS_SUMMARY_SEPARATOR);
    std::vector<std::u16string> text_pieces = base::SplitStringUsingSubstr(
        label, separator, base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

    // `text_pieces[1]` contains the label.
    label = text_pieces.size() > 1 ? text_pieces[1] : u"";
  }
  return labels;
}

// In addition to just getting the values out of the profile, this function
// handles type-specific formatting.
std::u16string GetProfileSuggestionMainText(const AutofillProfile& profile,
                                            const std::string& app_locale,
                                            FieldType trigger_field_type) {
  if (!IsAddressType(trigger_field_type) &&
      base::FeatureList::IsEnabled(
          features::kAutofillForUnclassifiedFieldsAvailable)) {
    return GetProfileSuggestionMainTextForNonAddressField(profile, app_locale);
  }
  if (trigger_field_type == ADDRESS_HOME_STREET_ADDRESS) {
    std::string street_address_line;
    ::i18n::addressinput::GetStreetAddressLinesAsSingleLine(
        *i18n::CreateAddressDataFromAutofillProfile(profile, app_locale),
        &street_address_line);
    return base::UTF8ToUTF16(street_address_line);
  }

  return profile.GetInfo(trigger_field_type, app_locale);
}

Suggestion GetEditAddressProfileSuggestion(Suggestion::BackendId backend_id) {
  Suggestion suggestion(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_EDIT_ADDRESS_PROFILE_POPUP_OPTION_SELECTED));
  suggestion.popup_item_id = PopupItemId::kEditAddressProfile;
  suggestion.icon = Suggestion::Icon::kEdit;
  suggestion.payload = backend_id;
  suggestion.acceptance_a11y_announcement = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_A11Y_ANNOUNCE_EDIT_ADDRESS_PROFILE_POPUP_OPTION_SELECTED);
  return suggestion;
}

// Creates the suggestion that will open the delete address profile dialog.
Suggestion GetDeleteAddressProfileSuggestion(Suggestion::BackendId backend_id) {
  Suggestion suggestion(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_DELETE_ADDRESS_PROFILE_POPUP_OPTION_SELECTED));
  suggestion.popup_item_id = PopupItemId::kDeleteAddressProfile;
  suggestion.icon = Suggestion::Icon::kDelete;
  suggestion.payload = backend_id;
  suggestion.acceptance_a11y_announcement = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_A11Y_ANNOUNCE_DELETE_ADDRESS_PROFILE_POPUP_OPTION_SELECTED);
  return suggestion;
}

// Creates the suggestion that will fill all address related fields.
Suggestion GetFillFullAddressSuggestion(Suggestion::BackendId backend_id) {
  Suggestion suggestion(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_FILL_ADDRESS_GROUP_POPUP_OPTION_SELECTED));
  suggestion.main_text.is_primary = Suggestion::Text::IsPrimary(false);
  suggestion.popup_item_id = PopupItemId::kFillFullAddress;
  suggestion.payload = backend_id;
  suggestion.acceptance_a11y_announcement = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_A11Y_ANNOUNCE_FILL_ADDRESS_GROUP_POPUP_OPTION_SELECTED);
  return suggestion;
}

// Creates the suggestion that will fill all name related fields.
Suggestion GetFillFullNameSuggestion(Suggestion::BackendId backend_id) {
  Suggestion suggestion(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_FILL_NAME_GROUP_POPUP_OPTION_SELECTED));
  suggestion.popup_item_id = PopupItemId::kFillFullName;
  suggestion.main_text.is_primary = Suggestion::Text::IsPrimary(false);
  suggestion.payload = backend_id;
  suggestion.acceptance_a11y_announcement = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_A11Y_ANNOUNCE_FILL_NAME_GROUP_POPUP_OPTION_SELECTED);

  return suggestion;
}

// Creates the suggestion that will fill the whole form for the profile. This
// suggestion is displayed once the users is on group filling level or field by
// field level. It is used as a way to allow users to go back to filling the
// whole form.
Suggestion GetFillEverythingFromAddressProfileSuggestion(
    Suggestion::BackendId backend_id) {
  Suggestion suggestion(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_FILL_EVERYTHING_FROM_ADDRESS_PROFILE_POPUP_OPTION_SELECTED));
  suggestion.popup_item_id = PopupItemId::kFillEverythingFromAddressProfile;
  suggestion.icon = Suggestion::Icon::kMagic;
  suggestion.payload = backend_id;
  suggestion.acceptance_a11y_announcement = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_A11Y_ANNOUNCE_FILL_EVERYTHING_FROM_ADDRESS_PROFILE_POPUP_OPTION_SELECTED);
  return suggestion;
}

// Append new suggestions to `suggestions` based on the `FieldType` list
// provided. Suggestions are not added if their info is not found in the
// provided `profile`. Returns true if any suggestion was added.
// Note that adding a new field-by-field filling `FieldType` should be
// reflected in `AutofillFieldByFieldFillingTypes`.
bool AddAddressFieldByFieldSuggestions(
    const std::vector<FieldType>& field_types,
    const AutofillProfile& profile,
    const std::string& app_locale,
    std::vector<Suggestion>& suggestions) {
  bool any_suggestion_added = false;
  for (auto field_type : field_types) {
    // Field-by-field suggestions are never generated for
    // `ADDRESS_HOME_STREET_ADDRESS` field type.
    CHECK(field_type != ADDRESS_HOME_STREET_ADDRESS);
    std::u16string main_text;
    if (field_type == PHONE_HOME_WHOLE_NUMBER) {
      main_text = GetFormattedPhoneNumber(profile, app_locale,
                                          /*should_use_national_format=*/false);
    } else {
      main_text = GetProfileSuggestionMainText(profile, app_locale, field_type);
    }
    if (!main_text.empty()) {
      suggestions.emplace_back(main_text,
                               PopupItemId::kAddressFieldByFieldFilling);
      suggestions.back().field_by_field_filling_type_used =
          std::optional(field_type);
      suggestions.back().payload = Suggestion::Guid(profile.guid());
      any_suggestion_added = true;
    }
  }
  return any_suggestion_added;
}

// Given an address `type` and `sub_type`, returns whether the `sub_type` info
// stored in `profile` is a substring of the info stored in `profile` for
// `type`.
bool CheckIfTypeContainsSubtype(FieldType type,
                                FieldType sub_type,
                                const AutofillProfile& profile,
                                const std::string& app_locale) {
  if (!profile.HasInfo(type) || !profile.HasInfo(sub_type)) {
    return false;
  }

  std::u16string value = profile.GetInfo(type, app_locale);
  std::u16string sub_value = profile.GetInfo(sub_type, app_locale);
  return value != sub_value && value.find(sub_value) != std::u16string::npos;
}

// Adds name related child suggestions to build autofill popup submenu.
// The param `type` refers to the triggering field type (clicked by the users)
// and is used to define  whether the `PopupItemId::kFillFullName` suggestion
// will be available.
void AddNameChildSuggestions(FieldTypeGroup trigger_field_type_group,
                             const AutofillProfile& profile,
                             const std::string& app_locale,
                             Suggestion& suggestion) {
  if (trigger_field_type_group == FieldTypeGroup::kName) {
    // Note that this suggestion can only be added if name infos exist in the
    // profile.
    suggestion.children.push_back(
        GetFillFullNameSuggestion(Suggestion::Guid(profile.guid())));
  }
  if (AddAddressFieldByFieldSuggestions({NAME_FIRST, NAME_MIDDLE, NAME_LAST},
                                        profile, app_locale,
                                        suggestion.children)) {
    suggestion.children.push_back(
        AutofillSuggestionGenerator::CreateSeparator());
  };
}

// Adds address line suggestions (ADDRESS_HOME_LINE1 and/or
// ADDRESS_HOME_LINE2) to `suggestions.children`. It potentially includes
// sub-children if one of the added suggestions contains
// ADDRESS_HOME_HOUSE_NUMBER and/or ADDRESS_HOME_STREET_NAME. Returns true if at
// least one suggestion was appended to `suggestions.children`.
bool AddAddressLineChildSuggestions(const AutofillProfile& profile,
                                    const std::string& app_locale,
                                    std::vector<Suggestion>& suggestions) {
  auto add_address_line = [&](FieldType type) -> bool {
    CHECK(type == ADDRESS_HOME_LINE1 || type == ADDRESS_HOME_LINE2);

    if (!AddAddressFieldByFieldSuggestions({type}, profile, app_locale,
                                           suggestions)) {
      return false;
    }

    if (CheckIfTypeContainsSubtype(type, ADDRESS_HOME_HOUSE_NUMBER, profile,
                                   app_locale) &&
        AddAddressFieldByFieldSuggestions({ADDRESS_HOME_HOUSE_NUMBER}, profile,
                                          app_locale,
                                          suggestions.back().children)) {
      Suggestion& address_line_suggestion = suggestions.back().children.back();
      address_line_suggestion.labels = {
          {Suggestion::Text(l10n_util::GetStringUTF16(
              IDS_AUTOFILL_HOUSE_NUMBER_SUGGESTION_SECONDARY_TEXT))}};
      address_line_suggestion
          .acceptance_a11y_announcement = l10n_util::GetStringUTF16(
          IDS_AUTOFILL_HOUSE_NUMBER_SUGGESTION_SECONDARY_TEXT_OPTION_SELECTED);
    }
    if (CheckIfTypeContainsSubtype(type, ADDRESS_HOME_STREET_NAME, profile,
                                   app_locale) &&
        AddAddressFieldByFieldSuggestions({ADDRESS_HOME_STREET_NAME}, profile,
                                          app_locale,
                                          suggestions.back().children)) {
      Suggestion& address_line_suggestion = suggestions.back().children.back();
      address_line_suggestion.labels = {
          {Suggestion::Text(l10n_util::GetStringUTF16(
              IDS_AUTOFILL_STREET_NAME_SUGGESTION_SECONDARY_TEXT))}};
      address_line_suggestion
          .acceptance_a11y_announcement = l10n_util::GetStringUTF16(
          IDS_AUTOFILL_STREET_NAME_SUGGESTION_SECONDARY_TEXT_OPTION_SELECTED);
    }

    return true;
  };
  bool added_address_line1 = add_address_line(ADDRESS_HOME_LINE1);
  bool added_address_line2 = add_address_line(ADDRESS_HOME_LINE2);
  return added_address_line1 || added_address_line2;
}

// Adds address related child suggestions to build autofill popup submenu.
// The param `trigger_field_type_group` refers to the type of the field clicked
// by the user and is used to define whether the `PopupItemId::kFillFullAddress`
// suggestion will be available. Note that `FieldTypeGroup::kCompany` is also
// included into the address group.
void AddAddressChildSuggestions(FieldTypeGroup trigger_field_type_group,
                                const AutofillProfile& profile,
                                const std::string& app_locale,
                                Suggestion& suggestion) {
  if (trigger_field_type_group == FieldTypeGroup::kAddress ||
      trigger_field_type_group == FieldTypeGroup::kCompany) {
    // Note that this suggestion can only be added if address infos exist in the
    // profile.
    suggestion.children.push_back(
        GetFillFullAddressSuggestion(Suggestion::Guid(profile.guid())));
  }

  bool added_company = AddAddressFieldByFieldSuggestions(
      {COMPANY_NAME}, profile, app_locale, suggestion.children);
  bool added_any_address_line =
      AddAddressLineChildSuggestions(profile, app_locale, suggestion.children);
  bool added_city = AddAddressFieldByFieldSuggestions(
      {ADDRESS_HOME_CITY}, profile, app_locale, suggestion.children);
  bool added_zip = AddAddressFieldByFieldSuggestions(
      {ADDRESS_HOME_ZIP}, profile, app_locale, suggestion.children);
  if (added_company || added_any_address_line || added_zip || added_city) {
    suggestion.children.push_back(
        AutofillSuggestionGenerator::CreateSeparator());
  }
}

// Adds contact related child suggestions (i.e email and phone number) to
// build autofill popup submenu. The param `trigger_field_type` refers to the
// field clicked by the user and affects whether international or local phone
// number will be shown to the user in the suggestion. The field type group of
// the `trigger_field_type` is used to define whether the phone number and email
// suggestions will behave as `PopupItemId::kAddressFieldByFieldFilling` or as
// `PopupItemId::kFillFullPhoneNumber`/`PopupItemId::kFillFullEmail`
// respectively. When the triggering field group matches the type of the field
// we are adding, the suggestion will be of group filling type, other than field
// by field.
void AddContactChildSuggestions(FieldType trigger_field_type,
                                const AutofillProfile& profile,
                                const std::string& app_locale,
                                Suggestion& suggestion) {
  const FieldTypeGroup trigger_field_type_group =
      GroupTypeOfFieldType(trigger_field_type);

  bool phone_number_suggestion_added = false;
  if (profile.HasInfo(PHONE_HOME_WHOLE_NUMBER)) {
    const bool is_phone_field =
        trigger_field_type_group == FieldTypeGroup::kPhone;
    if (is_phone_field) {
      Suggestion phone_number_suggestion(
          GetFormattedPhoneNumber(
              profile, app_locale,
              ShouldUseNationalFormatPhoneNumber(trigger_field_type)),
          PopupItemId::kFillFullPhoneNumber);
      // `PopupItemId::kAddressFieldByFieldFilling` suggestions do not use
      // profile, therefore only set the backend id in the group filling case.
      phone_number_suggestion.payload = Suggestion::Guid(profile.guid());
      suggestion.children.push_back(std::move(phone_number_suggestion));
      phone_number_suggestion_added = true;
    } else {
      phone_number_suggestion_added = AddAddressFieldByFieldSuggestions(
          {PHONE_HOME_WHOLE_NUMBER}, profile, app_locale, suggestion.children);
    }
  }

  bool email_address_suggestion_added = false;
  if (profile.HasInfo(EMAIL_ADDRESS)) {
    const bool is_email_field =
        trigger_field_type_group == FieldTypeGroup::kEmail;
    if (is_email_field) {
      Suggestion email_address_suggestion(
          profile.GetInfo(EMAIL_ADDRESS, app_locale),
          PopupItemId::kFillFullEmail);
      // `PopupItemId::kAddressFieldByFieldFilling` suggestions do not use
      // profile, therefore only set the backend id in the group filling case.
      email_address_suggestion.payload = Suggestion::Guid(profile.guid());
      suggestion.children.push_back(std::move(email_address_suggestion));
      email_address_suggestion_added = true;
    } else {
      email_address_suggestion_added = AddAddressFieldByFieldSuggestions(
          {EMAIL_ADDRESS}, profile, app_locale, suggestion.children);
    }
  }

  if (email_address_suggestion_added || phone_number_suggestion_added) {
    suggestion.children.push_back(
        AutofillSuggestionGenerator::CreateSeparator());
  }
}

// Adds footer child suggestions to build autofill popup submenu.
void AddFooterChildSuggestions(const AutofillProfile& profile,
                               FieldType trigger_field_type,
                               std::optional<FieldTypeSet> last_targeted_fields,
                               Suggestion& suggestion) {
  // If the trigger field is not classified as an address field, then the
  // filling was triggered from the context menu. In this scenario, the user
  // should not be able to fill everything.
  // If the last filling granularity was not full form, add the
  // `PopupItemId::kFillEverythingFromAddressProfile` suggestion. This allows
  // the user to go back to filling the whole form once in a more fine grained
  // filling experience.
  if (IsAddressType(trigger_field_type) &&
      (last_targeted_fields && *last_targeted_fields != kAllFieldTypes)) {
    suggestion.children.push_back(GetFillEverythingFromAddressProfileSuggestion(
        Suggestion::Guid(profile.guid())));
  }
  suggestion.children.push_back(
      GetEditAddressProfileSuggestion(Suggestion::Guid(profile.guid())));
  suggestion.children.push_back(
      GetDeleteAddressProfileSuggestion(Suggestion::Guid(profile.guid())));
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
                     PopupItemId::kCreditCardFieldByFieldFilling);
  // TODO(crbug.com/1121806): Use instrument ID for server credit cards.
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
                       PopupItemId::kCreditCardFieldByFieldFilling);
  // TODO(crbug.com/1121806): Use instrument ID for server credit cards.
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
      PopupItemId::kCreditCardFieldByFieldFilling);
  // TODO(crbug.com/1121806): Use instrument ID for server credit cards.
  cc_expiration.payload = Suggestion::Guid(credit_card.guid());
  cc_expiration.field_by_field_filling_type_used =
      CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR;
  cc_expiration.labels.push_back({Suggestion::Text(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_PAYMENTS_MANUAL_FALLBACK_AUTOFILL_POPUP_CC_EXPIRY_DATE_SUGGESTION_LABEL))});

  Suggestion cc_expiration_year(
      credit_card.GetInfo(CREDIT_CARD_EXP_2_DIGIT_YEAR, app_locale),
      PopupItemId::kCreditCardFieldByFieldFilling);
  // TODO(crbug.com/1121806): Use instrument ID for server credit cards.
  cc_expiration_year.payload = Suggestion::Guid(credit_card.guid());
  cc_expiration_year.field_by_field_filling_type_used =
      CREDIT_CARD_EXP_2_DIGIT_YEAR;
  cc_expiration_year.labels.push_back({Suggestion::Text(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_PAYMENTS_MANUAL_FALLBACK_AUTOFILL_POPUP_CC_EXPIRY_YEAR_SUGGESTION_LABEL))});

  Suggestion cc_expiration_month(
      credit_card.GetInfo(CREDIT_CARD_EXP_MONTH, app_locale),
      PopupItemId::kCreditCardFieldByFieldFilling);
  // TODO(crbug.com/1121806): Use instrument ID for server credit cards.
  cc_expiration_month.payload = Suggestion::Guid(credit_card.guid());
  cc_expiration_month.field_by_field_filling_type_used = CREDIT_CARD_EXP_MONTH;
  cc_expiration_month.labels.push_back({Suggestion::Text(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_PAYMENTS_MANUAL_FALLBACK_AUTOFILL_POPUP_CC_EXPIRY_MONTH_SUGGESTION_LABEL))});

  cc_expiration.children.push_back(std::move(cc_expiration_year));
  cc_expiration.children.push_back(std::move(cc_expiration_month));
  suggestion.children.push_back(std::move(cc_expiration));
}

// Sets the `popup_item_id` for `suggestion` depending on
// `last_filling_granularity`.
// `last_targeted_fields` specified the last set of fields target by the user.
// When not present, we default to full form.
// This function is called only for first-level popup.
PopupItemId GetProfileSuggestionPopupItemId(
    std::optional<FieldTypeSet> last_targeted_fields,
    FieldType trigger_field_type) {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillGranularFillingAvailable)) {
    return PopupItemId::kAddressEntry;
  }

  // If a field is not classified as an address, then autofill was triggered
  // from the context menu.
  if (!IsAddressType(trigger_field_type)) {
    return PopupItemId::kAddressEntry;
  }

  const FieldTypeGroup trigger_field_type_group =
      GroupTypeOfFieldType(trigger_field_type);

  // Lambda to return the expected `PopupItemId` when
  // `last_targeted_fields` matches one of the granular filling groups.
  auto get_popup_item_id_for_group_filling = [&] {
    switch (trigger_field_type_group) {
      case FieldTypeGroup::kName:
        return PopupItemId::kFillFullName;
      case FieldTypeGroup::kAddress:
      case FieldTypeGroup::kCompany:
        return PopupItemId::kFillFullAddress;
      case FieldTypeGroup::kPhone:
        return PopupItemId::kFillFullPhoneNumber;
      case FieldTypeGroup::kEmail:
        return PopupItemId::kFillFullEmail;
      case FieldTypeGroup::kNoGroup:
      case FieldTypeGroup::kCreditCard:
      case FieldTypeGroup::kPasswordField:
      case FieldTypeGroup::kTransaction:
      case FieldTypeGroup::kUsernameField:
      case FieldTypeGroup::kUnfillable:
      case FieldTypeGroup::kIban:
        NOTREACHED_NORETURN();
    }
  };

  switch (GetFillingMethodFromTargetedFields(
      last_targeted_fields.value_or(kAllFieldTypes))) {
    case AutofillFillingMethod::kGroupFilling:
      return get_popup_item_id_for_group_filling();
    case AutofillFillingMethod::kFullForm:
      return PopupItemId::kAddressEntry;
    case AutofillFillingMethod::kFieldByFieldFilling:
      return PopupItemId::kAddressFieldByFieldFilling;
    case AutofillFillingMethod::kNone:
      NOTREACHED_NORETURN();
  }
}

// Returns the number of occurrences of a certain `Suggestion::main_text` and
// its granular filling label. Used to decide whether or not a differentiating
// label should be added. If the concatenation of `Suggestion::main_text` and
// its respective granular filling label is unique, there is no need for a
// differentiating label.
std::map<std::u16string, size_t>
GetNumberOfSuggestionMainTextAndGranularFillingLabelOcurrences(
    base::span<const Suggestion> suggestions,
    const std::vector<std::vector<std::u16string>>&
        suggestions_granular_filling_labels) {
  CHECK_EQ(suggestions_granular_filling_labels.size(), suggestions.size());
  // Count the occurrences of the concatenation between `Suggestion::main_text`
  // and its granular filling label.
  std::vector<std::u16string> concatenated_suggestions_granular_filling_labels;
  concatenated_suggestions_granular_filling_labels.reserve(
      suggestions_granular_filling_labels.size());
  for (const std::vector<std::u16string>& granular_filling_labels :
       suggestions_granular_filling_labels) {
    concatenated_suggestions_granular_filling_labels.push_back(
        base::StrCat(granular_filling_labels));
  }
  std::map<std::u16string, size_t> main_text_and_granular_filling_label_count;
  for (size_t i = 0; i < suggestions.size(); ++i) {
    ++main_text_and_granular_filling_label_count
        [suggestions[i].main_text.value +
         concatenated_suggestions_granular_filling_labels[i]];
  }
  return main_text_and_granular_filling_label_count;
}

// Returns whether the `ADDRESS_HOME_LINE1` should be included in the granular
// filling labels vector. This depends on whether `triggering_field_type` is a
// field that will usually allow users to easily identify their address.
bool ShouldAddAddressLine1ToGranularFillingLabels(
    FieldType triggering_field_type) {
  static constexpr std::array kAddressRecognizingFields = {
      ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE2, ADDRESS_HOME_STREET_ADDRESS};
  return !base::Contains(kAddressRecognizingFields, triggering_field_type);
}
// Creates a specific granular filling labels vector for each `AutofillProfile`
// in `profiles` when the `last_filling_granularity` for a certain form was
// group filling. This is done to give users feedback about the filling
// behaviour. Returns an empty vector when no granular filling label needs to be
// applied for a profile.
std::vector<std::vector<std::u16string>> GetGranularFillingLabels(
    const std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>&
        profiles,
    std::optional<FieldTypeSet> last_targeted_fields,
    FieldType triggering_field_type,
    const std::string& app_locale) {
  if (!last_targeted_fields ||
      !AreFieldsGranularFillingGroup(*last_targeted_fields)) {
    return std::vector<std::vector<std::u16string>>(profiles.size());
  }
  std::vector<std::vector<std::u16string>> labels;
  labels.reserve(profiles.size());
  for (const AutofillProfile* profile : profiles) {
    switch (GroupTypeOfFieldType(triggering_field_type)) {
      case FieldTypeGroup::kName:
        labels.push_back({l10n_util::GetStringUTF16(
            IDS_AUTOFILL_FILL_NAME_GROUP_POPUP_OPTION_SELECTED)});
        break;
      case FieldTypeGroup::kCompany:
      case FieldTypeGroup::kAddress: {
        std::vector<std::u16string>& profile_labels = labels.emplace_back();
        profile_labels.push_back(l10n_util::GetStringUTF16(
            IDS_AUTOFILL_FILL_ADDRESS_GROUP_POPUP_OPTION_SELECTED));
        if (ShouldAddAddressLine1ToGranularFillingLabels(
                triggering_field_type)) {
          // If the triggering type does not contain information that is
          // useful to identify addresses, add `ADDRESS_HOME_LINE1` to
          // the differentiating labels list.
          profile_labels.push_back(
              profile->GetInfo(ADDRESS_HOME_LINE1, app_locale));
        }
        break;
      }
      case FieldTypeGroup::kNoGroup:
      case FieldTypeGroup::kPhone:
      case FieldTypeGroup::kEmail:
      case FieldTypeGroup::kCreditCard:
      case FieldTypeGroup::kPasswordField:
      case FieldTypeGroup::kTransaction:
      case FieldTypeGroup::kUsernameField:
      case FieldTypeGroup::kUnfillable:
      case FieldTypeGroup::kIban:
        labels.emplace_back();
    }
  }
  return labels;
}

// Returns a `FieldTypeSet` to be excluded from the differentiating labels
// generation. The granular filling labels can contain information such
// `ADDRESS_HOME_LINE1` depending on `triggering_field_type` and
// `last_targeted_fields`, see `GetGranularFillingLabels()` for
// details.
FieldTypeSet GetFieldTypesToExcludeFromDifferentiatingLabelsGeneration(
    FieldType triggering_field_type,
    std::optional<FieldTypeSet> last_targeted_fields) {
  if (!last_targeted_fields ||
      !AreFieldsGranularFillingGroup(*last_targeted_fields)) {
    return {triggering_field_type};
  }
  switch (GroupTypeOfFieldType(triggering_field_type)) {
    case FieldTypeGroup::kAddress:
      if (ShouldAddAddressLine1ToGranularFillingLabels(triggering_field_type)) {
        // In the case where the `ADDRESS_HOME_LINE1` was added to the granular
        // filling labels, make sure to exclude fields that contain
        // `ADDRESS_HOME_LINE1` from the field types to use when creating the
        // differentiating label.
        // For details on how `ADDRESS_HOME_LINE1` is added, see
        // `GetGranularFillingLabels()`.
        return {triggering_field_type, ADDRESS_HOME_LINE1,
                ADDRESS_HOME_STREET_ADDRESS};
      } else {
        return {triggering_field_type};
      }
    case FieldTypeGroup::kName:
    case FieldTypeGroup::kCompany:
    case FieldTypeGroup::kNoGroup:
    case FieldTypeGroup::kPhone:
    case FieldTypeGroup::kEmail:
    case FieldTypeGroup::kCreditCard:
    case FieldTypeGroup::kPasswordField:
    case FieldTypeGroup::kTransaction:
    case FieldTypeGroup::kUsernameField:
    case FieldTypeGroup::kUnfillable:
    case FieldTypeGroup::kIban:
      return {triggering_field_type};
  }
}

// Returns for each profile in `profiles` a differentiating label string to be
// used as a secondary text in the corresponding suggestion bubble.
// `field_types` the types of the fields that will be filled by the suggestion.
std::vector<std::u16string> GetProfileSuggestionLabels(
    const std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>&
        profiles,
    const FieldTypeSet& field_types,
    FieldType trigger_field_type,
    std::optional<FieldTypeSet> last_targeted_fields,
    const std::string& app_locale) {
  // Generate disambiguating labels based on the list of matches.
  std::vector<std::u16string> differentiating_labels;
  if (!IsAddressType(trigger_field_type) &&
      base::FeatureList::IsEnabled(
          features::kAutofillForUnclassifiedFieldsAvailable)) {
    differentiating_labels =
        GetProfileSuggestionLabelForNonAddressField(profiles, app_locale);
  } else {
    if (base::FeatureList::IsEnabled(
            features::kAutofillGranularFillingAvailable)) {
      AutofillProfile::CreateInferredLabels(
          profiles, /*suggested_fields=*/std::nullopt, trigger_field_type,
          GetFieldTypesToExcludeFromDifferentiatingLabelsGeneration(
              trigger_field_type, last_targeted_fields),
          // Phone fields are a special case. For them we want both the
          // `FULL_NAME` and `ADDRESS_HOME_LINE1` to be present.
          /*minimal_fields_shown=*/GroupTypeOfFieldType(trigger_field_type) ==
                  FieldTypeGroup::kPhone
              ? 2
              : 1,
          app_locale, &differentiating_labels);
    } else {
      AutofillProfile::CreateInferredLabels(
          profiles, field_types, /*triggering_field_type=*/std::nullopt,
          GetFieldTypesToExcludeFromDifferentiatingLabelsGeneration(
              trigger_field_type, last_targeted_fields),
          /*minimal_fields_shown=*/1, app_locale, &differentiating_labels);
    }
  }
  return differentiating_labels;
}

// For each profile in `profiles`, returns a vector of `Suggestion::labels` to
// be applied. Takes into account the `last_targeted_fields` and the
// `trigger_field_type` to add specific granular filling labels. Optionally adds
// a differentiating label if the Suggestion::main_text + granular filling label
// is not unique.
std::vector<std::vector<Suggestion::Text>>
CreateSuggestionLabelsWithGranularFillingDetails(
    base::span<const Suggestion> suggestions,
    const std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>&
        profiles,
    const FieldTypeSet& field_types,
    std::optional<FieldTypeSet> last_targeted_fields,
    FieldType trigger_field_type,
    const std::string& app_locale) {
  // Suggestions for filling only one field (field-by-field filling, email group
  // filling, etc.) should not have labels because they are guaranteed to be
  // unique, see `DeduplicatedProfilesForSuggestions()`.
  // As an exception, when a user triggers autofill from the context menu on a
  // field which is not classified as an address, labels should be added because
  // the first-level suggestion is not clickable. The first-level suggestion
  // needs to give plenty of info about the profile.
  if (field_types.size() == 1 && IsAddressType(trigger_field_type) &&
      base::FeatureList::IsEnabled(
          features::kAutofillGranularFillingAvailable)) {
    return std::vector<std::vector<Suggestion::Text>>(profiles.size());
  }

  const std::vector<std::vector<std::u16string>>
      suggestions_granular_filling_labels = GetGranularFillingLabels(
          profiles, last_targeted_fields, trigger_field_type, app_locale);
  CHECK_EQ(suggestions_granular_filling_labels.size(), suggestions.size());

  const std::vector<std::u16string> suggestions_differentiating_labels =
      GetProfileSuggestionLabels(profiles, field_types, trigger_field_type,
                                 last_targeted_fields, app_locale);

  const std::map<std::u16string, size_t>
      main_text_and_granular_filling_label_count =
          GetNumberOfSuggestionMainTextAndGranularFillingLabelOcurrences(
              suggestions, suggestions_granular_filling_labels);

  // For each suggestion/profile, generate its label based on granular filling
  // and differentiating labels.
  std::vector<std::vector<Suggestion::Text>> suggestions_labels;
  suggestions_labels.reserve(suggestions.size());
  for (size_t i = 0; i < suggestions.size(); ++i) {
    const std::u16string& differentiating_label =
        suggestions_differentiating_labels[i];
    const std::vector<std::u16string>& granular_filling_labels =
        suggestions_granular_filling_labels[i];

    if (granular_filling_labels.empty()) {
      if (!differentiating_label.empty()) {
        // If only a differentiating label exists.
        //  _________________________
        // | Jon snow                |
        // | Winterfel               |
        // |_________________________|
        suggestions_labels.push_back({Suggestion::Text(differentiating_label)});
      } else {
        suggestions_labels.emplace_back();
      }
      continue;
    }

    CHECK_LE(granular_filling_labels.size(), 2u);
    // Note that when only one granular filling label exists we have.
    //  _________________________
    // | Jon snow                |
    // | Fill address            |
    // |_________________________|
    //
    //
    // When two granular filling labels exists, they are separated with  " - ".
    //  __________________________
    // | 8129                     |
    // | Fill address - winterfel |
    // |__________________________|
    suggestions_labels.push_back(
        {Suggestion::Text(base::JoinString(granular_filling_labels, u" - "))});

    // Check whether main_text + granular filling label is unique.
    auto main_text_and_granular_filling_label_count_iterator =
        main_text_and_granular_filling_label_count.find(
            suggestions[i].main_text.value +
            base::StrCat(granular_filling_labels));
    CHECK(main_text_and_granular_filling_label_count_iterator !=
          main_text_and_granular_filling_label_count.end());
    const bool needs_differentiating_label =
        !differentiating_label.empty() &&
        main_text_and_granular_filling_label_count_iterator->second > 1;

    if (!needs_differentiating_label) {
      // if main text + granular filling labels are unique or there is no
      // differentiating label, no need to add a differentiating label.
      continue;
    }

    if (granular_filling_labels.size() == 1) {
      // If only one granular filling label exist for the profile, the
      // differentiating label is separated from it using a " - ".
      //  ___________________________
      // | Winterfel                 |
      // | Fill address - 81274      |
      // |_________________________  |
      suggestions_labels.back().back().value += u" - " + differentiating_label;
    } else {
      // Otherwise using ", ".
      //  _________________________________
      // | 81274                           |
      // | Fill address - Winterfel, 81274 |
      // |_________________________________|
      //
      // Note that in this case, we add the differentiating label as a new
      // `Suggestion::Text`, so its possible to have the following format (in
      //  case the granular filling label is too large).
      //  _______________________________________
      // | 81274                                 |
      // | Fill address - Winterfel nor... 81274 |
      // |______________________________________ |
      suggestions_labels.back().back().value +=
          l10n_util::GetStringUTF16(IDS_AUTOFILL_ADDRESS_SUMMARY_SEPARATOR);
      suggestions_labels.back().emplace_back(differentiating_label);
    }
  }
  return suggestions_labels;
}

// Assigns for each suggestion labels to be used as secondary text in the
// suggestion bubble, and deduplicates suggestions having the same main text
// and label. For each vector in `labels`, the last value is used to
// differentiate profiles, while the others are granular filling specific
// labels, see `GetGranularFillingLabels()`. In the case where `labels` is
// empty, we have no differentiating label for the profile.
void AssignLabelsAndDeduplicate(
    std::vector<Suggestion>& suggestions,
    const std::vector<std::vector<Suggestion::Text>>& labels,
    const std::string& app_locale) {
  DCHECK_EQ(suggestions.size(), labels.size());
  std::set<std::u16string> suggestion_text;
  size_t index_to_add_suggestion = 0;
  const AutofillProfileComparator comparator(app_locale);

  // Dedupes Suggestions to show in the dropdown once values and labels have
  // been created. This is useful when LabelFormatters make Suggestions' labels.
  //
  // Suppose profile A has the data John, 400 Oak Rd, and (617) 544-7411 and
  // profile B has the data John, 400 Oak Rd, (508) 957-5009. If a formatter
  // puts only 400 Oak Rd in the label, then there will be two Suggestions with
  // the normalized text "john400oakrd", and the Suggestion with the lower
  // ranking should be discarded.
  for (size_t i = 0; i < labels.size(); ++i) {
    // If there are no labels, consider the `differentiating_label` as an empty
    // string.
    const std::u16string& differentiating_label =
        !labels[i].empty() ? labels[i].back().value : std::u16string();

    // For example, a Suggestion with the value "John" and the label "400 Oak
    // Rd" has the normalized text "john400oakrd".
    bool text_inserted =
        suggestion_text
            .insert(AutofillProfileComparator::NormalizeForComparison(
                suggestions[i].main_text.value + differentiating_label,
                AutofillProfileComparator::DISCARD_WHITESPACE))
            .second;

    if (text_inserted) {
      if (index_to_add_suggestion != i) {
        suggestions[index_to_add_suggestion] = suggestions[i];
      }
      // The given |suggestions| are already sorted from highest to lowest
      // ranking. Suggestions with lower indices have a higher ranking and
      // should be kept.
      //
      // We check whether the value and label are the same because in certain
      // cases, e.g. when a credit card form contains a zip code field and the
      // user clicks on the zip code, a suggestion's value and the label
      // produced for it may both be a zip code.
      if (!comparator.Compare(
              suggestions[index_to_add_suggestion].main_text.value,
              differentiating_label)) {
        if (!base::FeatureList::IsEnabled(
                features::kAutofillGranularFillingAvailable)) {
          if (!differentiating_label.empty()) {
            suggestions[index_to_add_suggestion].labels = {
                {Suggestion::Text(differentiating_label)}};
          }
        } else {
          // Note that `labels[i]` can be empty, this is possible for example in
          // the field by field filling case.
          suggestions[index_to_add_suggestion].labels.emplace_back(
              std::move(labels[i]));
        }
      }
      ++index_to_add_suggestion;
    }
  }

  if (index_to_add_suggestion < suggestions.size()) {
    suggestions.resize(index_to_add_suggestion);
  }
}

// Returns whether the `suggestion_canon` is a valid match given
// `field_contents_canon`. To be used for address suggestions
bool IsValidAddressSuggestionForFieldContents(
    std::u16string suggestion_canon,
    std::u16string field_contents_canon,
    FieldType trigger_field_type) {
  // Phones should do a substring match because they can be trimmed to remove
  // the first parts (e.g. country code or prefix).
  if (GroupTypeOfFieldType(trigger_field_type) == FieldTypeGroup::kPhone &&
      suggestion_canon.find(field_contents_canon) != std::u16string::npos) {
    return true;
  }
  return suggestion_canon.starts_with(field_contents_canon);
}

// Returns whether the `suggestion_canon` is a valid match given
// `field_contents_canon`. To be used for payments suggestions.
bool IsValidPaymentsSuggestionForFieldContents(
    std::u16string suggestion_canon,
    std::u16string field_contents_canon,
    FieldType trigger_field_type,
    bool is_masked_server_card,
    bool field_is_autofilled) {
  if (trigger_field_type != CREDIT_CARD_NUMBER) {
    return suggestion_canon.starts_with(field_contents_canon);
  }
  // For card number fields, suggest the card if:
  // - the number matches any part of the card, or
  // - it's a masked card and there are 6 or fewer typed so far.
  // - it's a masked card, field is autofilled, and the last 4 digits in the
  // field match the last 4 digits of the card.
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

// Normalizes text for comparison based on the type of the field `text` was
// entered into.
std::u16string NormalizeForComparisonForType(const std::u16string& text,
                                             FieldType type) {
  if (GroupTypeOfFieldType(type) == FieldTypeGroup::kEmail) {
    // For emails, keep special characters so that if the user has two emails
    // `test@foo.xyz` and `test1@foo.xyz` saved, only the first one is suggested
    // upon entering `test@` into the email field.
    return RemoveDiacriticsAndConvertToLowerCase(text);
  }
  return AutofillProfileComparator::NormalizeForComparison(text);
}

std::optional<Suggestion> GetSuggestionForTestAddresses(
    base::span<const AutofillProfile> test_addresses,
    const std::string& locale) {
  if (test_addresses.empty()) {
    return std::nullopt;
  }

  Suggestion suggestion(u"Devtools", PopupItemId::kDevtoolsTestAddresses);
  suggestion.labels = {{Suggestion::Text(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_ADDRESS_TEST_DATA))}};
  suggestion.icon = Suggestion::Icon::kCode;
  for (const AutofillProfile& test_address : test_addresses) {
    const std::u16string test_address_country =
        test_address.GetInfo(ADDRESS_HOME_COUNTRY, locale);
    suggestion.children.emplace_back(test_address_country,
                                     PopupItemId::kDevtoolsTestAddressEntry);
    suggestion.children.back().payload = Suggestion::Guid(test_address.guid());
    suggestion.children.back().acceptance_a11y_announcement =
        l10n_util::GetStringFUTF16(IDS_AUTOFILL_TEST_ADDRESS_SELECTED_A11Y_HINT,
                                   test_address_country);
  }
  return suggestion;
}

Suggestion::Text GetBenefitTextWithTermsAppended(
    const std::u16string& benefit_text) {
  return Suggestion::Text(l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_CREDIT_CARD_BENEFIT_TEXT_FOR_SUGGESTIONS, benefit_text));
}

}  // namespace

AutofillSuggestionGenerator::AutofillSuggestionGenerator(
    AutofillClient& autofill_client,
    PersonalDataManager& personal_data)
    : autofill_client_(autofill_client), personal_data_(personal_data) {}

AutofillSuggestionGenerator::~AutofillSuggestionGenerator() = default;

std::vector<Suggestion> AutofillSuggestionGenerator::GetSuggestionsForProfiles(
    const FieldTypeSet& field_types,
    const FormFieldData& trigger_field,
    FieldType trigger_field_type,
    std::optional<FieldTypeSet> last_targeted_fields,
    AutofillSuggestionTriggerSource trigger_source) {
  // If the user manually triggered suggestions from the context menu, all
  // available profiles should be shown. Selecting a suggestion overwrites the
  // triggering field's value.
  const std::u16string field_value_for_filtering =
      trigger_source != AutofillSuggestionTriggerSource::kManualFallbackAddress
          ? trigger_field.value
          : u"";

  std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>
      profiles_to_suggest =
          GetProfilesToSuggest(trigger_field_type, field_value_for_filtering,
                               trigger_field.is_autofilled, field_types);

  // Find the profiles that were hidden prior to the effects of the feature
  // kAutofillUseAddressRewriterInProfileSubsetComparison.
  std::set<std::string> previously_hidden_profiles_guid;
  for (const AutofillProfile* profile : profiles_to_suggest) {
    previously_hidden_profiles_guid.insert(profile->guid());
  }
  constexpr FieldTypeSet street_address_field_types = {
      ADDRESS_HOME_STREET_ADDRESS, ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE2,
      ADDRESS_HOME_LINE3};
  FieldTypeSet field_types_without_address_types = field_types;
  field_types_without_address_types.erase_all(street_address_field_types);

  // Autofill already considers suggestions as different if the suggestion's
  // main text, to be filled in the triggering field, differs regardless of
  // the other fields.
  std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>
      previously_suggested_profiles =
          street_address_field_types.contains(trigger_field_type)
              ? profiles_to_suggest
              : GetProfilesToSuggest(trigger_field_type,
                                     field_value_for_filtering,
                                     trigger_field.is_autofilled,
                                     field_types_without_address_types);
  for (const AutofillProfile* profile : previously_suggested_profiles) {
    previously_hidden_profiles_guid.erase(profile->guid());
  }
  autofill_metrics::LogPreviouslyHiddenProfileSuggestionNumber(
      previously_hidden_profiles_guid.size());

  std::vector<Suggestion> suggestions = CreateSuggestionsFromProfiles(
      profiles_to_suggest, field_types, last_targeted_fields,
      trigger_field_type, trigger_field.max_length,
      previously_hidden_profiles_guid);

  if (suggestions.empty()) {
    return suggestions;
  }

  base::ranges::move(GetAddressFooterSuggestions(trigger_field.is_autofilled),
                     std::back_inserter(suggestions));

  return suggestions;
}

std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>
AutofillSuggestionGenerator::GetProfilesToSuggest(
    FieldType trigger_field_type,
    const std::u16string& field_contents,
    bool field_is_autofilled,
    const FieldTypeSet& field_types) {
  std::u16string field_contents_canon =
      NormalizeForComparisonForType(field_contents, trigger_field_type);

  // Get the profiles to suggest, which are already sorted.
  std::vector<AutofillProfile*> sorted_profiles =
      personal_data_->GetProfilesToSuggest();

  // When suggesting with no prefix to match, suppress disused address
  // suggestions as well as those based on invalid profile data.
  if (field_contents_canon.empty()) {
    const base::Time min_last_used =
        AutofillClock::Now() - kDisusedDataModelTimeDelta;
    RemoveProfilesNotUsedSinceTimestamp(min_last_used, sorted_profiles);
  }

  std::vector<const AutofillProfile*> matched_profiles =
      GetPrefixMatchedProfiles(sorted_profiles, trigger_field_type,
                               field_contents, field_contents_canon,
                               field_is_autofilled);

  const AutofillProfileComparator comparator(personal_data_->app_locale());
  // Don't show two suggestions if one is a subset of the other.
  // Duplicates across sources are resolved in favour of `kAccount` profiles.
  std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>
      unique_matched_profiles = DeduplicatedProfilesForSuggestions(
          matched_profiles, trigger_field_type, field_types, comparator);

  return unique_matched_profiles;
}

std::vector<Suggestion>
AutofillSuggestionGenerator::CreateSuggestionsFromProfiles(
    const std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>&
        profiles,
    const FieldTypeSet& field_types,
    std::optional<FieldTypeSet> last_targeted_fields,
    FieldType trigger_field_type,
    uint64_t trigger_field_max_length,
    const std::set<std::string>& previously_hidden_profiles_guid) {
  std::vector<Suggestion> suggestions;
  std::string app_locale = personal_data_->app_locale();

  // This will be used to check if suggestions should be supported with icons.
  const bool contains_profile_related_fields =
      base::ranges::count_if(field_types, [](FieldType field_type) {
        FieldTypeGroup field_type_group = GroupTypeOfFieldType(field_type);
        return field_type_group == FieldTypeGroup::kName ||
               field_type_group == FieldTypeGroup::kAddress ||
               field_type_group == FieldTypeGroup::kPhone ||
               field_type_group == FieldTypeGroup::kEmail;
      }) > 1;

  FieldTypeGroup trigger_field_type_group =
      GroupTypeOfFieldType(trigger_field_type);
  for (const AutofillProfile* profile : profiles) {
    // Name fields should have `NAME_FULL` as main text, unless in field by
    // field filling mode.
    const PopupItemId popup_item_id = GetProfileSuggestionPopupItemId(
        last_targeted_fields, trigger_field_type);
    FieldType main_text_field_type =
        GroupTypeOfFieldType(trigger_field_type) == FieldTypeGroup::kName &&
                popup_item_id != PopupItemId::kAddressFieldByFieldFilling &&
                base::FeatureList::IsEnabled(
                    features::kAutofillGranularFillingAvailable)
            ? NAME_FULL
            : trigger_field_type;
    // Compute the main text to be displayed in the suggestion bubble.
    std::u16string main_text = GetProfileSuggestionMainText(
        *profile, app_locale, main_text_field_type);
    if (trigger_field_type_group == FieldTypeGroup::kPhone) {
      main_text = GetFormattedPhoneNumber(
          *profile, app_locale,
          ShouldUseNationalFormatPhoneNumber(trigger_field_type));
    }

    suggestions.emplace_back(main_text);
    suggestions.back().payload = Suggestion::Guid(profile->guid());
    suggestions.back().acceptance_a11y_announcement =
        l10n_util::GetStringUTF16(IDS_AUTOFILL_A11Y_ANNOUNCE_FILLED_FORM);
    suggestions.back().popup_item_id = popup_item_id;
    suggestions.back().is_acceptable = IsAddressType(trigger_field_type);
    suggestions.back().hidden_prior_to_address_rewriter_usage =
        previously_hidden_profiles_guid.contains(profile->guid());
    if (suggestions.back().popup_item_id ==
        PopupItemId::kAddressFieldByFieldFilling) {
      suggestions.back().field_by_field_filling_type_used =
          std::optional(trigger_field_type);
    }
    // We add an icon to the address (profile) suggestion if there is more than
    // one profile related field in the form.
    if (contains_profile_related_fields) {
      const bool fill_full_form =
          suggestions.back().popup_item_id == PopupItemId::kAddressEntry;
      if (base::FeatureList::IsEnabled(
              features::kAutofillGranularFillingAvailable)) {
        suggestions.back().icon = fill_full_form ? Suggestion::Icon::kLocation
                                                 : Suggestion::Icon::kNoIcon;
      } else {
        suggestions.back().icon = Suggestion::Icon::kAccount;
      }
    }

    if (profile && profile->source() == AutofillProfile::Source::kAccount &&
        profile->initial_creator_id() !=
            AutofillProfile::kInitialCreatorOrModifierChrome) {
      suggestions.back().feature_for_iph =
          feature_engagement::
              kIPHAutofillExternalAccountProfileSuggestionFeature.name;
    }

    if (base::FeatureList::IsEnabled(
            features::kAutofillGranularFillingAvailable)) {
      // TODO(crbug.com/1502162): Make the granular filling options vary
      // depending on the locale.
      AddAddressGranularFillingChildSuggestions(last_targeted_fields,
                                                trigger_field_type, *profile,
                                                suggestions.back());
    }
  }

  AssignLabelsAndDeduplicate(
      suggestions,
      CreateSuggestionLabelsWithGranularFillingDetails(
          suggestions, profiles, field_types, last_targeted_fields,
          trigger_field_type, app_locale),
      app_locale);

  // Add devtools test addresses suggestion if it exists. A suggestion will
  // exist if devtools is open and therefore test addresses were set.
  if (std::optional<Suggestion> test_addresses_suggestion =
          GetSuggestionForTestAddresses(personal_data_->test_addresses(),
                                        app_locale)) {
    std::vector<Suggestion> suggestions_with_test_address;
    suggestions_with_test_address.push_back(
        std::move(*test_addresses_suggestion));
    suggestions_with_test_address.insert(suggestions_with_test_address.end(),
                                         suggestions.begin(),
                                         suggestions.end());
    return suggestions_with_test_address;
  }

  return suggestions;
}

// TODO(crbug.com/1417975): Remove `trigger_field_type` when
// `kAutofillUseAddressRewriterInProfileSubsetComparison` launches.
std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>
AutofillSuggestionGenerator::DeduplicatedProfilesForSuggestions(
    const std::vector<const AutofillProfile*>& matched_profiles,
    FieldType trigger_field_type,
    const FieldTypeSet& field_types,
    const AutofillProfileComparator& comparator) {
  // TODO(crbug.com/1417975): Remove when
  // `kAutofillUseAddressRewriterInProfileSubsetComparison` launches.
  std::vector<std::u16string> suggestion_main_text;
  for (const AutofillProfile* profile : matched_profiles) {
    suggestion_main_text.push_back(GetProfileSuggestionMainText(
        *profile, personal_data_->app_locale(), trigger_field_type));
  }

  std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>
      unique_matched_profiles;
  // Limit number of unique profiles as having too many makes the
  // browser hang due to drawing calculations (and is also not
  // very useful for the user).
  for (size_t a = 0;
       a < matched_profiles.size() &&
       unique_matched_profiles.size() < kMaxUniqueSuggestedProfilesCount;
       ++a) {
    bool include = true;
    const AutofillProfile* profile_a = matched_profiles[a];
    for (size_t b = 0; b < matched_profiles.size(); ++b) {
      const AutofillProfile* profile_b = matched_profiles[b];

      // TODO(crbug.com/1417975): Remove when
      // `kAutofillUseAddressRewriterInProfileSubsetComparison` launches.
      if (profile_a == profile_b ||
          !comparator.Compare(suggestion_main_text[a],
                              suggestion_main_text[b])) {
        continue;
      }

      if (!profile_a->IsSubsetOfForFieldSet(comparator, *profile_b,
                                            field_types)) {
        continue;
      }

      if (!profile_b->IsSubsetOfForFieldSet(comparator, *profile_a,
                                            field_types)) {
        // One-way subset. Don't include profile A.
        include = false;
        break;
      }

      // The profiles are identical and only one should be included.
      // Prefer `kAccount` profiles over `kLocalOrSyncable` ones. In case the
      // profiles have the same source, prefer the earlier one (since the
      // profiles are pre-sorted by their relevance).
      const bool prefer_a_over_b =
          profile_a->source() == profile_b->source()
              ? a < b
              : profile_a->source() == AutofillProfile::Source::kAccount;
      if (!prefer_a_over_b) {
        include = false;
        break;
      }
    }
    if (include) {
      unique_matched_profiles.push_back(profile_a);
    }
  }
  return unique_matched_profiles;
}

std::vector<const AutofillProfile*>
AutofillSuggestionGenerator::GetPrefixMatchedProfiles(
    const std::vector<AutofillProfile*>& profiles,
    FieldType trigger_field_type,
    const std::u16string& raw_field_contents,
    const std::u16string& field_contents_canon,
    bool field_is_autofilled) {
  std::vector<const AutofillProfile*> matched_profiles;
  for (const AutofillProfile* profile : profiles) {
    if (matched_profiles.size() == kMaxSuggestedProfilesCount) {
      break;
    }
    // Don't offer to fill the exact same value again. If detailed suggestions
    // with different secondary data is available, it would appear to offer
    // refilling the whole form with something else. E.g. the same name with a
    // work and a home address would appear twice but a click would be a noop.
    // TODO(fhorschig): Consider refilling form instead (at least on Android).
#if BUILDFLAG(IS_ANDROID)
    if (field_is_autofilled &&
        profile->GetRawInfo(trigger_field_type) == raw_field_contents) {
      continue;
    }
#endif  // BUILDFLAG(IS_ANDROID)
    std::u16string main_text = GetProfileSuggestionMainText(
        *profile, personal_data_->app_locale(), trigger_field_type);
    // Discard profiles that do not have a value for the trigger field.
    if (main_text.empty()) {
      continue;
    }
    std::u16string suggestion_canon =
        NormalizeForComparisonForType(main_text, trigger_field_type);
    if (IsValidAddressSuggestionForFieldContents(
            suggestion_canon, field_contents_canon, trigger_field_type)) {
      matched_profiles.push_back(profile);
    }
  }
  return matched_profiles;
}

void AutofillSuggestionGenerator::RemoveProfilesNotUsedSinceTimestamp(
    base::Time min_last_used,
    std::vector<AutofillProfile*>& profiles) {
  const size_t original_size = profiles.size();
  std::erase_if(profiles, [min_last_used](const AutofillProfile* profile) {
    return profile->use_date() <= min_last_used;
  });
  const size_t num_profiles_suppressed = original_size - profiles.size();
  AutofillMetrics::LogNumberOfAddressesSuppressedForDisuse(
      num_profiles_suppressed);
}

void AutofillSuggestionGenerator::AddAddressGranularFillingChildSuggestions(
    std::optional<FieldTypeSet> last_targeted_fields,
    FieldType trigger_field_type,
    const AutofillProfile& profile,
    Suggestion& suggestion) const {
  const FieldTypeGroup trigger_field_type_group =
      GroupTypeOfFieldType(trigger_field_type);
  const std::string app_locale = personal_data_->app_locale();
  AddNameChildSuggestions(trigger_field_type_group, profile, app_locale,
                          suggestion);
  AddAddressChildSuggestions(trigger_field_type_group, profile, app_locale,
                             suggestion);
  AddContactChildSuggestions(trigger_field_type, profile, app_locale,
                             suggestion);
  AddFooterChildSuggestions(profile, trigger_field_type, last_targeted_fields,
                            suggestion);
}

std::vector<Suggestion>
AutofillSuggestionGenerator::GetSuggestionsForCreditCards(
    const FormFieldData& trigger_field,
    FieldType trigger_field_type,
    AutofillSuggestionTriggerSource trigger_source,
    bool should_show_scan_credit_card,
    bool should_show_cards_from_account,
    bool& with_offer,
    bool& with_cvc,
    autofill_metrics::CardMetadataLoggingContext& metadata_logging_context) {
  std::vector<Suggestion> suggestions;
  // Manual fallback entries are shown for all non credit card fields.
  const bool is_manual_fallback_for_non_credit_card_field =
      GroupTypeOfFieldType(trigger_field_type) != FieldTypeGroup::kCreditCard;
  const std::string& app_locale = personal_data_->app_locale();

  std::map<std::string, AutofillOfferData*> card_linked_offers_map =
      GetCardLinkedOffers(*autofill_client_);
  with_offer = !card_linked_offers_map.empty();

  // The field value is sanitized before attempting to match it to the user's
  // data.
  auto field_contents = SanitizeCreditCardFieldValue(trigger_field.value);

  std::vector<CreditCard> cards_to_suggest = GetOrderedCardsToSuggest(
      *autofill_client_,
      field_contents.empty() &&
          trigger_source !=
              AutofillSuggestionTriggerSource::kManualFallbackPayments);

  std::u16string field_contents_lower = base::i18n::ToLower(field_contents);

  metadata_logging_context =
      autofill_metrics::GetMetadataLoggingContext(cards_to_suggest);

  for (const CreditCard& credit_card : cards_to_suggest) {
    // The value of the stored data for this field type in the |credit_card|.
    std::u16string creditcard_field_value =
        credit_card.GetInfo(trigger_field_type, app_locale);
    if (!is_manual_fallback_for_non_credit_card_field &&
        creditcard_field_value.empty()) {
      continue;
    }
    // Manual fallback suggestions aren't filtered based on the field's content.
    if (is_manual_fallback_for_non_credit_card_field ||
        IsValidPaymentsSuggestionForFieldContents(
            base::i18n::ToLower(creditcard_field_value), field_contents_lower,
            trigger_field_type,
            credit_card.record_type() ==
                CreditCard::RecordType::kMaskedServerCard,
            trigger_field.is_autofilled)) {
      bool card_linked_offer_available =
          base::Contains(card_linked_offers_map, credit_card.guid());
      if (ShouldShowVirtualCardOption(&credit_card)) {
        suggestions.push_back(CreateCreditCardSuggestion(
            credit_card, trigger_field_type,
            /*virtual_card_option=*/true, card_linked_offer_available,
            trigger_field.origin));
      }
      if (!credit_card.cvc().empty()) {
        with_cvc = true;
      }
      suggestions.push_back(CreateCreditCardSuggestion(
          credit_card, trigger_field_type,
          /*virtual_card_option=*/false, card_linked_offer_available,
          trigger_field.origin));
    }
  }

  if (suggestions.empty()) {
    return suggestions;
  }

  const bool display_gpay_logo = base::ranges::none_of(
      cards_to_suggest,
      [](const CreditCard& card) { return CreditCard::IsLocalCard(&card); });
  base::ranges::move(
      GetCreditCardFooterSuggestions(
          should_show_scan_credit_card, should_show_cards_from_account,
          trigger_field.is_autofilled, display_gpay_logo),
      std::back_inserter(suggestions));

  return suggestions;
}

std::vector<Suggestion>
AutofillSuggestionGenerator::GetSuggestionsForVirtualCardStandaloneCvc(
    const FormFieldData& trigger_field,
    autofill_metrics::CardMetadataLoggingContext& metadata_logging_context,
    base::flat_map<std::string, VirtualCardUsageData::VirtualCardLastFour>&
        virtual_card_guid_to_last_four_map) {
  // TODO(crbug.com/1453739): Refactor credit card suggestion code by moving
  // duplicate logic to helper functions.
  std::vector<Suggestion> suggestions;
  std::vector<CreditCard> cards_to_suggest = GetOrderedCardsToSuggest(
      *autofill_client_, /*suppress_disused_cards=*/true);
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
    suggestion.popup_item_id = PopupItemId::kVirtualCreditCardEntry;
    suggestion.payload = Suggestion::Guid(credit_card.guid());
    suggestion.feature_for_iph =
        feature_engagement::kIPHAutofillVirtualCardCVCSuggestionFeature.name;
    SetCardArtURL(suggestion, credit_card, /*virtual_card_option=*/true);
    // TODO(crbug.com/1511277): Create translation string for standalone CVC
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
                                     trigger_field.is_autofilled,
                                     /*with_gpay_logo=*/true),
      std::back_inserter(suggestions));

  return suggestions;
}

// static
Suggestion AutofillSuggestionGenerator::CreateSeparator() {
  Suggestion suggestion;
  suggestion.popup_item_id = PopupItemId::kSeparator;
  return suggestion;
}

// static
Suggestion AutofillSuggestionGenerator::CreateManageAddressesEntry() {
  Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_ADDRESSES),
      PopupItemId::kAutofillOptions);
  suggestion.icon = Suggestion::Icon::kSettings;
  return suggestion;
}

// static
Suggestion AutofillSuggestionGenerator::CreateManagePaymentMethodsEntry(
    bool with_gpay_logo) {
  Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_PAYMENT_METHODS),
      PopupItemId::kAutofillOptions);
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

Suggestion AutofillSuggestionGenerator::CreateClearFormSuggestion() {
  std::u16string value =
      base::FeatureList::IsEnabled(features::kAutofillUndo)
          ? l10n_util::GetStringUTF16(IDS_AUTOFILL_UNDO_MENU_ITEM)
          : l10n_util::GetStringUTF16(IDS_AUTOFILL_CLEAR_FORM_MENU_ITEM);
  if constexpr (BUILDFLAG(IS_ANDROID)) {
    value = base::i18n::ToUpper(value);
  }

  Suggestion suggestion(value, PopupItemId::kClearForm);
  suggestion.icon = base::FeatureList::IsEnabled(features::kAutofillUndo)
                        ? Suggestion::Icon::kUndo
                        : Suggestion::Icon::kClear;
  suggestion.acceptance_a11y_announcement =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_A11Y_ANNOUNCE_CLEARED_FORM);
  return suggestion;
}

// static
std::vector<CreditCard> AutofillSuggestionGenerator::GetOrderedCardsToSuggest(
    AutofillClient& autofill_client,
    bool suppress_disused_cards) {
  std::map<std::string, AutofillOfferData*> card_linked_offers_map =
      GetCardLinkedOffers(autofill_client);

  const PersonalDataManager& personal_data =
      CHECK_DEREF(autofill_client.GetPersonalDataManager());
  std::vector<CreditCard*> available_cards =
      personal_data.GetCreditCardsToSuggest();

  // If a card has available card linked offers on the last committed url, rank
  // it to the top.
  if (!card_linked_offers_map.empty()) {
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
    AutofillSuggestionGenerator::
        RemoveExpiredLocalCreditCardsNotUsedSinceTimestamp(min_last_used,
                                                           available_cards);
  }

  std::vector<CreditCard> cards_to_suggest;
  cards_to_suggest.reserve(available_cards.size());
  for (const CreditCard* card : available_cards) {
    cards_to_suggest.push_back(*card);
  }
  return cards_to_suggest;
}

// static
std::vector<Suggestion> AutofillSuggestionGenerator::GetSuggestionsForIbans(
    const std::vector<const Iban*>& ibans) {
  std::vector<Suggestion> suggestions;
  suggestions.reserve(ibans.size() + 2);
  for (const Iban* iban : ibans) {
    Suggestion& suggestion =
        suggestions.emplace_back(iban->GetIdentifierStringForAutofillDisplay());
    suggestion.custom_icon =
        ui::ResourceBundle::GetSharedInstance().GetImageNamed(
            IDR_AUTOFILL_IBAN);
    suggestion.popup_item_id = PopupItemId::kIbanEntry;
    if (iban->record_type() == Iban::kLocalIban) {
      suggestion.payload =
          Suggestion::BackendId(Suggestion::Guid(iban->guid()));
    } else {
      CHECK(iban->record_type() == Iban::kServerIban);
      suggestion.payload = Suggestion::BackendId(
          Suggestion::InstrumentId(iban->instrument_id()));
    }
    if (!iban->nickname().empty())
      suggestion.labels = {{Suggestion::Text(iban->nickname())}};
  }

  if (suggestions.empty()) {
    return suggestions;
  }

  suggestions.push_back(CreateSeparator());
  suggestions.push_back(
      CreateManagePaymentMethodsEntry(/*with_gpay_logo=*/false));
  return suggestions;
}

// static
std::vector<Suggestion>
AutofillSuggestionGenerator::GetPromoCodeSuggestionsFromPromoCodeOffers(
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
    suggestion.popup_item_id = PopupItemId::kMerchantPromoCodeEntry;

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
    suggestion.popup_item_id = PopupItemId::kSeePromoCodeDetails;

    // We set the payload for the footer as |footer_offer_details_url|, which is
    // the offer details url of the first offer we had for this merchant. We
    // will navigate to the url in |footer_offer_details_url| if the footer is
    // selected in AutofillExternalDelegate::DidAcceptSuggestion().
    suggestion.payload = std::move(footer_offer_details_url);
    suggestion.trailing_icon = Suggestion::Icon::kGoogle;
  }
  return suggestions;
}

// static
void AutofillSuggestionGenerator::
    RemoveExpiredLocalCreditCardsNotUsedSinceTimestamp(
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

std::u16string AutofillSuggestionGenerator::GetDisplayNicknameForCreditCard(
    const CreditCard& card) const {
  // Always prefer a local nickname if available.
  if (card.HasNonEmptyValidNickname() &&
      card.record_type() == CreditCard::RecordType::kLocalCard) {
    return card.nickname();
  }
  // Either the card a) has no nickname or b) is a server card and we would
  // prefer to use the nickname of a local card.
  std::vector<CreditCard*> candidates = personal_data_->GetCreditCards();
  for (CreditCard* candidate : candidates) {
    if (candidate->guid() != card.guid() &&
        candidate->MatchingCardDetails(card) &&
        candidate->HasNonEmptyValidNickname()) {
      return candidate->nickname();
    }
  }
  // Fall back to nickname of |card|, which may be empty.
  return card.nickname();
}

bool AutofillSuggestionGenerator::ShouldShowVirtualCardOption(
    const CreditCard* candidate_card) const {
  switch (candidate_card->record_type()) {
    case CreditCard::RecordType::kLocalCard:
      candidate_card =
          personal_data_->GetServerCardForLocalCard(candidate_card);

      // If we could not find a matching server duplicate, return false.
      if (!candidate_card) {
        return false;
      }
      ABSL_FALLTHROUGH_INTENDED;
    case CreditCard::RecordType::kMaskedServerCard:
      return ShouldShowVirtualCardOptionForServerCard(candidate_card);
    case CreditCard::RecordType::kFullServerCard:
      return false;
    case CreditCard::RecordType::kVirtualCard:
      // Should not happen since virtual card is not persisted.
      NOTREACHED();
      return false;
  }
}

// TODO(crbug.com/1346331): Separate logic for desktop, Android dropdown, and
// Keyboard Accessory.
Suggestion AutofillSuggestionGenerator::CreateCreditCardSuggestion(
    const CreditCard& credit_card,
    FieldType trigger_field_type,
    bool virtual_card_option,
    bool card_linked_offer_available,
    const url::Origin& origin) const {
  // Manual fallback entries are shown for all non credit card fields.
  const bool is_manual_fallback =
      GroupTypeOfFieldType(trigger_field_type) != FieldTypeGroup::kCreditCard;

  Suggestion suggestion;
  suggestion.icon = credit_card.CardIconForAutofillSuggestion();
  // First layer manual fallback entries can't fill forms and thus can't be
  // selected by the user.
  suggestion.popup_item_id = PopupItemId::kCreditCardEntry;
  suggestion.is_acceptable = !is_manual_fallback;
  suggestion.payload = Suggestion::Guid(credit_card.guid());
#if BUILDFLAG(IS_ANDROID)
  // The card art icon should always be shown at the start of the suggestion.
  suggestion.is_icon_at_start = true;
#endif  // BUILDFLAG(IS_ANDROID)

  // Manual fallback suggestions labels are computed as if the triggering field
  // type was the credit card number.
  auto [main_text, minor_text] = GetSuggestionMainTextAndMinorTextForCard(
      credit_card,
      is_manual_fallback ? CREDIT_CARD_NUMBER : trigger_field_type);
  suggestion.main_text = std::move(main_text);
  suggestion.minor_text = std::move(minor_text);
  if (std::vector<Suggestion::Text> card_labels = GetSuggestionLabelsForCard(
          credit_card,
          is_manual_fallback ? CREDIT_CARD_NUMBER : trigger_field_type, origin);
      !card_labels.empty()) {
    suggestion.labels.push_back(std::move(card_labels));
  }

  SetCardArtURL(suggestion, credit_card, virtual_card_option);

  // For virtual cards, make some adjustments for the suggestion contents.
  if (virtual_card_option) {
    // We don't show card linked offers for virtual card options.
    AdjustVirtualCardSuggestionContent(suggestion, credit_card,
                                       trigger_field_type, origin);
  } else if (card_linked_offer_available) {
#if BUILDFLAG(IS_ANDROID)
    // For Keyboard Accessory, set Suggestion::feature_for_iph and change the
    // suggestion icon only if card linked offers are also enabled.
    if (base::FeatureList::IsEnabled(
            features::kAutofillEnableOffersInClankKeyboardAccessory)) {
      suggestion.feature_for_iph =
          feature_engagement::kIPHKeyboardAccessoryPaymentOfferFeature.name;
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
    AddPaymentsGranularFillingChildSuggestions(credit_card, suggestion);
    suggestion.acceptance_a11y_announcement = l10n_util::GetStringUTF16(
        IDS_AUTOFILL_A11Y_ANNOUNCE_EXPANDABLE_ONLY_ENTRY);
  } else {
    suggestion.acceptance_a11y_announcement =
        l10n_util::GetStringUTF16(IDS_AUTOFILL_A11Y_ANNOUNCE_FILLED_FORM);
  }

  return suggestion;
}

void AutofillSuggestionGenerator::AddPaymentsGranularFillingChildSuggestions(
    const CreditCard& credit_card,
    Suggestion& suggestion) const {
  const std::string& app_locale = personal_data_->app_locale();

  bool has_content_above =
      AddCreditCardNameChildSuggestion(credit_card, app_locale, suggestion);
  has_content_above |=
      AddCreditCardNumberChildSuggestion(credit_card, app_locale, suggestion);

  if (credit_card.HasInfo(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR)) {
    if (has_content_above) {
      suggestion.children.push_back(
          AutofillSuggestionGenerator::CreateSeparator());
    }
    AddCreditCardExpiryDateChildSuggestion(credit_card, app_locale, suggestion);
  }
}

std::pair<Suggestion::Text, Suggestion::Text>
AutofillSuggestionGenerator::GetSuggestionMainTextAndMinorTextForCard(
    const CreditCard& credit_card,
    FieldType trigger_field_type) const {
  std::u16string main_text;
  std::u16string minor_text;
  if (trigger_field_type == CREDIT_CARD_NUMBER) {
    std::u16string nickname = GetDisplayNicknameForCreditCard(credit_card);
    if (ShouldSplitCardNameAndLastFourDigits()) {
      main_text = credit_card.CardNameForAutofillDisplay(nickname);
      minor_text = credit_card.ObfuscatedNumberWithVisibleLastFourDigits(
          GetObfuscationLength());
    } else {
      main_text = credit_card.CardNameAndLastFourDigits(nickname,
                                                        GetObfuscationLength());
    }
  } else if (trigger_field_type == CREDIT_CARD_VERIFICATION_CODE) {
    CHECK(!credit_card.cvc().empty());
#if BUILDFLAG(IS_ANDROID)
    main_text = l10n_util::GetStringFUTF16(
        IDS_AUTOFILL_CVC_SUGGESTION_MAIN_TEXT,
        credit_card.CardNameForAutofillDisplay(
            GetDisplayNicknameForCreditCard(credit_card)));
#else
    main_text =
        l10n_util::GetStringUTF16(IDS_AUTOFILL_CVC_SUGGESTION_MAIN_TEXT);
#endif
  } else {
    main_text =
        credit_card.GetInfo(trigger_field_type, personal_data_->app_locale());
  }

  return {Suggestion::Text(main_text, Suggestion::Text::IsPrimary(true),
                           Suggestion::Text::ShouldTruncate(
                               ShouldSplitCardNameAndLastFourDigits())),
          // minor_text should also be shown in primary style, since it is also
          // on the first line.
          Suggestion::Text(minor_text, Suggestion::Text::IsPrimary(true))};
}

std::vector<Suggestion::Text>
AutofillSuggestionGenerator::GetSuggestionLabelsForCard(
    const CreditCard& credit_card,
    FieldType trigger_field_type,
    const url::Origin& origin) const {
  const std::string& app_locale = personal_data_->app_locale();

  // If the focused field is a card number field.
  if (trigger_field_type == CREDIT_CARD_NUMBER) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    return {Suggestion::Text(
        credit_card.GetInfo(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, app_locale))};
#else
    std::optional<Suggestion::Text> benefit_label =
        GetCreditCardBenefitSuggestionLabel(credit_card, origin);
    if (benefit_label) {
      return {*benefit_label};
    }
    return {Suggestion::Text(
        ShouldSplitCardNameAndLastFourDigits()
            ? credit_card.GetInfo(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR, app_locale)
            : credit_card.DescriptiveExpiration(app_locale))};
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  }

  // If the focused field is not a card number field AND the card number is
  // empty (i.e. local cards added via settings page).
  std::u16string nickname = GetDisplayNicknameForCreditCard(credit_card);
  if (credit_card.number().empty()) {
    DCHECK_EQ(credit_card.record_type(), CreditCard::RecordType::kLocalCard);

    if (credit_card.HasNonEmptyValidNickname())
      return {Suggestion::Text(nickname)};

    if (trigger_field_type != CREDIT_CARD_NAME_FULL) {
      return {Suggestion::Text(
          credit_card.GetInfo(CREDIT_CARD_NAME_FULL, app_locale))};
    }
    return {};
  }

  // If the focused field is not a card number field AND the card number is NOT
  // empty.

  if constexpr (BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)) {
    // On Mobile, the label is formatted as either "••••1234" or "••1234",
    // depending on the obfuscation length.
    return {
        Suggestion::Text(credit_card.ObfuscatedNumberWithVisibleLastFourDigits(
            GetObfuscationLength()))};
  }

  if (ShouldSplitCardNameAndLastFourDigits()) {
    // Format the label as "Product Description/Nickname/Network  ••••1234".
    // If the card name is too long, it will be truncated from the tail.
    return {
        Suggestion::Text(credit_card.CardNameForAutofillDisplay(nickname),
                         Suggestion::Text::IsPrimary(false),
                         Suggestion::Text::ShouldTruncate(true)),
        Suggestion::Text(credit_card.ObfuscatedNumberWithVisibleLastFourDigits(
            GetObfuscationLength()))};
  }

  // Format the label as
  // "Product Description/Nickname/Network  ••••1234, expires on 01/25".
  return {Suggestion::Text(
      credit_card.CardIdentifierStringAndDescriptiveExpiration(app_locale))};
}

std::optional<Suggestion::Text>
AutofillSuggestionGenerator::GetCreditCardBenefitSuggestionLabel(
    const CreditCard& credit_card,
    const url::Origin& origin) const {
  // Benefits are only displayed for app locale set to U.S. English.
  if (!base::FeatureList::IsEnabled(features::kAutofillEnableCardBenefits) ||
      personal_data_->app_locale() != "en-US") {
    return std::nullopt;
  }
  CreditCardBenefitBase::LinkedCardInstrumentId benefit_instrument_id(
      credit_card.instrument_id());

  // 1. Check merchant benefit.
  std::optional<CreditCardMerchantBenefit> merchant_benefit =
      personal_data_->GetMerchantBenefitByInstrumentIdAndOrigin(
          benefit_instrument_id, origin);
  if (merchant_benefit && merchant_benefit->IsActiveBenefit()) {
    return GetBenefitTextWithTermsAppended(
        merchant_benefit->benefit_description());
  }

  // 2. Check category benefit.
  CreditCardCategoryBenefit::BenefitCategory category_benefit_type =
      autofill_client_->GetAutofillOptimizationGuide()
          ->AttemptToGetEligibleCreditCardBenefitCategory(
              credit_card.issuer_id(), origin);
  if (category_benefit_type !=
      CreditCardCategoryBenefit::BenefitCategory::kUnknownBenefitCategory) {
    std::optional<CreditCardCategoryBenefit> category_benefit =
        personal_data_->GetCategoryBenefitByInstrumentIdAndCategory(
            benefit_instrument_id, category_benefit_type);
    if (category_benefit && category_benefit->IsActiveBenefit()) {
      return GetBenefitTextWithTermsAppended(
          category_benefit->benefit_description());
    }
  }

  // 3. Check flat rate benefit.
  std::optional<CreditCardFlatRateBenefit> flat_rate_benefit =
      personal_data_->GetFlatRateBenefitByInstrumentId(benefit_instrument_id);
  if (flat_rate_benefit && flat_rate_benefit->IsActiveBenefit()) {
    return GetBenefitTextWithTermsAppended(
        flat_rate_benefit->benefit_description());
  }

  // No eligible benefit to display.
  return std::nullopt;
}

void AutofillSuggestionGenerator::AdjustVirtualCardSuggestionContent(
    Suggestion& suggestion,
    const CreditCard& credit_card,
    FieldType trigger_field_type,
    const url::Origin& origin) const {
  if (credit_card.record_type() == CreditCard::RecordType::kLocalCard) {
    const CreditCard* server_duplicate_card =
        personal_data_->GetServerCardForLocalCard(&credit_card);
    DCHECK(server_duplicate_card);
    suggestion.payload = Suggestion::Guid(server_duplicate_card->guid());
  }

  suggestion.popup_item_id = PopupItemId::kVirtualCreditCardEntry;
  suggestion.is_acceptable = true;
  suggestion.feature_for_iph =
      feature_engagement::kIPHAutofillVirtualCardSuggestionFeature.name;

  // Add virtual card labelling to suggestions. For keyboard accessory, it is
  // prefixed to the suggestion, and for the dropdown, it is shown as a label on
  // a separate line.
  const std::u16string& VIRTUAL_CARD_LABEL = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_VIRTUAL_CARD_SUGGESTION_OPTION_VALUE);
  if (!base::FeatureList::IsEnabled(
          features::kAutofillEnableVirtualCardMetadata)) {
    suggestion.minor_text.value = suggestion.main_text.value;
    suggestion.main_text.value = VIRTUAL_CARD_LABEL;
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
          base::StrCat({VIRTUAL_CARD_LABEL, u"  ", suggestion.main_text.value});
    } else {
      suggestion.minor_text.value = suggestion.main_text.value;
      suggestion.main_text.value = VIRTUAL_CARD_LABEL;
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
          GetCreditCardBenefitSuggestionLabel(credit_card, origin);
      if (benefit_label) {
        suggestion.labels.push_back({*benefit_label});
      }
    }
    suggestion.labels.push_back(
        std::vector<Suggestion::Text>{Suggestion::Text(VIRTUAL_CARD_LABEL)});
#endif  // BUILDFLAG(IS_ANDROID)
  }
}

void AutofillSuggestionGenerator::SetCardArtURL(
    Suggestion& suggestion,
    const CreditCard& credit_card,
    bool virtual_card_option) const {
  const GURL card_art_url = personal_data_->GetCardArtURL(credit_card);

  if (card_art_url.is_empty() || !card_art_url.is_valid())
    return;

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
#if BUILDFLAG(IS_ANDROID)
    suggestion.custom_icon_url = card_art_url;
#else
    gfx::Image* image =
        personal_data_->GetCreditCardArtImageForUrl(card_art_url);
    if (image) {
      suggestion.custom_icon = *image;
    }
#endif
  }
}

std::vector<Suggestion>
AutofillSuggestionGenerator::GetAddressFooterSuggestions(
    bool is_autofilled) const {
  std::vector<Suggestion> footer_suggestions;

  footer_suggestions.push_back(CreateSeparator());

  if (is_autofilled) {
    footer_suggestions.push_back(CreateClearFormSuggestion());
  }

  footer_suggestions.push_back(CreateManageAddressesEntry());

  return footer_suggestions;
}

std::vector<Suggestion>
AutofillSuggestionGenerator::GetCreditCardFooterSuggestions(
    bool should_show_scan_credit_card,
    bool should_show_cards_from_account,
    bool is_autofilled,
    bool with_gpay_logo) const {
  std::vector<Suggestion> footer_suggestions;
  if (should_show_scan_credit_card) {
    Suggestion scan_credit_card(
        l10n_util::GetStringUTF16(IDS_AUTOFILL_SCAN_CREDIT_CARD),
        PopupItemId::kScanCreditCard);
    scan_credit_card.icon = Suggestion::Icon::kScanCreditCard;
    footer_suggestions.push_back(scan_credit_card);
  }

  if (should_show_cards_from_account) {
    Suggestion show_card_from_account(
        l10n_util::GetStringUTF16(IDS_AUTOFILL_SHOW_ACCOUNT_CARDS),
        PopupItemId::kShowAccountCards);
    show_card_from_account.icon = Suggestion::Icon::kGoogle;
    footer_suggestions.push_back(show_card_from_account);
  }

  footer_suggestions.push_back(CreateSeparator());

  if (is_autofilled) {
    footer_suggestions.push_back(CreateClearFormSuggestion());
  }

  footer_suggestions.push_back(CreateManagePaymentMethodsEntry(with_gpay_logo));

  return footer_suggestions;
}

bool AutofillSuggestionGenerator::ShouldShowVirtualCardOptionForServerCard(
    const CreditCard* card) const {
  CHECK(card);

  // If the card is not enrolled into virtual cards, we should not show a
  // virtual card suggestion for it.
  if (card->virtual_card_enrollment_state() !=
      CreditCard::VirtualCardEnrollmentState::kEnrolled) {
    return false;
  }

  // We should not show a suggestion for this card if the autofill
  // optimization guide returns that this suggestion should be blocked.
  if (auto* autofill_optimization_guide =
          autofill_client_->GetAutofillOptimizationGuide()) {
    bool blocked = autofill_optimization_guide->ShouldBlockFormFieldSuggestion(
        autofill_client_->GetLastCommittedPrimaryMainFrameOrigin().GetURL(),
        card);
    return !blocked;
  }

  // No conditions to prevent displaying a virtual card suggestion were
  // found, so return true.
  return true;
}

}  // namespace autofill
