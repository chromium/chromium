// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/autofill/core/browser/ui/suggestion_selection.h"

#include <string>
#include <unordered_set>
#include <vector>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_data_util.h"
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
using ::i18n::addressinput::AddressField;
using ::i18n::addressinput::GetStreetAddressLinesAsSingleLine;
using ::i18n::addressinput::STREET_ADDRESS;

// In addition to just getting the values out of the autocomplete profile, this
// function handles formatting of the street address into a single string.
std::u16string GetInfoInOneLine(const AutofillProfile* profile,
                                const AutofillType& type,
                                const std::string& app_locale) {
  AddressField address_field;
  if (i18n::FieldForType(type.GetStorableType(), &address_field) &&
      address_field == STREET_ADDRESS) {
    std::string street_address_line;
    GetStreetAddressLinesAsSingleLine(
        *i18n::CreateAddressDataFromAutofillProfile(*profile, app_locale),
        &street_address_line);
    return base::UTF8ToUTF16(street_address_line);
  }

  return profile->GetInfo(type, app_locale);
}

// // Sets the `popup_item_id` for `suggestion` depending on
// `last_filling_granularity`. If the `last_filling_granularity` for a certain
// form was group filling, also add labels to give users feedback about the next
// filling behaviour.
// TODO(crbug.com/1466116): Possibly update the filling granularity check once
// https://chromium-review.googlesource.com/c/chromium/src/+/4778661 has been
// submitted.
// TODO(crbug.com/1466116): Add tests when this is actually used.
// TODO(crbug.com/1466116): Add labels when `last_filling_granularity` is group
// filling.
void AddSuggestionDetailsForCurrentFillingGranularity(
    FillingGranularity last_filling_granularity,
    const AutofillType& triggering_field_type,
    Suggestion& suggestion) {
  switch (last_filling_granularity) {
    case FillingGranularity::kGroupFilling: {
      switch (triggering_field_type.group()) {
        case FieldTypeGroup::kName:
          suggestion.popup_item_id = PopupItemId::kFillFullName;
          return;
        case FieldTypeGroup::kAddress:
          suggestion.popup_item_id = PopupItemId::kFillFullAddress;
          return;
        case FieldTypeGroup::kPhone:
          suggestion.popup_item_id = PopupItemId::kFillFullPhoneNumber;
          return;
        default:
          // If the `current_granularity` is group filling, BUT the current
          // focused field is not one for which group we offer group filling
          // (kName, kAddress and kPhone), we default back to fill full form
          // behaviour/pre-granular filling popup id.
          suggestion.popup_item_id = PopupItemId::kAddressEntry;
          return;
      }
    }
    case FillingGranularity::kFieldByFieldFilling:
      suggestion.popup_item_id = PopupItemId::kFieldByFieldFilling;
      return;
    case FillingGranularity::kFillForm:
      suggestion.popup_item_id = PopupItemId::kAddressEntry;
      return;
    default:
      NOTREACHED();
  }
}

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
// TODO(crbug.com/1459990): Use this once the new popup with submenus
// implementation is complete.
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
void AddFooterChildSuggestions(const AutofillProfile& profile,
                               Suggestion& suggestion) {
  // TODO(crbug.com/1459990): Add fill everything option if current filling is
  // not in the form filling granularity level. This would mean that an user
  // just filled specifics groups or fields.
  suggestion.children.push_back(
      GetEditAddressProfileSuggestion(Suggestion::BackendId(profile.guid())));
  suggestion.children.push_back(
      GetDeleteAddressProfileSuggestion(Suggestion::BackendId(profile.guid())));
}

