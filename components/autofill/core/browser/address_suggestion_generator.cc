// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/address_suggestion_generator.h"

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "base/check_deref.h"
#include "base/containers/contains.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/autofill_browser_util.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_granular_filling_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/data_model/borrowed_transliterator.h"
#include "components/autofill/core/browser/field_filling_address_util.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/address_field_parser.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/geo/address_i18n.h"
#include "components/autofill/core/browser/geo/phone_number_i18n.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_field_data.h"
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

constexpr DenseSet<SuggestionType> kGroupFillingSuggestions = {
    SuggestionType::kFillFullName, SuggestionType::kFillFullAddress,
    SuggestionType::kFillFullEmail, SuggestionType::kFillFullPhoneNumber};

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

// First, check the comment of
// `GetProfileSuggestionMainTextForNonAddressField()`.
//
// To find the position and length of the first address field, the code
// iterates through all of the possible fields which can be part of the result
// of `AutofillProfile::CreateDifferentiatingLabels()`, and checks which one is
// the closest to the beginning of the string.
//
// Note: Right-to-left languages are only displayed from right to left, the
// characters are stored from left to right.
std::pair<size_t, size_t>
GetFirstAddressFieldPositionAndLengthForNonAddressField(
    const AutofillProfile& profile,
    const std::string& app_locale,
    std::u16string_view suggestion_text) {
  std::pair<size_t, size_t> result = {suggestion_text.size(), 0};
  for (const FieldType field_type :
       AutofillProfile::kDefaultDistinguishingFieldsForLabels) {
    // For phone numbers, `suggestion_text` contains the user-formatted (raw)
    // version of a field instead of the canonicalized version.
    const std::u16string field_value =
        field_type == PHONE_HOME_WHOLE_NUMBER
            ? profile.GetRawInfo(field_type)
            : profile.GetInfo(field_type, app_locale);
    size_t field_value_position = suggestion_text.find(field_value);
    if (!field_value.empty() && field_value_position != std::u16string::npos &&
        field_value_position < result.first) {
      result = {field_value_position, field_value.size()};
    }
  }
  return result;
}

// For a profile containing a full address, the main text is the name, and
// the label is the address. The problem arises when a profile isn't complete
// (aka it doesn't have a name or an address etc.).
//
// `AutofillProfile::CreateDifferentiatingLabels()` generates a text which
// contains 2 values from profile.
//
// Example for a full Autofill profile:
// "Full Name, Address"
//
// Examples where Autofill profiles are incomplete:
// "City, Country"
// "Country, Email"
//
// Note: the separator isn't necessarily ", ", it can be an arabic comma, a
// space or no separator at all, depending on the language.
//
// The main text is located at the beginning of the aforementioned strings. In
// order to extract it, we calculate its position into the string and its length
// using `GetFirstAddressFieldPositionAndLengthForNonAddressField()`. We cannot
// split the string by separators because of two reasons: some languages don't
// have any separator, and because address fields can have commas inside them in
// some countries.
//
// The reason why the position is also needed is because in some languages the
// address can start with something different than an address field.
// For example, in Japanese, addresses can start with the "〒" character, which
// is the Japanese postal mark.
std::u16string GetProfileSuggestionMainTextForNonAddressField(
    const AutofillProfile& profile,
    const std::string& app_locale) {
  std::vector<std::u16string> suggestion_text_array;
  AutofillProfile::CreateDifferentiatingLabels({&profile}, app_locale,
                                               &suggestion_text_array);
  CHECK_EQ(suggestion_text_array.size(), 1u);
  auto [position, length] =
      GetFirstAddressFieldPositionAndLengthForNonAddressField(
          profile, app_locale, suggestion_text_array[0]);
  return suggestion_text_array[0].substr(position, length);
}

// Check comment of `GetProfileSuggestionMainTextForNonAddressField()` method.
std::vector<std::u16string> GetProfileSuggestionLabelForNonAddressField(
    const std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>&
        profiles,
    const std::string& app_locale) {
  std::vector<std::u16string> labels;
  AutofillProfile::CreateDifferentiatingLabels(profiles, app_locale, &labels);
  CHECK_EQ(labels.size(), profiles.size());

  for (size_t index = 0; index < profiles.size(); index++) {
    auto [main_text_position, main_text_length] =
        GetFirstAddressFieldPositionAndLengthForNonAddressField(
            *profiles[index], app_locale, labels[index]);
    // Erasing the main text results in the label being the first address field
    // in the string.
    labels[index].erase(main_text_position, main_text_length);
    size_t start_position_of_label =
        GetFirstAddressFieldPositionAndLengthForNonAddressField(
            *profiles[index], app_locale, labels[index])
            .first;
    labels[index] = start_position_of_label < labels[index].size()
                        ? labels[index].substr(start_position_of_label)
                        : u"";
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
  suggestion.type = SuggestionType::kEditAddressProfile;
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
  suggestion.type = SuggestionType::kDeleteAddressProfile;
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
  suggestion.type = SuggestionType::kFillFullAddress;
  suggestion.payload = backend_id;
  suggestion.acceptance_a11y_announcement = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_A11Y_ANNOUNCE_FILL_ADDRESS_GROUP_POPUP_OPTION_SELECTED);
  return suggestion;
}

