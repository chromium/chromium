// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_address_util.h"

#include <iterator>
#include <memory>
#include <utility>

#include "autofill_address_util.h"
#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
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

// Returns a vector of AutofillAddressUIComponent for `country_code` when using
// `ui_language_code`. If no components are available for `country_code`, it
// defaults back to the US. If `ui_language_code` is not valid,  the default
// format is returned.
std::vector<AutofillAddressUIComponent> GetAddressComponents(
    const std::string& country_code,
    const std::string& ui_language_code,
    std::string* components_language_code) {
  DCHECK(components_language_code);

  // Return strings in the current application locale.
  Localization localization;
  localization.SetGetter(l10n_util::GetStringUTF8);
  AutofillCountry country(country_code);

  std::vector<AutofillAddressUIComponent> components =
      ConvertAddressUiComponents(
          ::i18n::addressinput::BuildComponentsWithLiterals(
              country_code, localization, ui_language_code,
              components_language_code),
          country);

  if (!components.empty()) {
    ExtendAddressComponents(components, country, localization,
                            /*include_literals=*/true);
    return components;
  } else if (country_code != kAddressComponentsFallbackCountryCode) {
    return GetAddressComponents(kAddressComponentsFallbackCountryCode,
                                ui_language_code, components_language_code);
  }

  NOTREACHED_IN_MIGRATION();
  return {};
}

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
  std::vector<AutofillAddressUIComponent> components;
  components.reserve(addressinput_components.size());

  base::ranges::transform(
      addressinput_components, std::back_inserter(components),
      [&country](const AddressUiComponent& component) {
        // The component's field property may not be initialized if the
        // component is literal, so it should not be used to avoid
        // memory sanitizer's errors (`use-of-uninitialized-value`).
        if (!component.literal.empty()) {
          return AutofillAddressUIComponent{
              .literal = component.literal,
          };
        }
        autofill::FieldType field = i18n::TypeForField(component.field);
        return AutofillAddressUIComponent{
            .field = field,
            .name = component.name,
            .length_hint = ConvertLengthHint(component.length_hint),
            .literal = component.literal,
            .is_required = country.IsAddressFieldRequired(field),
        };
      });

  return components;
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
    auto prev_component = base::ranges::find_if(
        components, [&rule](const AutofillAddressUIComponent& component) {
          return component.literal.empty() &&
                 component.field == rule.placed_after;
        });
    CHECK(prev_component != components.end(), base::NotFatalUntil::M130);

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
  std::vector<AutofillAddressUIComponent> components = GetAddressComponents(
      country_code, ui_language_code,
      components_language_code ? components_language_code : &not_used);
  std::vector<AutofillAddressUIComponent>* line_components = nullptr;
  for (const AutofillAddressUIComponent& component : components) {
    // Start a new line if this is the first line, or a new line literal exists.
    if (!line_components || component.literal == "\n") {
      address_components->push_back(std::vector<AutofillAddressUIComponent>());
      line_components = &address_components->back();
    }

    if (!component.literal.empty()) {
      if (!include_literals)
        continue;
      // No need to return new line literals since components are split into
      // different lines anyway (one line per vector).
      if (component.literal == "\n")
        continue;
    }

    line_components->push_back(component);
  }
  // Filter empty lines. Those can appear e.g. when the line consists only of
  // literals and |include_literals| is false.
  address_components->erase(
      base::ranges::remove_if(*address_components,
                              [](auto line) { return line.empty(); }),
      address_components->end());
}

std::u16string GetEnvelopeStyleAddress(const AutofillProfile& profile,
                                       const std::string& ui_language_code,
                                       bool include_recipient,
                                       bool include_country) {
  const std::u16string& country_code = profile.GetInfo(
      AutofillType(HtmlFieldType::kCountryCode), ui_language_code);

  std::string not_used;
  std::vector<AutofillAddressUIComponent> components = GetAddressComponents(
      base::UTF16ToUTF8(country_code), ui_language_code, &not_used);

  DCHECK(!components.empty());
  std::string address;
  for (const AutofillAddressUIComponent& component : components) {
    // Add string literals directly.
    if (!component.literal.empty()) {
      address += component.literal;
      continue;
    }
    if (!include_recipient && component.field == NAME_FULL) {
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
  static constexpr auto kTypeToCompare = {
      NAME_FULL, ADDRESS_HOME_ADDRESS, EMAIL_ADDRESS, PHONE_HOME_WHOLE_NUMBER};

  base::flat_map<FieldType, std::pair<std::u16string, std::u16string>>
      differences = AutofillProfileComparator::GetProfileDifferenceMap(
          first_profile, second_profile, FieldTypeSet(kTypeToCompare),
          app_locale);

  std::u16string first_address = GetEnvelopeStyleAddress(
      first_profile, app_locale, /*include_recipient=*/false,
      /*include_country=*/true);
  std::u16string second_address = GetEnvelopeStyleAddress(
      second_profile, app_locale, /*include_recipient=*/false,
      /*include_country=*/true);

  std::vector<ProfileValueDifference> differences_for_ui;
  for (FieldType type : kTypeToCompare) {
    // Address is handled seprately.
    if (type == ADDRESS_HOME_ADDRESS) {
      if (first_address != second_address) {
        differences_for_ui.push_back({type, first_address, second_address});
      }
      continue;
    }
    auto it = differences.find(type);
    if (it == differences.end())
      continue;
    differences_for_ui.push_back({type, it->second.first, it->second.second});
  }
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

}  // namespace autofill