// Creates nested/child suggestions for `suggestion` with the `profile`
// information. Uses `type` to define what group filling suggestion to add
// (name, address or phone). The existence of child suggestions defines whether
// the autofill popup will have submenus.
void AddGranularFillingChildSuggestions(const AutofillType& type,
                                        const AutofillProfile& profile,
                                        const std::string& app_locale,
                                        Suggestion& suggestion) {
  AddNameChildSuggestions(type, profile, app_locale, suggestion);
  AddAddressChildSuggestions(type, profile, app_locale, suggestion);
  AddContactChildSuggestions(type, profile, app_locale, suggestion);
  AddFooterChildSuggestions(profile, suggestion);
}

}  // namespace

// As of November 2018, 50 profiles should be more than enough to cover at least
// 99% of all times the dropdown is shown.
constexpr size_t kMaxSuggestedProfilesCount = 50;

// As of November 2018, displaying 10 suggestions cover at least 99% of the
// indices clicked by our users. The suggestions will also refine as they type.
constexpr size_t kMaxUniqueSuggestionsCount = 10;

std::u16string NormalizeForComparisonForType(const std::u16string& text,
                                             ServerFieldType type) {
  if (GroupTypeOfServerFieldType(type) == FieldTypeGroup::kEmail) {
    // For emails, keep special characters so that if the user has two emails
    // `test@foo.xyz` and `test1@foo.xyz` saved, only the first one is suggested
    // upon entering `test@` into the email field.
    return RemoveDiacriticsAndConvertToLowerCase(text);
  }
  return AutofillProfileComparator::NormalizeForComparison(text);
}

std::vector<Suggestion> GetPrefixMatchedSuggestions(
    const AutofillType& type,
    const std::u16string& raw_field_contents,
    const std::u16string& field_contents_canon,
    const std::string& app_locale,
    bool field_is_autofilled,
    const std::vector<AutofillProfile*>& profiles,
    std::vector<AutofillProfile*>* matched_profiles) {
  std::vector<Suggestion> suggestions;

  for (size_t i = 0; i < profiles.size() &&
                     matched_profiles->size() < kMaxSuggestedProfilesCount;
       i++) {
    AutofillProfile* profile = profiles[i];

    // Don't offer to fill the exact same value again. If detailed suggestions
    // with different secondary data is available, it would appear to offer
    // refilling the whole form with something else. E.g. the same name with a
    // work and a home address would appear twice but a click would be a noop.
    // TODO(fhorschig): Consider refilling form instead (at least on Android).
#if BUILDFLAG(IS_ANDROID)
    if (field_is_autofilled &&
        profile->GetRawInfo(type.GetStorableType()) == raw_field_contents) {
      continue;
    }
#endif  // BUILDFLAG(IS_ANDROID)

    std::u16string value = GetInfoInOneLine(profile, type, app_locale);
    if (value.empty())
      continue;

    bool prefix_matched_suggestion;
    std::u16string suggestion_canon =
        NormalizeForComparisonForType(value, type.GetStorableType());
    if (IsValidSuggestionForFieldContents(
            suggestion_canon, field_contents_canon, type,
            /* is_masked_server_card= */ false, field_is_autofilled,
            &prefix_matched_suggestion)) {
      matched_profiles->push_back(profile);

      if (type.group() == FieldTypeGroup::kPhone) {
        bool format_phone;

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
        format_phone = base::FeatureList::IsEnabled(
            features::kAutofillUseMobileLabelDisambiguation);
#else
        format_phone = base::FeatureList::IsEnabled(
            features::kAutofillUseImprovedLabelDisambiguation);
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

        if (format_phone) {
          // Formats, e.g., the US phone numbers 15084880800, 508 488 0800, and
          // +15084880800, as (508) 488-0800, and the Brazilian phone numbers
          // 21987650000 and +55 11 2648-0254 as (21) 98765-0000 and
          // (11) 2648-0254, respectively.
          value = base::UTF8ToUTF16(i18n::FormatPhoneNationallyForDisplay(
              base::UTF16ToUTF8(value),
              data_util::GetCountryCodeWithFallback(*profile, app_locale)));
        }
      }

      suggestions.emplace_back(value);
      suggestions.back().payload = Suggestion::BackendId(profile->guid());
      suggestions.back().match = prefix_matched_suggestion
                                     ? Suggestion::PREFIX_MATCH
                                     : Suggestion::SUBSTRING_MATCH;
      suggestions.back().acceptance_a11y_announcement =
          l10n_util::GetStringUTF16(IDS_AUTOFILL_A11Y_ANNOUNCE_FILLED_FORM);
      if (base::FeatureList::IsEnabled(
              features::kAutofillGranularFillingAvailable)) {
        AddGranularFillingChildSuggestions(type, *profile, app_locale,
                                           suggestions.back());

        // TODO(crbug.com/1466116): Make this hard coded value a param to
        // GetPrefixMatchedSuggestions.
        const FillingGranularity kLastFillingGranularity =
            FillingGranularity::kGroupFilling;
        AddSuggestionDetailsForCurrentFillingGranularity(
            kLastFillingGranularity, type, suggestions.back());
      }
    }
  }

  // Prefix matches should precede other token matches.
  if (IsFeatureSubstringMatchEnabled()) {
    std::stable_sort(suggestions.begin(), suggestions.end(),
                     [](const Suggestion& a, const Suggestion& b) {
                       return a.match < b.match;
                     });
  }

  return suggestions;
}