// Creates the suggestion that will fill all name related fields.
Suggestion GetFillFullNameSuggestion(Suggestion::BackendId backend_id) {
  Suggestion suggestion(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_FILL_NAME_GROUP_POPUP_OPTION_SELECTED));
  suggestion.type = SuggestionType::kFillFullName;
  suggestion.main_text.is_primary = Suggestion::Text::IsPrimary(false);
  suggestion.payload = backend_id;
  suggestion.acceptance_a11y_announcement = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_A11Y_ANNOUNCE_FILL_NAME_GROUP_POPUP_OPTION_SELECTED);

  return suggestion;
}

// Creates the suggestion that will fill the whole form for the profile.
Suggestion GetFillEverythingFromAddressProfileSuggestion(
    Suggestion::BackendId backend_id) {
  Suggestion suggestion(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_FILL_EVERYTHING_FROM_ADDRESS_PROFILE_POPUP_OPTION_SELECTED));
  suggestion.type = SuggestionType::kFillEverythingFromAddressProfile;
  suggestion.icon = Suggestion::Icon::kMagic;
  if (!features::
           kAutofillGranularFillingAvailableWithFillEverythingAtTheBottomParam
               .Get()) {
    // If at the top, this suggestion has to have the same style as `Fill full
    // name` or `Fill full address` suggestions. If at the bottom, this line has
    // to be omitted not to interfere with the styling applied to footer
    // suggestions.
    suggestion.main_text.is_primary = Suggestion::Text::IsPrimary(false);
  }
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
                               SuggestionType::kAddressFieldByFieldFilling);
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
// and is used to define  whether the `SuggestionType::kFillFullName`
// suggestion will be available.
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
    suggestion.children.push_back(CreateSeparator());
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
// by the user and is used to define whether the
// `SuggestionType::kFillFullAddress` suggestion will be available. Note that
// `FieldTypeGroup::kCompany` is also included into the address group.
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
    suggestion.children.push_back(CreateSeparator());
  }
}

// Adds contact related child suggestions (i.e email and phone number) to
// build autofill popup submenu. The param `trigger_field_type` refers to the
// field clicked by the user and affects whether international or local phone
// number will be shown to the user in the suggestion. The field type group of
// the `trigger_field_type` is used to define whether the phone number and email
// suggestions will behave as `SuggestionType::kAddressFieldByFieldFilling` or
// as
// `SuggestionType::kFillFullPhoneNumber`/`SuggestionType::kFillFullEmail`
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
          SuggestionType::kFillFullPhoneNumber);
      // `SuggestionType::kAddressFieldByFieldFilling` suggestions do not use
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
          SuggestionType::kFillFullEmail);
      // `SuggestionType::kAddressFieldByFieldFilling` suggestions do not use
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
    suggestion.children.push_back(CreateSeparator());
  }
}

// Adds footer child suggestions for editing and deleting a profile from the
// popup submenu. Note that these footer suggestions are not added in incognito
// mode (`is_off_the_record`).
void AddFooterChildSuggestions(const AutofillProfile& profile,
                               FieldType trigger_field_type,
                               bool is_off_the_record,
                               Suggestion& suggestion) {
  // If the trigger field is not classified as an address field, then the
  // filling was triggered from the context menu. In this scenario, the user
  // should not be able to fill everything.
  // If the last filling granularity was not full form, add the
  // `SuggestionType::kFillEverythingFromAddressProfile` suggestion. This
  // allows the user to go back to filling the whole form once in a more fine
  // grained filling experience.
  if (IsAddressType(trigger_field_type) &&
      suggestion.type != SuggestionType::kAddressEntry &&
      features::
          kAutofillGranularFillingAvailableWithFillEverythingAtTheBottomParam
              .Get()) {
    suggestion.children.push_back(GetFillEverythingFromAddressProfileSuggestion(
        Suggestion::Guid(profile.guid())));
  }
  if (!is_off_the_record) {
    suggestion.children.push_back(
        GetEditAddressProfileSuggestion(Suggestion::Guid(profile.guid())));
    suggestion.children.push_back(
        GetDeleteAddressProfileSuggestion(Suggestion::Guid(profile.guid())));
  }
}

// Creates a specific granular filling label which will be used for each
// `AutofillProfile` in `profiles` for group filling suggestions. This is done
// to give users feedback about the filling behaviour. Returns an empty string
// when no granular filling label needs to be applied for a profile.
std::u16string GetGranularFillingLabels(SuggestionType suggestion_type) {
  if (!kGroupFillingSuggestions.contains(suggestion_type)) {
    return u"";
  }
  switch (suggestion_type) {
    case SuggestionType::kFillFullAddress:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_FILL_ADDRESS_GROUP_POPUP_OPTION_SELECTED);
    case SuggestionType::kFillFullName:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_FILL_NAME_GROUP_POPUP_OPTION_SELECTED);
    case SuggestionType::kFillFullPhoneNumber:
    case SuggestionType::kFillFullEmail:
    default:
      NOTREACHED();
  }
}

