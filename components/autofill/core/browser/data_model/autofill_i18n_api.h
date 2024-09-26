// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_I18N_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_I18N_API_H_

#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/autofill_i18n_hierarchies.h"
#include "components/autofill/core/browser/data_model/autofill_i18n_parsing_expression_components.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_component.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_component_store.h"

namespace autofill::i18n_model_definition {

// Country code that represents autofill's legacy address hierarchy model as
// stored `kAutofillModelRules`. As a workaround for GCC we declare the
// std::string constexpr first.
constexpr inline std::string kLegacyHierarchyCountryCodeString{"XX"};
constexpr AddressCountryCode kLegacyHierarchyCountryCode =
    AddressCountryCode(kLegacyHierarchyCountryCodeString);

// Creates an instance of the address hierarchy model corresponding to the
// provided country. All the hierarchy nodes are contained within the store.
// The hierarchy is accessible through its root (e.g. ADDRESS_HOME_ADDRESS). All
// the nodes have empty values, except for the country node (if exist). If no
// country is provided, returns the legacy address hierarchy.
AddressComponentsStore CreateAddressComponentModel(
    AddressCountryCode country_code = AddressCountryCode(""));

// Returns whether or not the `field_type` is a synthesized type on the address
// model for `country_code`.
// A synthesized type, is a type whose value is calculated from other address
// types (its constituents). While synthesized types are not directly part of
// the address hierarchy tree, its constituents are. Synthesized types cannot be
// stored, learned from form submissions, or viewed in settings.
bool IsSynthesizedType(FieldType field_type, AddressCountryCode country_code);

// Returns the formatting expression corresponding to the provided parameters.
// If the expression can't be found or the country is empty, it attempts to look
// for a legacy expression. Returns an empty string if none can be found.
std::u16string GetFormattingExpression(FieldType field_type,
                                       AddressCountryCode country_code);

// Parses the given `value` using a custom parsing process (if available) for
// the corresponding `field_type` and `country_code`. If the country is empty or
// there is no expression available, it attempts to parse the value using one of
// the legacy regular expressions. If no matching is found, `nullopt` is
// returned.
i18n_model_definition::ValueParsingResults ParseValueByI18nRegularExpression(
    std::string_view value,
    FieldType field_type,
    AddressCountryCode country_code);

// The function returns true if the provided `field_type` is included in the
// hierarchy model of the given country. Otherwise it returns false.
bool IsTypeEnabledForCountry(FieldType field_type,
                             AddressCountryCode country_code);

// Returns whether there is a custom address hierarchy available for the
// provided `country_code`.
bool IsCustomHierarchyAvailableForCountry(AddressCountryCode country_code);
}  // namespace autofill::i18n_model_definition

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_I18N_API_H_
