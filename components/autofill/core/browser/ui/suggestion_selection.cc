// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/autofill/core/browser/ui/suggestion_selection.h"

#include <string>
#include <unordered_set>
#include <vector>

#include "base/containers/cxx20_erase_vector.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_granular_filling_utils.h"
#include "components/autofill/core/browser/autofill_suggestion_generator.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/data_model/borrowed_transliterator.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/address_i18n.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_formatter.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill::suggestion_selection {

namespace {

Suggestion GetEditAddressProfileSuggestion(Suggestion::BackendId backend_id) {
  Suggestion suggestion(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_EDIT_ADDRESS_PROFILE_POPUP_OPTION_SELECTED));
  suggestion.popup_item_id = PopupItemId::kEditAddressProfile;
  suggestion.icon = "editIcon";
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
  suggestion.icon = "deleteIcon";
  suggestion.payload = backend_id;
  suggestion.acceptance_a11y_announcement = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_A11Y_ANNOUNCE_DELETE_ADDRESS_PROFILE_POPUP_OPTION_SELECTED);
  return suggestion;
}

// Creates the suggestion that will fill all address related fields.
Suggestion GetFillFullAddressSuggestion(Suggestion::BackendId backend_id) {
  Suggestion suggestion(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_FILL_ADDRESS_GROUP_POPUP_OPTION_SELECTED));
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
  suggestion.icon = "magicIcon";
  suggestion.payload = backend_id;
  suggestion.acceptance_a11y_announcement = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_A11Y_ANNOUNCE_FILL_EVERYTHING_FROM_ADDRESS_PROFILE_POPUP_OPTION_SELECTED);
  return suggestion;
}

// Append new suggestions to `suggestions` based on the `ServerFieldType` list
// provided. Suggestions are not added if their info is not found in the
// provided `profile`. Returns true if any suggestion was added.
bool AddFieldByFieldSuggestions(const std::vector<ServerFieldType>& types,
                                const AutofillProfile& profile,
                                const std::string& app_locale,
                                std::vector<Suggestion>& suggestions) {
  bool any_suggestion_added = false;
  for (auto type : types) {
    std::u16string value = profile.GetInfo(type, app_locale);
    if (!value.empty()) {
      suggestions.emplace_back(value, PopupItemId::kFieldByFieldFilling);
      any_suggestion_added = true;
    }
  }
  return any_suggestion_added;
}