// Returns whether the `ADDRESS_HOME_LINE1` should be included into the labels
// of the suggestion. Returns true if `trigger_field_type` is an address field
// (actual address field: ADDRESS_HOME_ZIP, ADDRESS_HOME_CITY, etc.; not
// NAME_FULL or PHONE_HOME_NUMBER) that usually does not allow users to easily
// identify their address.
bool ShouldAddAddressLine1ToSuggestionLabels(FieldType trigger_field_type) {
  static constexpr DenseSet kAddressRecognizingFields = {
      ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE2, ADDRESS_HOME_STREET_ADDRESS};
  return GroupTypeOfFieldType(trigger_field_type) == FieldTypeGroup::kAddress &&
         !kAddressRecognizingFields.contains(trigger_field_type);
}

// Returns the minimum number of fields that should be returned by
// `AutofillProfile::CreateInferredLabels()`, based on the type of the
// triggering field and on the filling granularity.
int GetNumberOfMinimalFieldsToShow(FieldType trigger_field_type,
                                   SuggestionType suggestion_type) {
  if (kGroupFillingSuggestions.contains(suggestion_type)) {
    // If an address field cannot provide sufficient information about the
    // address, then `ADDRESS_HOME_LINE1` should be part of the label.
    // Otherwise, no general labels are needed in group filling mode. Only
    // differentiating labels should exist.
    return ShouldAddAddressLine1ToSuggestionLabels(trigger_field_type) ? 1 : 0;
  } else if (GroupTypeOfFieldType(trigger_field_type) ==
             FieldTypeGroup::kPhone) {
    // Phone fields are a special case. For them we want both the
    // `FULL_NAME` and `ADDRESS_HOME_LINE1` to be present.
    return 2;
  } else {
    return 1;
  }
}

// Returns for each profile in `profiles` a differentiating label string to be
// used as a secondary text in the corresponding suggestion bubble.
// `field_types` the types of the fields that will be filled by the suggestion.
std::vector<std::u16string> GetProfileSuggestionLabels(
    const std::vector<AutofillProfile>& profiles,
    const FieldTypeSet& field_types,
    FieldType trigger_field_type,
    SuggestionType suggestion_type,
    const std::string& app_locale) {
  // Generate disambiguating labels based on the list of matches.
  std::vector<std::u16string> differentiating_labels;
  const bool is_in_full_form_filling_mode =
      suggestion_type == SuggestionType::kAddressEntry;
  auto profile_ptrs =
      base::ToVector(profiles,
                     [](const AutofillProfile& profile)
                         -> raw_ptr<const AutofillProfile, VectorExperimental> {
                       return &profile;
                     });
  if (!IsAddressType(trigger_field_type) &&
      base::FeatureList::IsEnabled(
          features::kAutofillForUnclassifiedFieldsAvailable)) {
    differentiating_labels =
        GetProfileSuggestionLabelForNonAddressField(profile_ptrs, app_locale);
  } else if ((!is_in_full_form_filling_mode ||
              features::kAutofillGranularFillingAvailableWithImprovedLabelsParam
                  .Get()) &&
             base::FeatureList::IsEnabled(
                 features::kAutofillGranularFillingAvailable)) {
    // When in full form filling mode, this flow should only be used if
    // `features::kAutofillGranularFillingAvailableWithImprovedLabelsParam` of
    // the feature is enabled.
    // All other granular filling modes should always use this flow.
    AutofillProfile::CreateInferredLabels(
        profile_ptrs, /*suggested_fields=*/std::nullopt, trigger_field_type,
        {trigger_field_type},
        GetNumberOfMinimalFieldsToShow(trigger_field_type, suggestion_type),
        app_locale, &differentiating_labels,
        /*use_improved_labels_order=*/true);
  } else {
    AutofillProfile::CreateInferredLabels(
        profile_ptrs, field_types, /*triggering_field_type=*/std::nullopt,
        {trigger_field_type},
        /*minimal_fields_shown=*/1, app_locale, &differentiating_labels);
  }
  return differentiating_labels;
}