std::vector<Suggestion> GetUniqueSuggestions(
    const ServerFieldTypeSet& field_types,
    const AutofillProfileComparator& comparator,
    const std::vector<AutofillProfile*> matched_profiles,
    const std::vector<Suggestion>& suggestions,
    std::vector<AutofillProfile*>* unique_matched_profiles) {
  std::vector<Suggestion> unique_suggestions;

  // Limit number of unique profiles as having too many makes the
  // browser hang due to drawing calculations (and is also not
  // very useful for the user).
  for (size_t i = 0; i < matched_profiles.size() &&
                     unique_suggestions.size() < kMaxUniqueSuggestionsCount;
       ++i) {
    bool include = true;
    AutofillProfile* profile_a = matched_profiles[i];
    for (size_t j = 0; j < matched_profiles.size(); ++j) {
      AutofillProfile* profile_b = matched_profiles[j];
      // Check if profile A is a subset of profile B. If not, continue.
      if (i == j ||
          !comparator.Compare(suggestions[i].main_text.value,
                              suggestions[j].main_text.value) ||
          !profile_a->IsSubsetOfForFieldSet(comparator, *profile_b,
                                            field_types)) {
        continue;
      }

      // Check if profile B is also a subset of profile A. If so, the
      // profiles are identical and only one should be included.
      // Prefer `kAccount` profiles over `kLocalOrSyncable` ones. In case the
      // profiles have the same source, prefer the earlier one (since the
      // profiles are pre-sorted by their relevance).
      const bool prefer_a_over_b =
          profile_a->source() == profile_b->source()
              ? i < j
              : profile_a->source() == AutofillProfile::Source::kAccount;
      if (prefer_a_over_b && profile_b->IsSubsetOfForFieldSet(
                                 comparator, *profile_a, field_types)) {
        continue;
      }

      // One-way subset. Don't include profile A.
      include = false;
      break;
    }
    if (include) {
      unique_matched_profiles->push_back(profile_a);
      unique_suggestions.push_back(suggestions[i]);
    }
  }
  return unique_suggestions;
}

