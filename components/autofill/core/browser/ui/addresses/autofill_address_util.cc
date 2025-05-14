// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/addresses/autofill_address_util.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>

#include "autofill_address_util.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/to_vector.h"
#include "base/memory/ptr_util.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile_comparator.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/address_i18n.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/ui/country_combobox_model.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_ui.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_ui_component.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/base/l10n/l10n_util.h"

using i18n::addressinput::AddressField;
using i18n::addressinput::Localization;

namespace autofill {

namespace {

using ::i18n::addressinput::AddressUiComponent;

constexpr char kAddressComponentsFallbackCountryCode[] = "US";

AutofillAddressUIComponent::LengthHint ConvertLengthHint(
    AddressUiComponent::LengthHint length_hint) {
  switch (length_hint) {
    case AddressUiComponent::LengthHint::HINT_LONG:
      return AutofillAddressUIComponent::LengthHint::HINT_LONG;
    case AddressUiComponent::LengthHint::HINT_SHORT:
      return AutofillAddressUIComponent::LengthHint::HINT_SHORT;
  }
  NOTREACHED();
}
}  // namespace

std::vector<AutofillAddressUIComponent> ConvertAddressUiComponents(
    const std::vector<AddressUiComponent>& addressinput_components,
    const AutofillCountry& country) {
  return base::ToVector(
      addressinput_components, [&country](const AddressUiComponent& component) {
        // The component's field property may not be initialized if the
        // component is literal, so it should not be used to avoid
        // memory sanitizer's errors (`use-of-uninitialized-value`).
        if (!component.literal.empty()) {
          return AutofillAddressUIComponent{
              .literal = component.literal,
          };
        }
        FieldType field = i18n::TypeForField(component.field);
        return AutofillAddressUIComponent{
            .field = field,
            .name = component.name,
            .length_hint = ConvertLengthHint(component.length_hint),
            .literal = component.literal,
            .is_required = country.IsAddressFieldRequired(field),
        };
      });
}

void ExtendAddressComponents(
    std::vector<AutofillAddressUIComponent>& components,
    const AutofillCountry& country,
    const Localization& localization,
    bool include_literals) {
  for (const AutofillCountry::AddressFormatExtension& rule :
       country.address_format_extensions()) {
    // Find the location of `rule.placed_after` in `components`.
    // `components.field` is only valid if `components.literal.empty()`.
    auto prev_component = std::ranges::find_if(
        components, [&rule](const AutofillAddressUIComponent& component) {
          return component.literal.empty() &&
                 component.field == rule.placed_after;
        });
    CHECK(prev_component != components.end());

    // Insert the separator and `rule.type` after `prev_component`.
    if (include_literals) {
      prev_component = components.insert(
          ++prev_component,
          AutofillAddressUIComponent{
              .literal = std::string(rule.separator_before_label)});
    }

    components.insert(
        ++prev_component,
        AutofillAddressUIComponent{
            .field = rule.type,
            .name = localization.GetString(rule.label_id),
            .length_hint = rule.large_sized
                               ? AutofillAddressUIComponent::HINT_LONG
                               : AutofillAddressUIComponent::HINT_SHORT,
            .is_required = country.IsAddressFieldRequired(rule.type)});
  }
}

void GetAddressComponents(
    const std::string& country_code,
    const std::string& ui_language_code,
    bool include_literals,
    std::vector<std::vector<AutofillAddressUIComponent>>* address_components,
    std::string* components_language_code) {
  std::string not_used;
  // The `include_literals` used below is different
  // from the one passed to this function. The latter controls whether to
  // include line separators, while the former controls whether to split
  // components into lines.
  std::vector<AutofillAddressUIComponent> components = GetAddressComponents(
      country_code, ui_language_code,
      /*enable_field_labels_localization=*/true,
      /*include_literals=*/true,
      components_language_code ? *components_language_code : not_used);
  std::vector<AutofillAddressUIComponent>* line_components = nullptr;
  for (const AutofillAddressUIComponent& component : components) {
    // Start a new line if this is the first line, or a new line literal exists.
    if (!line_components || component.literal == "\n") {
      address_components->push_back(std::vector<AutofillAddressUIComponent>());
      line_components = &address_components->back();
    }

    if (!component.literal.empty()) {
      if (!include_literals) {
        continue;
      }
      // No need to return new line literals since components are split into
      // different lines anyway (one line per vector).
      if (component.literal == "\n") {
        continue;
      }
    }

    line_components->push_back(component);
  }
  // Filter empty lines. Those can appear e.g. when the line consists only of
  // literals and |include_literals| is false.
  auto to_remove = std::ranges::remove_if(
      *address_components, [](auto line) { return line.empty(); });
  address_components->erase(to_remove.begin(), to_remove.end());
}

std::vector<AutofillAddressUIComponent> GetAddressComponents(
    const std::string& country_code,
    const std::string& ui_language_code,
    bool enable_field_labels_localization,
    bool include_literals,
    std::string& components_language_code) {
  // Return strings in the current application locale.
  Localization localization;
  if (enable_field_labels_localization) {
    localization.SetGetter(l10n_util::GetStringUTF8);
  }
  AutofillCountry country(country_code);

  std::vector<AutofillAddressUIComponent> components;
  if (include_literals) {
    components = ConvertAddressUiComponents(
        ::i18n::addressinput::BuildComponentsWithLiterals(
            country_code, localization, ui_language_code,
            &components_language_code),
        country);
  } else {
    components = ConvertAddressUiComponents(
        ::i18n::addressinput::BuildComponents(country_code, localization,
                                              ui_language_code,
                                              &components_language_code),
        country);
  }

  if (!components.empty()) {
    ExtendAddressComponents(components, country, localization,
                            /*include_literals=*/include_literals);
    return components;
  } else if (country_code != kAddressComponentsFallbackCountryCode) {
    return GetAddressComponents(kAddressComponentsFallbackCountryCode,
                                ui_language_code,
                                enable_field_labels_localization,
                                include_literals, components_language_code);
  }

  NOTREACHED();
}

std::u16string GetEnvelopeStyleAddress(const AutofillProfile& profile,
                                       const std::string& ui_language_code,
                                       bool include_recipient,
                                       bool include_country) {
  const std::u16string& country_code = profile.GetInfo(
      AutofillType(HtmlFieldType::kCountryCode), ui_language_code);

  std::string not_used;
  std::vector<AutofillAddressUIComponent> components =
      GetAddressComponents(base::UTF16ToUTF8(country_code), ui_language_code,
                           /*enable_field_labels_localization=*/true,
                           /*include_literals=*/true, not_used);

  DCHECK(!components.empty());
  std::string address;
  for (const AutofillAddressUIComponent& component : components) {
    // Add string literals directly.
    if (!component.literal.empty()) {
      address += component.literal;
      continue;
    }
    if (!include_recipient && (component.field == NAME_FULL ||
                               component.field == ALTERNATIVE_FULL_NAME)) {
      continue;
    }
    FieldType type = component.field;
    address += base::UTF16ToUTF8(profile.GetInfo(type, ui_language_code));
  }
  if (include_country) {
    address += "\n";
    address += base::UTF16ToUTF8(
        profile.GetInfo(ADDRESS_HOME_COUNTRY, ui_language_code));
  }

  // Remove all white spaces and new lines from the beginning and the end of the
  // address.
  base::TrimString(address, base::kWhitespaceASCII, &address);

  // Collapse new lines to remove empty lines.
  re2::RE2::GlobalReplace(&address, re2::RE2("\\n+"), "\n");

  // Collapse white spaces.
  re2::RE2::GlobalReplace(&address, re2::RE2("[ ]+"), " ");

  return base::UTF8ToUTF16(address);
}

std::u16string GetProfileDescription(const AutofillProfile& profile,
                                     const std::string& ui_language_code,
                                     bool include_address_and_contacts) {
  // All user-visible fields.
  static constexpr FieldType kDetailsFields[] = {
      NAME_FULL,
      ADDRESS_HOME_LINE1,
      ADDRESS_HOME_LINE2,
      ADDRESS_HOME_DEPENDENT_LOCALITY,
      ADDRESS_HOME_CITY,
      ADDRESS_HOME_STATE,
      ADDRESS_HOME_ZIP,
      EMAIL_ADDRESS,
      PHONE_HOME_WHOLE_NUMBER,
      COMPANY_NAME,
      ADDRESS_HOME_COUNTRY};

  if (!include_address_and_contacts) {
    return profile.GetInfo(NAME_FULL, ui_language_code);
  }

  return profile.ConstructInferredLabel(
      kDetailsFields, /*num_fields_to_include=*/2, ui_language_code);
}

std::vector<ProfileValueDifference> GetProfileDifferenceForUi(
    const AutofillProfile& first_profile,
    const AutofillProfile& second_profile,
    const std::string& app_locale) {
  std::vector<ProfileValueDifference> differences_for_ui =
      AutofillProfileComparator::GetProfileDifference(
          first_profile, second_profile,
          {NAME_FULL, ALTERNATIVE_FULL_NAME, EMAIL_ADDRESS,
           PHONE_HOME_WHOLE_NUMBER},
          app_locale);

  // ADDRESS_HOME_ADDRESS is handled separately.
  std::u16string first_address = GetEnvelopeStyleAddress(
      first_profile, app_locale, /*include_recipient=*/false,
      /*include_country=*/true);
  std::u16string second_address = GetEnvelopeStyleAddress(
      second_profile, app_locale, /*include_recipient=*/false,
      /*include_country=*/true);
  if (first_address != second_address) {
    differences_for_ui.push_back(
        {ADDRESS_HOME_ADDRESS, first_address, second_address});
  }
  static constexpr auto kPriorityOrder =
      std::to_array({NAME_FULL, ALTERNATIVE_FULL_NAME, ADDRESS_HOME_ADDRESS,
                     EMAIL_ADDRESS, PHONE_HOME_WHOLE_NUMBER});

  std::erase_if(differences_for_ui, [](const ProfileValueDifference& diff) {
    return !base::Contains(kPriorityOrder, diff.type);
  });

  auto get_priority = [](FieldType type) -> size_t {
    auto it = std::ranges::find(kPriorityOrder, type);
    CHECK(it != kPriorityOrder.end());
    return std::distance(kPriorityOrder.begin(), it);
  };

  std::ranges::sort(differences_for_ui,
                    [get_priority](const ProfileValueDifference& a,
                                   const ProfileValueDifference& b) {
                      return get_priority(a.type) < get_priority(b.type);
                    });
  return differences_for_ui;
}

std::u16string GetProfileSummaryForMigrationPrompt(
    const AutofillProfile& profile,
    const std::string& app_locale) {
  std::vector<FieldType> fields = {
      FieldType::NAME_FULL, FieldType::ADDRESS_HOME_LINE1,
      FieldType::EMAIL_ADDRESS, FieldType::PHONE_HOME_WHOLE_NUMBER};
  std::vector<std::u16string> values;
  values.reserve(fields.size());
  for (FieldType field : fields) {
    std::u16string value = profile.GetInfo(field, app_locale);
    if (!value.empty()) {
      values.push_back(value);
    }
  }
  return base::JoinString(values, u"\n");
}

AddressUIComponentIconType GetAddressUIComponentIconTypeForFieldType(
    FieldType field_type) {
  switch (field_type) {
    case NAME_FULL:
    case ALTERNATIVE_FULL_NAME:
      return AddressUIComponentIconType::kName;
    case ADDRESS_HOME_STREET_ADDRESS:
    case ADDRESS_HOME_ADDRESS:
      return AddressUIComponentIconType::kAddress;
    case EMAIL_ADDRESS:
      return AddressUIComponentIconType::kEmail;
    case PHONE_HOME_WHOLE_NUMBER:
      return AddressUIComponentIconType::kPhone;
    case NAME_HONORIFIC_PREFIX:
    case NAME_FIRST:
    case NAME_MIDDLE:
    case NAME_LAST:
    case NAME_LAST_FIRST:
    case NAME_LAST_CONJUNCTION:
    case NAME_LAST_SECOND:
    case NAME_MIDDLE_INITIAL:
    case NAME_SUFFIX:
    case NAME_LAST_PREFIX:
    case NAME_LAST_CORE:
    case ALTERNATIVE_GIVEN_NAME:
    case ALTERNATIVE_FAMILY_NAME:
    case USERNAME_AND_EMAIL_ADDRESS:
    case PHONE_HOME_NUMBER:
    case PHONE_HOME_NUMBER_PREFIX:
    case PHONE_HOME_NUMBER_SUFFIX:
    case PHONE_HOME_CITY_CODE:
    case PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX:
    case PHONE_HOME_COUNTRY_CODE:
    case PHONE_HOME_CITY_AND_NUMBER:
    case PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX:
    case PHONE_HOME_EXTENSION:
    case CREDIT_CARD_NAME_FULL:
    case CREDIT_CARD_NAME_FIRST:
    case CREDIT_CARD_NAME_LAST:
    case CREDIT_CARD_NUMBER:
    case CREDIT_CARD_EXP_MONTH:
    case CREDIT_CARD_EXP_2_DIGIT_YEAR:
    case CREDIT_CARD_EXP_4_DIGIT_YEAR:
    case CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
    case CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR:
    case CREDIT_CARD_TYPE:
    case CREDIT_CARD_VERIFICATION_CODE:
    case CREDIT_CARD_STANDALONE_VERIFICATION_CODE:
    case IBAN_VALUE:
    case MERCHANT_PROMO_CODE:
    case USERNAME:
    case PASSWORD:
    case ACCOUNT_CREATION_PASSWORD:
    case CONFIRMATION_PASSWORD:
    case SINGLE_USERNAME:
    case SINGLE_USERNAME_FORGOT_PASSWORD:
    case SINGLE_USERNAME_WITH_INTERMEDIATE_VALUES:
    case NOT_PASSWORD:
    case NOT_USERNAME:
    case NOT_ACCOUNT_CREATION_PASSWORD:
    case NEW_PASSWORD:
    case PROBABLY_NEW_PASSWORD:
    case NOT_NEW_PASSWORD:
    case ONE_TIME_CODE:
    case NO_SERVER_DATA:
    case EMPTY_TYPE:
    case AMBIGUOUS_TYPE:
    case FIELD_WITH_DEFAULT_VALUE:
    case MERCHANT_EMAIL_SIGNUP:
    case PRICE:
    case NUMERIC_QUANTITY:
    case SEARCH_TERM:
    case IMPROVED_PREDICTION:
    case PASSPORT_NAME_TAG:
    case PASSPORT_NUMBER:
    case PASSPORT_ISSUING_COUNTRY:
    case PASSPORT_EXPIRATION_DATE:
    case PASSPORT_ISSUE_DATE:
    case LOYALTY_MEMBERSHIP_PROGRAM:
    case LOYALTY_MEMBERSHIP_PROVIDER:
    case LOYALTY_MEMBERSHIP_ID:
    case VEHICLE_OWNER_TAG:
    case VEHICLE_LICENSE_PLATE:
    case VEHICLE_VIN:
    case VEHICLE_MAKE:
    case VEHICLE_MODEL:
    case VEHICLE_YEAR:
    case VEHICLE_PLATE_STATE:
    case DRIVERS_LICENSE_NAME_TAG:
    case DRIVERS_LICENSE_REGION:
    case DRIVERS_LICENSE_NUMBER:
    case DRIVERS_LICENSE_EXPIRATION_DATE:
    case DRIVERS_LICENSE_ISSUE_DATE:
    case MAX_VALID_FIELD_TYPE:
    case DELIVERY_INSTRUCTIONS:
    case ADDRESS_HOME_SUBPREMISE:
    case ADDRESS_HOME_OTHER_SUBUNIT:
    case ADDRESS_HOME_ADDRESS_WITH_NAME:
    case ADDRESS_HOME_FLOOR:
    case ADDRESS_HOME_SORTING_CODE:
    case UNKNOWN_TYPE:
    case ADDRESS_HOME_LINE1:
    case ADDRESS_HOME_LINE2:
    case ADDRESS_HOME_LINE3:
    case ADDRESS_HOME_APT_NUM:
    case ADDRESS_HOME_APT:
    case ADDRESS_HOME_APT_TYPE:
    case ADDRESS_HOME_HOUSE_NUMBER_AND_APT:
    case ADDRESS_HOME_CITY:
    case ADDRESS_HOME_STATE:
    case ADDRESS_HOME_ZIP:
    case ADDRESS_HOME_COUNTRY:
    case ADDRESS_HOME_DEPENDENT_LOCALITY:
    case ADDRESS_HOME_STREET_NAME:
    case ADDRESS_HOME_HOUSE_NUMBER:
    case ADDRESS_HOME_STREET_LOCATION:
    case ADDRESS_HOME_LANDMARK:
    case ADDRESS_HOME_BETWEEN_STREETS:
    case ADDRESS_HOME_BETWEEN_STREETS_1:
    case ADDRESS_HOME_BETWEEN_STREETS_2:
    case ADDRESS_HOME_ADMIN_LEVEL2:
    case ADDRESS_HOME_OVERFLOW:
    case ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK:
    case ADDRESS_HOME_OVERFLOW_AND_LANDMARK:
    case COMPANY_NAME:
    case ADDRESS_HOME_STREET_LOCATION_AND_LOCALITY:
    case ADDRESS_HOME_STREET_LOCATION_AND_LANDMARK:
    case ADDRESS_HOME_DEPENDENT_LOCALITY_AND_LANDMARK:
    case EMAIL_OR_LOYALTY_MEMBERSHIP_ID:
      return AddressUIComponentIconType::kNoIcon;
  }
}

}  // namespace autofill