// For each profile in `profiles`, returns a vector of `Suggestion::labels` to
// be applied. Takes into account the `suggestion_type` and the
// `trigger_field_type` to add specific granular filling labels. Optionally adds
// a differentiating label if the Suggestion::main_text + granular filling label
// is not unique.
std::vector<std::vector<Suggestion::Text>>
CreateSuggestionLabelsWithGranularFillingDetails(
    const std::vector<AutofillProfile>& profiles,
    const FieldTypeSet& field_types,
    SuggestionType suggestion_type,
    FieldType trigger_field_type,
    const std::string& app_locale) {
  // Suggestions for filling only one field (field-by-field filling, email group
  // filling, etc.) should not have labels because they are guaranteed to be
  // unique, see `DeduplicatedProfilesForSuggestions()`.
  // As an exception, when a user triggers autofill from the context menu on a
  // field which is not classified as an address, labels should be added because
  // the first-level suggestion is not clickable. The first-level suggestion
  // needs to give plenty of info about the profile.
  if (field_types.size() == 1 && IsAddressType(trigger_field_type)) {
    return std::vector<std::vector<Suggestion::Text>>(profiles.size());
  }

  const std::u16string suggestions_granular_filling_label =
      GetGranularFillingLabels(suggestion_type);

  const std::vector<std::u16string> suggestions_differentiating_labels =
      GetProfileSuggestionLabels(profiles, field_types, trigger_field_type,
                                 suggestion_type, app_locale);

  // For each suggestion/profile, generate its label based on granular filling
  // and differentiating labels.
  std::vector<std::vector<Suggestion::Text>> suggestions_labels;
  suggestions_labels.reserve(profiles.size());
  for (size_t i = 0; i < profiles.size(); ++i) {
    std::vector<std::u16string> labels;

    if (!suggestions_granular_filling_label.empty()) {
      labels.push_back(suggestions_granular_filling_label);
    }
    if (!suggestions_differentiating_labels[i].empty()) {
      labels.push_back(suggestions_differentiating_labels[i]);
    }

    // The granular filling label and the differentiating label are separated by
    // "-".
    //
    // Below are examples of how the final label of the suggestion would look
    // like.
    //
    // 1. With a granular filling label and no differentiating label.
    //  _________________________
    // | Jon snow                |
    // | Fill address            |
    // |_________________________|
    //
    //
    // 2. With both granular filling label and differentiating label (note: a
    // differentiating label is ONE string consisting of one or multiple fields
    // separated by ",").
    //  __________________________
    // | 8129                     |
    // | Fill address - Winterfel |
    // |__________________________|
    //  ________________________________________
    // | 8129                                   |
    // | Fill address - Winterfel, jon@snow.com |
    // |________________________________________|
    //
    // 3. With no granular filling label and a differentiating label.
    //  ___________
    // | 8129      |
    // | Winterfel |
    // |___________|
    //
    //  _________________________
    // | 8129                    |
    // | Winterfel, jon@snow.com |
    // |_________________________|
    suggestions_labels.push_back(
        {Suggestion::Text(base::JoinString(labels, u" - "))});
  }
  return suggestions_labels;
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
  Suggestion suggestion(l10n_util::GetStringUTF16(IDS_AUTOFILL_DEVELOPER_TOOLS),
                        SuggestionType::kDevtoolsTestAddresses);
  suggestion.main_text.is_primary = Suggestion::Text::IsPrimary(false);
  suggestion.icon = Suggestion::Icon::kCode;
  suggestion.is_acceptable = false;
  suggestion.children.emplace_back(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_TEST_ADDRESS_BY_COUNTRY),
      SuggestionType::kDevtoolsTestAddressByCountry);
  suggestion.children.back().is_acceptable = false;
  suggestion.children.back().apply_deactivated_style = true;
  suggestion.children.emplace_back(SuggestionType::kSeparator);
  for (const AutofillProfile& test_address : test_addresses) {
    const std::u16string test_address_country =
        test_address.GetInfo(ADDRESS_HOME_COUNTRY, locale);
    suggestion.children.emplace_back(test_address_country,
                                     SuggestionType::kDevtoolsTestAddressEntry);
    suggestion.children.back().payload = Suggestion::Guid(test_address.guid());
    suggestion.children.back().acceptance_a11y_announcement =
        l10n_util::GetStringFUTF16(IDS_AUTOFILL_TEST_ADDRESS_SELECTED_A11Y_HINT,
                                   test_address_country);
  }
  return suggestion;
}