bool IsValidSuggestionForFieldContents(std::u16string suggestion_canon,
                                       std::u16string field_contents_canon,
                                       const AutofillType& type,
                                       bool is_masked_server_card,
                                       bool field_is_autofilled,
                                       bool* is_prefix_matched) {
  *is_prefix_matched = true;

  // Phones should do a substring match because they can be trimmed to remove
  // the first parts (e.g. country code or prefix). It is still considered a
  // prefix match in order to put it at the top of the suggestions.
  if (type.group() == FieldTypeGroup::kPhone &&
      suggestion_canon.find(field_contents_canon) != std::u16string::npos) {
    return true;
  }

  // For card number fields, suggest the card if:
  // - the number matches any part of the card, or
  // - it's a masked card and there are 6 or fewer typed so far.
  // - it's a masked card, field is autofilled, and the last 4 digits in the
  // field match the last 4 digits of the card.
  if (type.GetStorableType() == CREDIT_CARD_NUMBER) {
    if (suggestion_canon.find(field_contents_canon) != std::u16string::npos) {
      return true;
    }

    if (is_masked_server_card) {
      if (field_contents_canon.length() < 6) {
        return true;
      }
      if (field_is_autofilled) {
        int field_contents_length = field_contents_canon.length();
        DCHECK(field_contents_length >= 4);
        if (suggestion_canon.find(field_contents_canon.substr(
                field_contents_length - 4, field_contents_length)) !=
            std::u16string::npos) {
          return true;
        }
      }
    }

    return false;
  }

  if (base::StartsWith(suggestion_canon, field_contents_canon,
                       base::CompareCase::SENSITIVE)) {
    return true;
  }

  if (IsFeatureSubstringMatchEnabled() &&
      suggestion_canon.length() >= field_contents_canon.length() &&
      GetTextSelectionStart(suggestion_canon, field_contents_canon, false) !=
          std::u16string::npos) {
    *is_prefix_matched = false;
    return true;
  }

  return false;
}

void RemoveProfilesNotUsedSinceTimestamp(
    base::Time min_last_used,
    std::vector<AutofillProfile*>* profiles) {
  const size_t original_size = profiles->size();
  profiles->erase(
      std::stable_partition(profiles->begin(), profiles->end(),
                            [min_last_used](const AutofillDataModel* m) {
                              return m->use_date() > min_last_used;
                            }),
      profiles->end());
  const size_t num_profiles_suppressed = original_size - profiles->size();
  AutofillMetrics::LogNumberOfAddressesSuppressedForDisuse(
      num_profiles_suppressed);
}

void PrepareSuggestions(const std::vector<std::u16string>& labels,
                        std::vector<Suggestion>* suggestions,
                        const AutofillProfileComparator& comparator) {
  DCHECK_EQ(suggestions->size(), labels.size());

  // This set is used to determine whether duplicate Suggestions exist. For
  // example, a Suggestion with the value "John" and the label "400 Oak Rd" has
  // the normalized text "john400oakrd". This text can only be added to the set
  // once.
  std::unordered_set<std::u16string> suggestion_text;
  size_t index_to_add_suggestion = 0;

  // Dedupes Suggestions to show in the dropdown once values and labels have
  // been created. This is useful when LabelFormatters make Suggestions' labels.
  //
  // Suppose profile A has the data John, 400 Oak Rd, and (617) 544-7411 and
  // profile B has the data John, 400 Oak Rd, (508) 957-5009. If a formatter
  // puts only 400 Oak Rd in the label, then there will be two Suggestions with
  // the normalized text "john400oakrd", and the Suggestion with the lower
  // ranking should be discarded.
  for (size_t i = 0; i < labels.size(); ++i) {
    std::u16string label = labels[i];

    bool text_inserted =
        suggestion_text
            .insert(AutofillProfileComparator::NormalizeForComparison(
                (*suggestions)[i].main_text.value + label,
                AutofillProfileComparator::DISCARD_WHITESPACE))
            .second;

    if (text_inserted) {
      if (index_to_add_suggestion != i) {
        (*suggestions)[index_to_add_suggestion] = (*suggestions)[i];
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
              (*suggestions)[index_to_add_suggestion].main_text.value,
              labels[i]) &&
          !labels[i].empty()) {
        (*suggestions)[index_to_add_suggestion].labels = {
            {Suggestion::Text(labels[i])}};
      }
      ++index_to_add_suggestion;
    }
  }

  if (index_to_add_suggestion < suggestions->size()) {
    suggestions->resize(index_to_add_suggestion);
  }
}

}  // namespace autofill::suggestion_selection