// Given an address `type` and `sub_type`, returns whether the `sub_type` info
// stored in `profile` is a substring of the info stored in `profile` for
// `type`.
bool CheckIfTypeContainsSubtype(ServerFieldType type,
                                ServerFieldType sub_type,
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
void AddNameChildSuggestions(const AutofillType& type,
                             const AutofillProfile& profile,
                             const std::string& app_locale,
                             Suggestion& suggestion) {
  const FieldTypeGroup field_type_group = type.group();
  if (field_type_group == FieldTypeGroup::kName) {
    // Note that this suggestion can only be added if name infos exist in the
    // profile.
    suggestion.children.push_back(
        GetFillFullNameSuggestion(Suggestion::BackendId(profile.guid())));
  }
  if (AddFieldByFieldSuggestions({NAME_FIRST, NAME_MIDDLE, NAME_LAST}, profile,
                                 app_locale, suggestion.children)) {
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
  auto add_address_line = [&](ServerFieldType type) -> bool {
    CHECK(type == ADDRESS_HOME_LINE1 || type == ADDRESS_HOME_LINE2);

    if (!AddFieldByFieldSuggestions({type}, profile, app_locale, suggestions)) {
      return false;
    }

    if (CheckIfTypeContainsSubtype(type, ADDRESS_HOME_HOUSE_NUMBER, profile,
                                   app_locale) &&
        AddFieldByFieldSuggestions({ADDRESS_HOME_HOUSE_NUMBER}, profile,
                                   app_locale, suggestions.back().children)) {
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
        AddFieldByFieldSuggestions({ADDRESS_HOME_STREET_NAME}, profile,
                                   app_locale, suggestions.back().children)) {
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
// The param `type` refers to the triggering field type (clicked by the users)
// and is used to define  whether the `PopupItemId::kFillFullAddress` suggestion
// will be available.
void AddAddressChildSuggestions(const AutofillType& type,
                                const AutofillProfile& profile,
                                const std::string& app_locale,
                                Suggestion& suggestion) {
  const FieldTypeGroup field_type_group = type.group();
  if (field_type_group == FieldTypeGroup::kAddress) {
    // Note that this suggestion can only be added if address infos exist in the
    // profile.
    suggestion.children.push_back(
        GetFillFullAddressSuggestion(Suggestion::BackendId(profile.guid())));
  }

  bool added_any_address_line =
      AddAddressLineChildSuggestions(profile, app_locale, suggestion.children);
  bool added_zip = AddFieldByFieldSuggestions({ADDRESS_HOME_ZIP}, profile,
                                              app_locale, suggestion.children);
  if (added_any_address_line || added_zip) {
    suggestion.children.push_back(
        AutofillSuggestionGenerator::CreateSeparator());
  }
}

// Adds contact related child suggestions (i.e email and phone number) to
// build autofill popup submenu. The param `type` refers to the triggering field
// type (clicked by the users) and is used to define  whether the phone number
// suggestion will behave as `PopupItemId::kFieldByFieldFilling` or as
// `PopupItemId::kFillFullPhoneNumber`.
void AddContactChildSuggestions(const AutofillType& type,
                                const AutofillProfile& profile,
                                const std::string& app_locale,
                                Suggestion& suggestion) {
  // Creates a phone number suggestion for the autofill submenu. When triggered
  // from a phone number field this suggestion will fill every phone number
  // field. Otherwise it fills a specific field.
  bool phone_number_suggestion_added = false;
  if (profile.HasInfo(PHONE_HOME_WHOLE_NUMBER)) {
    Suggestion phone_number_suggestion(
        profile.GetInfo(PHONE_HOME_WHOLE_NUMBER, app_locale));
    const bool is_phone_field = type.group() == FieldTypeGroup::kPhone;
    phone_number_suggestion.popup_item_id =
        is_phone_field ? PopupItemId::kFillFullPhoneNumber
                       : PopupItemId::kFieldByFieldFilling;
    phone_number_suggestion.payload = Suggestion::BackendId(profile.guid());
    suggestion.children.push_back(std::move(phone_number_suggestion));
    phone_number_suggestion_added = true;
  }
  if (AddFieldByFieldSuggestions({EMAIL_ADDRESS}, profile, app_locale,
                                 suggestion.children) ||
      phone_number_suggestion_added) {
    suggestion.children.push_back(
        AutofillSuggestionGenerator::CreateSeparator());
  }
}

// Adds footer child suggestions to build autofill popup submenu.
void AddFooterChildSuggestions(
    const AutofillProfile& profile,
    absl::optional<ServerFieldTypeSet> last_targeted_fields,
    Suggestion& suggestion) {
  // If the last filling granularity was not full form, add the
  // `PopupItemId::kFillEverythingFromAddressProfile` suggestion. This allows
  // the user to go back to filling the whole form once in a more fine grained
  // filling experience.
  if (!last_targeted_fields || *last_targeted_fields != kAllServerFieldTypes) {
    suggestion.children.push_back(GetFillEverythingFromAddressProfileSuggestion(
        Suggestion::BackendId(profile.guid())));
  }
  suggestion.children.push_back(
      GetEditAddressProfileSuggestion(Suggestion::BackendId(profile.guid())));
  suggestion.children.push_back(
      GetDeleteAddressProfileSuggestion(Suggestion::BackendId(profile.guid())));
}

}  // namespace

void AddSuggestionDetailsForCurrentFillingGranularity(
    absl::optional<ServerFieldTypeSet> optional_last_targeted_fields,
    const AutofillType& triggering_field_type,
    Suggestion& suggestion) {
  const ServerFieldTypeSet& last_targeted_fields =
      optional_last_targeted_fields.value_or(kAllServerFieldTypes);

  if (AreFieldsGranularFillingGroup(last_targeted_fields)) {
    switch (triggering_field_type.group()) {
      case FieldTypeGroup::kName:
        suggestion.popup_item_id = PopupItemId::kFillFullName;
        break;
      case FieldTypeGroup::kAddress:
        suggestion.popup_item_id = PopupItemId::kFillFullAddress;
        break;
      case FieldTypeGroup::kPhone:
        suggestion.popup_item_id = PopupItemId::kFillFullPhoneNumber;
        break;
      default:
        // If the 'current_granularity' is group filling, BUT the current
        // focused field is not one for which group we offer group filling
        // (kName, kAddress and kPhone), we default back to fill full form
        // behaviour/pre-granular filling popup id.
        suggestion.popup_item_id = PopupItemId::kAddressEntry;
    }
  } else if (last_targeted_fields == kAllServerFieldTypes) {
    suggestion.popup_item_id = PopupItemId::kAddressEntry;
  } else if (last_targeted_fields.size() == 1) {
    // Note: This does not affect SingleFieldFormFillers such
    // Autocomplete, IBANs and merchand promo. Even though they also fill only
    // one field, they have different code paths, therefore their suggestions
    // are not generated here. Furthermore, we do not store
    // `last_targeted_fields` for them.
    suggestion.popup_item_id = PopupItemId::kFieldByFieldFilling;
  } else {
    NOTREACHED_NORETURN();
  }
}

void AddGranularFillingChildSuggestions(
    const AutofillType& type,
    absl::optional<ServerFieldTypeSet> last_targeted_fields,
    const AutofillProfile& profile,
    const std::string& app_locale,
    Suggestion& suggestion) {
  AddNameChildSuggestions(type, profile, app_locale, suggestion);
  AddAddressChildSuggestions(type, profile, app_locale, suggestion);
  AddContactChildSuggestions(type, profile, app_locale, suggestion);
  AddFooterChildSuggestions(profile, last_targeted_fields, suggestion);
}

}  // namespace autofill::suggestion_selection