// Dedupes the given profiles based on if one is a subset of the other for
// suggestions represented by `field_types`. The function returns at most
// `kMaxDeduplicatedProfilesForSuggestion` profiles. `field_types` stores all
// of the FieldTypes relevant for the current suggestions, including that of
// the field on which the user is currently focused.
std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>
DeduplicatedProfilesForSuggestions(
    const std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>&
        matched_profiles,
    FieldType trigger_field_type,
    const FieldTypeSet& field_types,
    const AutofillProfileComparator& comparator) {
  std::vector<std::u16string> suggestion_main_text;
  for (const AutofillProfile* profile : matched_profiles) {
    suggestion_main_text.push_back(GetProfileSuggestionMainText(
        *profile, comparator.app_locale(), trigger_field_type));
  }
  std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>
      unique_matched_profiles;
  // Limit number of unique profiles as having too many makes the
  // browser hang due to drawing calculations (and is also not
  // very useful for the user).
  for (size_t a = 0;
       a < matched_profiles.size() &&
       unique_matched_profiles.size() < kMaxDeduplicatedProfilesForSuggestion;
       ++a) {
    bool include = true;
    const AutofillProfile* profile_a = matched_profiles[a];
    for (size_t b = 0; b < matched_profiles.size(); ++b) {
      const AutofillProfile* profile_b = matched_profiles[b];
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
      // Prefer account profiles over local ones. In case the profiles are of
      // the same type, prefer the earlier one (since the profiles are
      // pre-sorted by their relevance).
      const bool prefer_a_over_b =
          profile_a->IsAccountProfile() == profile_b->IsAccountProfile()
              ? a < b
              : profile_a->IsAccountProfile();
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

// Matches based on prefix search, and limits number of profiles.
// Returns the top matching profiles based on prefix search. At most
// `kMaxPrefixMatchedProfilesForSuggestion` are returned.
std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>
GetPrefixMatchedProfiles(const std::vector<const AutofillProfile*>& profiles,
                         FieldType trigger_field_type,
                         const std::u16string& raw_field_contents,
                         const std::u16string& field_contents_canon,
                         bool field_is_autofilled,
                         const std::string& app_locale) {
  std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>
      matched_profiles;
  for (const AutofillProfile* profile : profiles) {
    if (matched_profiles.size() == kMaxPrefixMatchedProfilesForSuggestion) {
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
    std::u16string main_text =
        GetProfileSuggestionMainText(*profile, app_locale, trigger_field_type);
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

// Removes profiles that haven't been used after `kDisusedDataModelTimeDelta`
// from `profiles`. Note that the goal of this filtering strategy is only to
// reduce visual noise for users that have many profiles, and therefore in
// some cases, some disused profiles might be kept in the list, to avoid
// filtering out all profiles, leading to no suggestions being shown. The
// relative ordering of `profiles` is maintained.
void RemoveDisusedSuggestions(
    std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>& profiles) {
  const base::Time min_last_used =
      AutofillClock::Now() - kDisusedDataModelTimeDelta;
  auto is_profile_disused =
      [&min_last_used](
          const raw_ptr<const AutofillProfile, VectorExperimental>& profile) {
        return profile->use_date() <= min_last_used;
      };
  if (base::ranges::count_if(profiles, is_profile_disused) == 0) {
    AutofillMetrics::LogNumberOfAddressesSuppressedForDisuse(0);
    return;
  }
  const size_t original_size = profiles.size();
  if (base::FeatureList::IsEnabled(
          features::kAutofillChangeDisusedAddressSuggestionTreatment)) {
    profiles.erase(
        std::remove_if(profiles.begin() +
                           std::min(features::kNumberOfIgnoredSuggestions.Get(),
                                    static_cast<int>(profiles.size())),
                       profiles.end(), is_profile_disused),
        profiles.end());
  } else {
    std::erase_if(profiles, is_profile_disused);
  }
  AutofillMetrics::LogNumberOfAddressesSuppressedForDisuse(original_size -
                                                           profiles.size());
}

// Creates nested/child suggestions for `suggestion` with the `profile`
// information. Uses `trigger_field_type` to define what group filling
// suggestion to add (name, address or phone). The existence of child
// suggestions defines whether the autofill popup will have submenus.
void AddAddressGranularFillingChildSuggestions(FieldType trigger_field_type,
                                               const AutofillProfile& profile,
                                               Suggestion& suggestion,
                                               bool is_off_the_record,
                                               const std::string& app_locale) {
  const FieldTypeGroup trigger_field_type_group =
      GroupTypeOfFieldType(trigger_field_type);
  // The "Fill everything" suggestion is added at the top (even if the filling
  // mode is full form filling), in its own section of the sub-menu, if
  // `features::kAutofillGranularFillingAvailableWithFillEverythingAtTheBottomParam`
  // is disabled. Otherwise, it is added in the footer.
  //
  // If the trigger field is not classified as an address field, then the
  // filling was triggered from the context menu. In this scenario, the user
  // should not be able to fill everything.
  // TODO(crbug.com/40274514): Maybe separate this in a different function when
  // the feature is launched.
  if (IsAddressType(trigger_field_type) &&
      !features::
           kAutofillGranularFillingAvailableWithFillEverythingAtTheBottomParam
               .Get()) {
    suggestion.children.push_back(GetFillEverythingFromAddressProfileSuggestion(
        Suggestion::Guid(profile.guid())));
    suggestion.children.push_back(CreateSeparator());
  }
  AddNameChildSuggestions(trigger_field_type_group, profile, app_locale,
                          suggestion);
  AddAddressChildSuggestions(trigger_field_type_group, profile, app_locale,
                             suggestion);
  AddContactChildSuggestions(trigger_field_type, profile, app_locale,
                             suggestion);
  AddFooterChildSuggestions(profile, trigger_field_type, is_off_the_record,
                            suggestion);
  // In incognito mode we do not have edit and deleting options. In this
  // situation there is a chance that no footer suggestions exist, which could
  // lead to a leading `kSeparator` suggestion.
  if (suggestion.children.back().type == SuggestionType::kSeparator) {
    suggestion.children.pop_back();
  }
}

// Returns non address suggestions which are displayed below address
// suggestions in the Autofill popup. `is_autofilled` is used to conditionally
// add suggestion for clearing all autofilled fields.
std::vector<Suggestion> GetAddressFooterSuggestions(bool is_autofilled) {
  std::vector<Suggestion> footer_suggestions;
  footer_suggestions.push_back(CreateSeparator());
  if (is_autofilled) {
    footer_suggestions.push_back(CreateUndoOrClearFormSuggestion());
  }
  footer_suggestions.push_back(CreateManageAddressesSuggestion());
  return footer_suggestions;
}

ProfilesToSuggestOptions GetProfilesToSuggestOptions(
    FieldType trigger_field_type,
    const std::u16string& trigger_field_contents,
    bool trigger_field_is_autofilled,
    AutofillSuggestionTriggerSource trigger_source) {
  // By default, disused profiles are excluded only if the normalized field
  // value is empty. However, triggering suggestions via manual fallback should
  // allow the user to access all their profiles, which is why this option is
  // disabled there.
  bool should_excluded_disused_addresses =
      trigger_source !=
          AutofillSuggestionTriggerSource::kManualFallbackAddress &&
      NormalizeForComparisonForType(trigger_field_contents, trigger_field_type)
          .empty();
  // By default, suggestions should have non-empty value on the trigger field.
  // However, triggering suggestions via manual fallback on non-address fields
  // has a separate main-text retrieving logic, ensuring that this text is never
  // empty, which is why we disable this options there.
  bool should_require_non_empty_value_on_trigger_field =
      IsAddressType(trigger_field_type);
  // By default, suggestions should be matched with the field content. However,
  // triggering suggestions via manual fallback should allow the user to access
  // all their profiles, which is why this option is disabled there. Moreover,
  // prefix matching in that case could result in poor UX, since the user
  // explicitly asked for Autofill address suggestions.
  // Additionally, if AutofillAddressFieldSwapping is enabled, prefix matching
  // is disabled because we want to offer the user suggestions to swap the
  // current value of the field with something else, making the prefix matching
  // not useful.
  bool should_prefix_match_suggestions =
      trigger_source !=
          AutofillSuggestionTriggerSource::kManualFallbackAddress &&
      (!trigger_field_is_autofilled || !IsAddressFieldSwappingEnabled());
  // By default, suggestions should be deduplicated in order to not offer
  // redundant suggestions. However, triggering suggestions via manual fallback
  // should allow the user to access all their profiles, which is why this
  // option is disabled there.
  bool should_deduplicate_suggestions =
      trigger_source != AutofillSuggestionTriggerSource::kManualFallbackAddress;
  return ProfilesToSuggestOptions{
      .exclude_disused_addresses = should_excluded_disused_addresses,
      .require_non_empty_value_on_trigger_field =
          should_require_non_empty_value_on_trigger_field,
      .prefix_match_suggestions = should_prefix_match_suggestions,
      .deduplicate_suggestions = should_deduplicate_suggestions};
}

// Returns a list of profiles that will be displayed as suggestions to the user,
// sorted by their relevance. This involves many steps from fetching the
// profiles to matching with `field_contents`, and deduplicating based on
// `field_types`, which are the relevant types for the current suggestion.
// `options` defines what strategies to follow by the function in order to
// filter the list or returned profiles.
std::vector<AutofillProfile> GetProfilesToSuggest(
    const AddressDataManager& address_data,
    FieldType trigger_field_type,
    const std::u16string& field_contents,
    bool field_is_autofilled,
    const FieldTypeSet& field_types,
    ProfilesToSuggestOptions options) {
  // Get the profiles to suggest, which are already sorted.
  std::vector<const AutofillProfile*> sorted_profiles =
      address_data.GetProfilesToSuggest();
  if (options.require_non_empty_value_on_trigger_field) {
    std::erase_if(sorted_profiles, [&](const AutofillProfile* profile) {
      return GetProfileSuggestionMainText(*profile, address_data.app_locale(),
                                          trigger_field_type)
          .empty();
    });
  }
  std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>
      profiles_to_suggest(sorted_profiles.begin(), sorted_profiles.end());
  if (options.prefix_match_suggestions) {
    profiles_to_suggest = GetPrefixMatchedProfiles(
        sorted_profiles, trigger_field_type, field_contents,
        NormalizeForComparisonForType(field_contents, trigger_field_type),
        field_is_autofilled, address_data.app_locale());
  }
  if (options.exclude_disused_addresses) {
    RemoveDisusedSuggestions(profiles_to_suggest);
  }
  const AutofillProfileComparator comparator(address_data.app_locale());
  // It is important that deduplication is the last filtering strategy to be
  // executed, otherwise some profiles could be deduplicated in favor of another
  // profile that is later removed by another filtering strategy.
  if (options.deduplicate_suggestions) {
    profiles_to_suggest = DeduplicatedProfilesForSuggestions(
        profiles_to_suggest, trigger_field_type, field_types, comparator);
  }
  // Do not show more than `kMaxDisplayedAddressSuggestions` suggestions since
  // it would result in poor UX.
  if (profiles_to_suggest.size() > kMaxDisplayedAddressSuggestions) {
    profiles_to_suggest.resize(kMaxDisplayedAddressSuggestions);
  }
  return base::ToVector(
      profiles_to_suggest,
      [](const AutofillProfile* profile) { return *profile; });
}

// Returns a list of Suggestion objects, each representing an element in
// `profiles`.
// `field_types` holds the type of fields relevant for the current suggestion.
// The profiles passed to this function should already have been matched on
// `trigger_field_contents_canon` and deduplicated.
std::vector<Suggestion> CreateSuggestionsFromProfiles(
    std::vector<AutofillProfile> profiles,
    const std::string& gaia_email,
    const FieldTypeSet& field_types,
    SuggestionType suggestion_type,
    FieldType trigger_field_type,
    uint64_t trigger_field_max_length,
    std::optional<std::string> plus_address_email_override,
    bool is_off_the_record,
    const std::string& app_locale) {
  // TODO(crbug.com/40274514): Remove when launching
  // AutofillGranularFillingAvailable.
  if (profiles.empty()) {
    return {};
  }

  for (AutofillProfile& profile : profiles) {
    // If the following conditions are met:
    // - A plus address override is available
    // - The profile's email address is the same as the user's Google Account
    // email.
    // Then the profile's email address will be replaced with the plus
    // address in order to show the updated email on the suggestion label.
    if (plus_address_email_override && profile.HasInfo(EMAIL_ADDRESS) &&
        base::UTF16ToUTF8(profile.GetRawInfo(EMAIL_ADDRESS)) == gaia_email) {
      profile.SetRawInfo(EMAIL_ADDRESS,
                         base::UTF8ToUTF16(*plus_address_email_override));
    }
  }

  std::vector<Suggestion> suggestions;
  std::vector<std::vector<Suggestion::Text>> labels =
      CreateSuggestionLabelsWithGranularFillingDetails(
          profiles, field_types, suggestion_type, trigger_field_type,
          app_locale);
  // This will be used to check if suggestions should be supported with icons.
  // TODO(crbug.com/40285811): Consider simplifying this to be any address
  // field. This will allow to add icons for every full form filling and manual
  // fallback case.
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
  // If `features::kAutofillGranularFillingAvailableWithImprovedLabelsParam` of
  // the `kAutofillGranularFillingAvailable` feature is enabled, name fields
  // should have `NAME_FULL` as main text, unless in field by field filling
  // mode.
  FieldType main_text_field_type =
      GroupTypeOfFieldType(trigger_field_type) == FieldTypeGroup::kName &&
              suggestion_type != SuggestionType::kAddressFieldByFieldFilling &&
              base::FeatureList::IsEnabled(
                  features::kAutofillGranularFillingAvailable) &&
              features::kAutofillGranularFillingAvailableWithImprovedLabelsParam
                  .Get()
          ? NAME_FULL
          : trigger_field_type;
  const bool is_filling_address_form = IsAddressType(trigger_field_type);
  for (size_t i = 0; i < profiles.size(); ++i) {
    const AutofillProfile& profile = profiles[i];
    // Compute the main text to be displayed in the suggestion bubble.
    std::u16string main_text =
        GetProfileSuggestionMainText(profile, app_locale, main_text_field_type);
    if (trigger_field_type_group == FieldTypeGroup::kPhone) {
      main_text = GetFormattedPhoneNumber(
          profile, app_locale,
          ShouldUseNationalFormatPhoneNumber(trigger_field_type));
    }
    Suggestion& suggestion = suggestions.emplace_back(main_text);
    if (!labels[i].empty()) {
      suggestion.labels.emplace_back(std::move(labels[i]));
    }
    // TODO(crbug.com/324557053): Update the payload beyond the guid to support
    // preview and filling.
    suggestion.payload = Suggestion::Guid(profile.guid());
    suggestion.acceptance_a11y_announcement =
        l10n_util::GetStringUTF16(IDS_AUTOFILL_A11Y_ANNOUNCE_FILLED_FORM);
    suggestion.type = suggestion_type;
    suggestion.is_acceptable = is_filling_address_form;
    if (suggestion.type == SuggestionType::kAddressFieldByFieldFilling) {
      suggestion.field_by_field_filling_type_used =
          std::optional(trigger_field_type);
    }
    // We add an icon to the address (profile) suggestion if there is more than
    // one profile related field in the form or the user triggered address
    // filling on on a non address field (manual fallbacks). For email fields,
    // the email icon is used unconditionally to create consistency with plus
    // address suggestions.
    if (GroupTypeOfFieldType(trigger_field_type) == FieldTypeGroup::kEmail) {
      suggestion.icon = Suggestion::Icon::kEmail;
    } else if (contains_profile_related_fields || !is_filling_address_form) {
      const bool fill_full_form =
          suggestion.type == SuggestionType::kAddressEntry;
      if (!is_filling_address_form ||
          base::FeatureList::IsEnabled(
              features::kAutofillGranularFillingAvailable)) {
        suggestion.icon = fill_full_form ? Suggestion::Icon::kLocation
                                         : Suggestion::Icon::kNoIcon;
      } else {
        suggestion.icon = Suggestion::Icon::kAccount;
      }
    }
    // This is intentionally not using `profile.IsAccountProfile()` because the
    // IPH should only be shown for non-H/W profiles.
    if (profile.record_type() == AutofillProfile::RecordType::kAccount &&
        profile.initial_creator_id() !=
            AutofillProfile::kInitialCreatorOrModifierChrome) {
      suggestion.feature_for_iph =
          &feature_engagement::
              kIPHAutofillExternalAccountProfileSuggestionFeature;
    }
    const bool should_offer_manual_fallback_on_unclassified_fields =
        !IsAddressType(trigger_field_type) &&
        base::FeatureList::IsEnabled(
            features::kAutofillForUnclassifiedFieldsAvailable);
    if (should_offer_manual_fallback_on_unclassified_fields ||
        base::FeatureList::IsEnabled(
            features::kAutofillGranularFillingAvailable)) {
      // TODO(crbug.com/40942505): Make the granular filling options vary
      // depending on the locale.
      AddAddressGranularFillingChildSuggestions(trigger_field_type, profile,
                                                suggestion, is_off_the_record,
                                                app_locale);
    }
  }
  return suggestions;
}

}  // namespace

std::vector<Suggestion> GetSuggestionsForProfiles(
    const AutofillClient& client,
    const FieldTypeSet& field_types,
    const FormFieldData& trigger_field,
    FieldType trigger_field_type,
    SuggestionType suggestion_type,
    AutofillSuggestionTriggerSource trigger_source,
    std::optional<std::string> plus_address_email_override) {
  std::vector<AutofillProfile> profiles_to_suggest = GetProfilesToSuggest(
      client.GetPersonalDataManager()->address_data_manager(),
      trigger_field_type, trigger_field.value(), trigger_field.is_autofilled(),
      field_types,
      GetProfilesToSuggestOptions(trigger_field_type, trigger_field.value(),
                                  trigger_field.is_autofilled(),
                                  trigger_source));
  // If autofill for addresses is triggered from the context menu on an address
  // field and no suggestions can be shown (i.e. if a user has only addresses
  // without emails and then triggers autofill from the context menu on an email
  // field), then default to the same behaviour as if the user triggers autofill
  // on a non-address field. This is done to avoid a situation when the user
  // would trigger autofill from the context menu, and then no suggestions
  // appear.
  // The "if condition" is satisfied only if `trigger_field_type` is an address
  // field. Then, `GetSuggestionsForProfiles()` is called with `UNKOWN_TYPE` for
  // the `trigger_field_type`. This guarantees no infinite recursion occurs.
  if (profiles_to_suggest.empty() && IsAddressType(trigger_field_type) &&
      trigger_source ==
          AutofillSuggestionTriggerSource::kManualFallbackAddress &&
      base::FeatureList::IsEnabled(
          features::kAutofillForUnclassifiedFieldsAvailable)) {
    return GetSuggestionsForProfiles(
        client, {UNKNOWN_TYPE}, trigger_field, UNKNOWN_TYPE, suggestion_type,
        trigger_source, std::move(plus_address_email_override));
  }

  const std::string gaia_email =
      client.GetIdentityManager()
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .email;

  std::vector<Suggestion> suggestions = CreateSuggestionsFromProfiles(
      std::move(profiles_to_suggest), gaia_email, field_types, suggestion_type,
      trigger_field_type, trigger_field.max_length(),
      std::move(plus_address_email_override), client.IsOffTheRecord(),
      client.GetPersonalDataManager()->address_data_manager().app_locale());

  // Add devtools test addresses suggestion if it exists. A suggestion will
  // exist if devtools is open and therefore test addresses were set.
  if (IsAddressType(trigger_field_type)) {
    if (std::optional<Suggestion> test_addresses_suggestion =
            GetSuggestionForTestAddresses(client.GetTestAddresses(),
                                          client.GetPersonalDataManager()
                                              ->address_data_manager()
                                              .app_locale())) {
      suggestions.push_back(std::move(*test_addresses_suggestion));
    }
  }
  if (suggestions.empty()) {
    return suggestions;
  }
  base::ranges::move(GetAddressFooterSuggestions(trigger_field.is_autofilled()),
                     std::back_inserter(suggestions));
  return suggestions;
}

Suggestion CreateManageAddressesSuggestion() {
  Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_ADDRESSES),
      SuggestionType::kManageAddress);
  suggestion.icon = Suggestion::Icon::kSettings;
  return suggestion;
}

std::vector<AutofillProfile> GetProfilesToSuggestForTest(
    const AddressDataManager& address_data,
    FieldType trigger_field_type,
    const std::u16string& field_contents,
    bool field_is_autofilled,
    const FieldTypeSet& field_types,
    AutofillSuggestionTriggerSource trigger_source) {
  return GetProfilesToSuggest(
      address_data, trigger_field_type, field_contents, field_is_autofilled,
      field_types,
      GetProfilesToSuggestOptions(trigger_field_type, field_contents,
                                  field_is_autofilled, trigger_source));
}

std::vector<Suggestion> CreateSuggestionsFromProfilesForTest(
    std::vector<AutofillProfile> profiles,
    const FieldTypeSet& field_types,
    SuggestionType suggestion_type,
    FieldType trigger_field_type,
    uint64_t trigger_field_max_length,
    bool is_off_the_record,
    const std::string& app_locale,
    std::optional<std::string> plus_address_email_override,
    const std::string& gaia_email) {
  return CreateSuggestionsFromProfiles(
      std::move(profiles), gaia_email, field_types, suggestion_type,
      trigger_field_type, trigger_field_max_length, plus_address_email_override,
      is_off_the_record, app_locale);
}

}  // namespace autofill
