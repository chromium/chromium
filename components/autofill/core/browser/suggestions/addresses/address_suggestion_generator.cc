// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/addresses/address_suggestion_generator.h"

#include <algorithm>
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
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_browser_util.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile_comparator.h"
#include "components/autofill/core/browser/data_model/transliterator.h"
#include "components/autofill/core/browser/data_quality/autofill_data_util.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling/addresses/field_filling_address_util.h"
#include "components/autofill/core/browser/form_parsing/address_field_parser.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/geo/address_i18n.h"
#include "components/autofill/core/browser/geo/phone_number_i18n.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
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

// In addition to just getting the values out of the profile, this function
// handles type-specific formatting.
std::u16string GetProfileSuggestionMainText(const AutofillProfile& profile,
                                            const std::string& app_locale,
                                            FieldType trigger_field_type) {
  if (trigger_field_type == ADDRESS_HOME_STREET_ADDRESS) {
    std::string street_address_line;
    ::i18n::addressinput::GetStreetAddressLinesAsSingleLine(
        *i18n::CreateAddressDataFromAutofillProfile(profile, app_locale),
        &street_address_line);
    return base::UTF8ToUTF16(street_address_line);
  }
  return profile.GetInfo(trigger_field_type, app_locale);
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Returns the minimum number of fields that should be returned by
// `AutofillProfile::CreateInferredLabels()`, based on the type of the
// triggering field.
int GetNumberOfMinimalFieldsToShow(FieldType trigger_field_type) {
  if (GroupTypeOfFieldType(trigger_field_type) == FieldTypeGroup::kPhone) {
    // Phone fields are a special case. For them we want both the
    // `FULL_NAME` and `ADDRESS_HOME_LINE1` to be present.
    return 2;
  } else {
    return 1;
  }
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

// Returns for each profile in `profiles` a differentiating label string to be
// used as a secondary text in the corresponding suggestion bubble.
// `field_types` the types of the fields that will be filled by the suggestion.
std::vector<std::u16string> GetProfileSuggestionLabels(
    const std::vector<AutofillProfile>& profiles,
    const FieldTypeSet& field_types,
    FieldType trigger_field_type,
    const std::string& app_locale) {
  // Generate disambiguating labels based on the list of matches.
  std::vector<std::u16string> differentiating_labels;
  auto profile_ptrs =
      base::ToVector(profiles,
                     [](const AutofillProfile& profile)
                         -> raw_ptr<const AutofillProfile, VectorExperimental> {
                       return &profile;
                     });
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  if (base::FeatureList::IsEnabled(features::kAutofillImprovedLabels)) {
    differentiating_labels = AutofillProfile::CreateInferredLabels(
        profile_ptrs, /*suggested_fields=*/std::nullopt, trigger_field_type,
        {trigger_field_type},
        GetNumberOfMinimalFieldsToShow(trigger_field_type), app_locale,
        /*use_improved_labels_order=*/true);
  } else
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  {
    differentiating_labels = AutofillProfile::CreateInferredLabels(
        profile_ptrs, field_types, /*triggering_field_type=*/std::nullopt,
        {trigger_field_type},
        /*minimal_fields_shown=*/1, app_locale);
  }
  return differentiating_labels;
}

// For each profile in `profiles`, returns a vector of `Suggestion::labels` to
// be applied. Takes into account the `trigger_field_type` to add specific
// labels. Optionally adds a differentiating label if the Suggestion::main_text
// +  label is not unique.
std::vector<std::vector<Suggestion::Text>> CreateSuggestionLabels(
    const std::vector<AutofillProfile>& profiles,
    const FieldTypeSet& field_types,
    FieldType trigger_field_type,
    const std::string& app_locale) {
  // Suggestions for filling only one field (field-by-field filling) should not
  // have labels because they are guaranteed to be unique, see
  // `DeduplicatedProfilesForSuggestions()`.
  if (field_types.size() == 1) {
    return std::vector<std::vector<Suggestion::Text>>(profiles.size());
  }
  const std::vector<std::u16string> suggestions_differentiating_labels =
      GetProfileSuggestionLabels(profiles, field_types, trigger_field_type,
                                 app_locale);
  return base::ToVector(
      suggestions_differentiating_labels, [](const std::u16string& label) {
        return std::vector<Suggestion::Text>{Suggestion::Text(label)};
      });
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
  suggestion.acceptability = Suggestion::Acceptability::kUnacceptable;
  suggestion.children.emplace_back(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_TEST_ADDRESS_BY_COUNTRY),
      SuggestionType::kDevtoolsTestAddressByCountry);
  suggestion.children.back().acceptability =
      Suggestion::Acceptability::kUnacceptableWithDeactivatedStyle;
  suggestion.children.emplace_back(SuggestionType::kSeparator);
  for (const AutofillProfile& test_address : test_addresses) {
    const std::u16string test_address_country =
        test_address.GetInfo(ADDRESS_HOME_COUNTRY, locale);
    suggestion.children.emplace_back(test_address_country,
                                     SuggestionType::kDevtoolsTestAddressEntry);
    suggestion.children.back().payload = Suggestion::AutofillProfilePayload(
        Suggestion::Guid(test_address.guid()));
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
                         const std::u16string& field_contents_canon,
                         const std::string& app_locale) {
  std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>
      matched_profiles;
  for (const AutofillProfile* profile : profiles) {
    if (matched_profiles.size() == kMaxPrefixMatchedProfilesForSuggestion) {
      break;
    }
    std::u16string main_text =
        GetProfileSuggestionMainText(*profile, app_locale, trigger_field_type);
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
  if (profiles.empty()) {
    return;
  }
  const base::Time min_last_used =
      AutofillClock::Now() - kDisusedDataModelTimeDelta;
  auto is_profile_disused =
      [&min_last_used](
          const raw_ptr<const AutofillProfile, VectorExperimental>& profile) {
        return std::max(profile->usage_history().use_date(),
                        profile->usage_history().modification_date()) <=
               min_last_used;
      };
  const size_t original_size = profiles.size();
  // Exclude the first address from the list of potentially removed ones so that
  // this strategy never results in a non empty list becoming empty.
  profiles.erase(
      std::remove_if(profiles.begin() + 1, profiles.end(), is_profile_disused),
      profiles.end());
  AutofillMetrics::LogNumberOfAddressesSuppressedForDisuse(original_size -
                                                           profiles.size());
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
    const FieldTypeSet& field_types) {
  // Get the profiles to suggest, which are already sorted.
  std::vector<const AutofillProfile*> sorted_profiles =
      address_data.GetProfilesToSuggest();

  // Erase profiles which has empty value for the trigger field type.
  std::erase_if(sorted_profiles, [&](const AutofillProfile* profile) {
    return GetProfileSuggestionMainText(*profile, address_data.app_locale(),
                                        trigger_field_type)
        .empty();
  });
  std::vector<raw_ptr<const AutofillProfile, VectorExperimental>>
      profiles_to_suggest(sorted_profiles.begin(), sorted_profiles.end());

  // By default, suggestions should be matched with the field content.
  // However, for field by field filling suggestions, prefix matching is
  // disabled because we want to offer the user suggestions to swap the
  // current value of the field with something else, making the prefix
  // matching not useful.
  if (!field_is_autofilled) {
    profiles_to_suggest = GetPrefixMatchedProfiles(
        sorted_profiles, trigger_field_type,
        NormalizeForComparisonForType(field_contents, trigger_field_type),
        address_data.app_locale());
  }

  // Disused profiles are excluded only if the normalized field value is empty.
  if (NormalizeForComparisonForType(field_contents, trigger_field_type)
          .empty()) {
    RemoveDisusedSuggestions(profiles_to_suggest);
  }

  // By default, prefix matching and deduplication are sufficient filtering
  // mechanisms for suggestions. However, for field-by-field filling
  // suggestions, we also want to remove suggestions that have the same value
  // as the trigger field. Such suggestions would be useless and thus
  // add visual noise to the suggestion UI.
  //
  // This filtering logic should not result in removing all address suggestions,
  // but rather in reducing the number of suggestions displayed. This is why
  // filtering is only performed when more than one address is stored. It is
  // assumed that addresses filtered by this strategy will all have
  // different values for the trigger field.
  if (field_is_autofilled &&
      base::FeatureList::IsEnabled(
          features::kAutofillImproveAddressFieldSwapping) &&
      profiles_to_suggest.size() > 1) {
    std::erase_if(profiles_to_suggest, [&](const AutofillProfile* profile) {
      return NormalizeForComparisonForType(field_contents,
                                           trigger_field_type) ==
             NormalizeForComparisonForType(
                 GetProfileSuggestionMainText(
                     *profile, address_data.app_locale(), trigger_field_type),
                 trigger_field_type);
    });
  }
  // It is important that deduplication is the last filtering strategy to be
  // executed, otherwise some profiles could be deduplicated in favor of
  // another profile that is later removed by another filtering strategy.
    profiles_to_suggest = DeduplicatedProfilesForSuggestions(
        profiles_to_suggest, trigger_field_type, field_types,
        AutofillProfileComparator(address_data.app_locale()));

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
    const std::string& app_locale) {
  if (profiles.empty()) {
    return {};
  }

  std::vector<Suggestion::AutofillProfilePayload> payloads;
  payloads.reserve(profiles.size());
  for (AutofillProfile& profile : profiles) {
    std::u16string email_override;
    // If the following conditions are met:
    // - A plus address override is available
    // - The profile's email address is the same as the user's Google Account
    // email.
    // Then the profile's email address will be replaced with the plus
    // address in order to show the updated email on the suggestion label.
    if (plus_address_email_override && profile.HasInfo(EMAIL_ADDRESS) &&
        base::UTF16ToUTF8(profile.GetRawInfo(EMAIL_ADDRESS)) == gaia_email) {
      email_override = base::UTF8ToUTF16(*plus_address_email_override);
      profile.SetRawInfo(EMAIL_ADDRESS, email_override);
    }
    payloads.emplace_back(Suggestion::Guid(profile.guid()),
                          std::move(email_override));
  }

  std::vector<Suggestion> suggestions;
  std::vector<std::vector<Suggestion::Text>> labels = CreateSuggestionLabels(
      profiles, field_types, trigger_field_type, app_locale);
  FieldTypeGroup trigger_field_type_group =
      GroupTypeOfFieldType(trigger_field_type);
  // If `features::kAutofillImprovedLabels` is enabled, name fields should have
  // `NAME_FULL` as main text, unless in field by field filling mode.
  FieldType main_text_field_type =
      GroupTypeOfFieldType(trigger_field_type) == FieldTypeGroup::kName &&
              !IsAlternativeNameType(trigger_field_type) &&
              suggestion_type != SuggestionType::kAddressFieldByFieldFilling &&
              base::FeatureList::IsEnabled(features::kAutofillImprovedLabels) &&
              !features::kAutofillImprovedLabelsParamWithoutMainTextChangesParam
                   .Get()
          ? NAME_FULL
          : trigger_field_type;
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
    suggestion.payload = std::move(payloads[i]);
    suggestion.acceptance_a11y_announcement =
        l10n_util::GetStringUTF16(IDS_AUTOFILL_A11Y_ANNOUNCE_FILLED_FORM);
    suggestion.type = suggestion_type;
    suggestion.acceptability = Suggestion::Acceptability::kAcceptable;
    if (suggestion.type == SuggestionType::kAddressFieldByFieldFilling) {
      suggestion.field_by_field_filling_type_used =
          std::optional(trigger_field_type);
    }
    // We add an icon to the address (profile) suggestion if there is more than
    // one profile related field in the form. For email fields,
    // the email icon is used unconditionally to create consistency with plus
    // address suggestions.
    if (GroupTypeOfFieldType(trigger_field_type) == FieldTypeGroup::kEmail) {
      suggestion.icon = Suggestion::Icon::kEmail;
    } else {
      if (base::FeatureList::IsEnabled(
              features::kAutofillEnableSupportForHomeAndWork)) {
        // TODO(crbug.com/6373444): Confirm that the distance between icon and
        // text is correct.
        switch (profile.record_type()) {
          case AutofillProfile::RecordType::kAccountHome:
            suggestion.icon = Suggestion::Icon::kHome;
            suggestion.iph_metadata = Suggestion::IPHMetadata(
                &feature_engagement::
                    kIPHAutofillHomeWorkProfileSuggestionFeature);
            break;
          case AutofillProfile::RecordType::kAccountWork:
            suggestion.icon = Suggestion::Icon::kWork;
            suggestion.iph_metadata = Suggestion::IPHMetadata(
                &feature_engagement::
                    kIPHAutofillHomeWorkProfileSuggestionFeature);
            break;
          case AutofillProfile::RecordType::kLocalOrSyncable:
          case AutofillProfile::RecordType::kAccount:
            suggestion.icon = Suggestion::Icon::kAccount;
        }
      } else {
        suggestion.icon = Suggestion::Icon::kAccount;
      }
    }
    // This is intentionally not using `profile.IsAccountProfile()` because the
    // IPH should only be shown for non-H/W profiles.
    if (profile.record_type() == AutofillProfile::RecordType::kAccount &&
        profile.initial_creator_id() !=
            AutofillProfile::kInitialCreatorOrModifierChrome) {
      suggestion.iph_metadata = Suggestion::IPHMetadata(
          &feature_engagement::
              kIPHAutofillExternalAccountProfileSuggestionFeature);
    }
  }
  return suggestions;
}

}  // namespace

std::vector<Suggestion> GetSuggestionsOnTypingForProfile(
    const AddressDataManager& address_data_manager,
    const std::u16string& field_contents) {
  // Get the profiles to suggest, which are already sorted by relevance.
  std::vector<const AutofillProfile*> profiles =
      address_data_manager.GetProfilesToSuggest();
  if (profiles.empty()) {
    return {};
  }

  // The minimum number of characters a user needs to type to maybe see a
  // suggestion.
  static constexpr size_t kMinNumberCharactersToMatch = 3;
  // This defines the maximum number of characters typed until suggestions are
  // no longer displayed.
  static constexpr size_t kMaxNumberCharactersToMatch = 10;
  // Defines the required number of characters that need to be missing between
  // the typed data and the profile data. This makes sure the value
  // offered by the feature is higher, by for example not displaying a
  // suggestion to fill "Tomas" when the user typed "Tom", since at this point
  // users are more likely to simply finish typing.
  static constexpr size_t kMinMissingCharactersNumber = 5;
  // Field types we are interested in showing suggestions for.
  // TODO(crbug.com/381994105): Add a finch parameter to easily experiment with
  // adding and removing field types.
  static constexpr FieldTypeSet kTypes = {NAME_FULL,
                                          NAME_LAST,
                                          NAME_LAST_SECOND,
                                          COMPANY_NAME,
                                          ADDRESS_HOME_LINE1,
                                          ADDRESS_HOME_LINE2,
                                          ADDRESS_HOME_LINE3,
                                          ADDRESS_HOME_STREET_ADDRESS,
                                          ADDRESS_HOME_CITY,
                                          ADDRESS_HOME_STATE,
                                          ADDRESS_HOME_COUNTRY,
                                          ADDRESS_HOME_STREET_NAME,
                                          EMAIL_ADDRESS,
                                          EMAIL_OR_LOYALTY_MEMBERSHIP_ID,
                                          PHONE_HOME_CITY_AND_NUMBER,
                                          PHONE_HOME_WHOLE_NUMBER,
                                          ADDRESS_HOME_ZIP};
  // Some field types require only `kMinNumberCharactersToMatch - 1` matching
  // characters for a suggestion to be shown. The assumption is that these field
  // types do not need the same matching prefix length to produce less false
  // positives.
  static constexpr FieldTypeSet kTypesWithLessRequiredMatchingCharacters = {
      ADDRESS_HOME_ZIP};

  std::vector<Suggestion> suggestions;
  std::set<std::u16string> suggestions_text;
  // The number of profiles that data will be derived from when generating
  // suggestions.
  static constexpr size_t kMaxNumberProfilesToUse = 2;
  size_t profiles_used_count = 0;
  for (const AutofillProfile* profile : profiles) {
    if (profiles_used_count == kMaxNumberProfilesToUse) {
      break;
    }
    profiles_used_count++;

    for (FieldType type : kTypes) {
      const size_t effective_num_characters_to_match =
          kTypesWithLessRequiredMatchingCharacters.contains(type)
              ? kMinNumberCharactersToMatch - 1
              : kMinNumberCharactersToMatch;

      const std::u16string normalized_field_contents =
          NormalizeForComparisonForType(field_contents, type);
      if (normalized_field_contents.size() <
          effective_num_characters_to_match) {
        // Sometimes normalizing the string makes it shorter because of trimming
        // spaces.
        continue;
      }

      if (normalized_field_contents.size() > kMaxNumberCharactersToMatch) {
        continue;
      }

      std::u16string suggestion_text =
          profile->GetInfo(type, address_data_manager.app_locale());
      const std::u16string profile_data =
          NormalizeForComparisonForType(suggestion_text, type);

      if (profile_data.empty()) {
        continue;
      }

      if (!IsValidAddressSuggestionForFieldContents(
              profile_data, normalized_field_contents, type)) {
        continue;
      }

      if (profile_data.size() - normalized_field_contents.size() <
          kMinMissingCharactersNumber) {
        continue;
      }

      // Do not allow duplicated suggestions, for example if
      // `ADDRESS_HOME_LINE1` and
      // `ADDRESS_HOME_STREET_ADDRESS` hold the same data.
      if (!suggestions_text.contains(suggestion_text)) {
        suggestions.emplace_back(suggestion_text,
                                 SuggestionType::kAddressEntryOnTyping);
        suggestions.back().field_by_field_filling_type_used = type;
        suggestions.back().payload = Suggestion::AutofillProfilePayload(
            Suggestion::Guid(profile->guid()));
        suggestions_text.insert(suggestion_text);
      }
    }
  }
  if (suggestions.size() > 0) {
    // TODO(crbug.com/381994105): Consider adding undo.
    std::ranges::move(GetAddressFooterSuggestions(/*is_autofilled=*/false),
                      std::back_inserter(suggestions));
  }
  return suggestions;
}

std::vector<Suggestion> GetSuggestionsForProfiles(
    const AutofillClient& client,
    const FieldTypeSet& field_types,
    const FormFieldData& trigger_field,
    FieldType trigger_field_type,
    SuggestionType suggestion_type,
    std::optional<std::string> plus_address_email_override) {
  std::vector<AutofillProfile> profiles_to_suggest = GetProfilesToSuggest(
      client.GetPersonalDataManager().address_data_manager(),
      trigger_field_type, trigger_field.value(), trigger_field.is_autofilled(),
      field_types);
  const std::string gaia_email =
      client.GetIdentityManager()
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .email;
  std::vector<Suggestion> suggestions = CreateSuggestionsFromProfiles(
      std::move(profiles_to_suggest), gaia_email, field_types, suggestion_type,
      trigger_field_type, trigger_field.max_length(),
      std::move(plus_address_email_override), client.GetAppLocale());

  // Add devtools test addresses suggestion if it exists. A suggestion will
  // exist if devtools is open and therefore test addresses were set.
  if (std::optional<Suggestion> test_addresses_suggestion =
          GetSuggestionForTestAddresses(client.GetTestAddresses(),
                                        client.GetAppLocale())) {
    suggestions.push_back(std::move(*test_addresses_suggestion));
  }
  if (suggestions.empty()) {
    return suggestions;
  }
  std::ranges::move(GetAddressFooterSuggestions(trigger_field.is_autofilled()),
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
    const FieldTypeSet& field_types) {
  return GetProfilesToSuggest(address_data, trigger_field_type, field_contents,
                              field_is_autofilled, field_types);
}

std::vector<Suggestion> CreateSuggestionsFromProfilesForTest(
    std::vector<AutofillProfile> profiles,
    const FieldTypeSet& field_types,
    SuggestionType suggestion_type,
    FieldType trigger_field_type,
    uint64_t trigger_field_max_length,
    const std::string& app_locale,
    std::optional<std::string> plus_address_email_override,
    const std::string& gaia_email) {
  return CreateSuggestionsFromProfiles(
      std::move(profiles), gaia_email, field_types, suggestion_type,
      trigger_field_type, trigger_field_max_length, plus_address_email_override,
      app_locale);
}

}  // namespace autofill
