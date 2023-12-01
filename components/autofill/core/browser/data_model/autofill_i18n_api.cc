// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_i18n_api.h"

#include <string>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_model/autofill_i18n_formatting_expressions.h"
#include "components/autofill/core/browser/data_model/autofill_i18n_hierarchies.h"
#include "components/autofill/core/browser/data_model/autofill_i18n_parsing_expressions.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_format_provider.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_name.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill::i18n_model_definition {

namespace {
using i18n_model_definition::kAutofillFormattingRulesMap;
using i18n_model_definition::kAutofillModelRules;
using i18n_model_definition::kAutofillParsingRulesMap;

// Adjacency mapping, stores for each field type X the list of field types
// which are children of X.
using TreeDefinition =
    base::flat_map<ServerFieldType, base::span<const ServerFieldType>>;

using TreeEdgesList =
    base::span<const autofill::i18n_model_definition::FieldTypeDescription>;

// Address lines are currently the only computed types. These are are shared by
// all countries.
constexpr ServerFieldTypeSet kAddressComputedTypes = {
    ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE2, ADDRESS_HOME_LINE3};

// Returns an instance of the AddressComponent implementation that matches
// the corresponding ServerFieldType if exists. Otherwise, returns a default
// AddressComponent.
std::unique_ptr<AddressComponent> BuildTreeNode(
    autofill::ServerFieldType type,
    std::vector<std::unique_ptr<AddressComponent>> children) {
  switch (type) {
    case ADDRESS_HOME_ADDRESS:
      return std::make_unique<AddressNode>(std::move(children));
    case ADDRESS_HOME_ADMIN_LEVEL2:
      return std::make_unique<AdminLevel2Node>(std::move(children));
    case ADDRESS_HOME_APT_NUM:
      return std::make_unique<ApartmentNode>(std::move(children));
    case ADDRESS_HOME_BETWEEN_STREETS:
      return std::make_unique<BetweenStreetsNode>(std::move(children));
    case ADDRESS_HOME_BETWEEN_STREETS_1:
      return std::make_unique<BetweenStreets1Node>(std::move(children));
    case ADDRESS_HOME_BETWEEN_STREETS_2:
      return std::make_unique<BetweenStreets2Node>(std::move(children));
    case ADDRESS_HOME_CITY:
      return std::make_unique<CityNode>(std::move(children));
    case ADDRESS_HOME_COUNTRY:
      return std::make_unique<CountryCodeNode>(std::move(children));
    case ADDRESS_HOME_DEPENDENT_LOCALITY:
      return std::make_unique<DependentLocalityNode>(std::move(children));
    case ADDRESS_HOME_FLOOR:
      return std::make_unique<FloorNode>(std::move(children));
    case ADDRESS_HOME_HOUSE_NUMBER:
      return std::make_unique<HouseNumberNode>(std::move(children));
    case ADDRESS_HOME_LANDMARK:
      return std::make_unique<LandmarkNode>(std::move(children));
    case ADDRESS_HOME_SORTING_CODE:
      return std::make_unique<SortingCodeNode>(std::move(children));
    case ADDRESS_HOME_STATE:
      return std::make_unique<StateNode>(std::move(children));
    case ADDRESS_HOME_STREET_ADDRESS:
      return std::make_unique<StreetAddressNode>(std::move(children));
    case ADDRESS_HOME_STREET_LOCATION:
      return std::make_unique<StreetLocationNode>(std::move(children));
    case ADDRESS_HOME_STREET_NAME:
      return std::make_unique<StreetNameNode>(std::move(children));
    case ADDRESS_HOME_SUBPREMISE:
      return std::make_unique<SubPremiseNode>(std::move(children));
    case ADDRESS_HOME_ZIP:
      return std::make_unique<PostalCodeNode>(std::move(children));
    case ADDRESS_HOME_OVERFLOW:
      return std::make_unique<AddressOverflowNode>(std::move(children));
    case ADDRESS_HOME_OVERFLOW_AND_LANDMARK:
      return std::make_unique<AddressOverflowAndLandmarkNode>(
          std::move(children));
    case ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK:
      return std::make_unique<BetweenStreetsOrLandmarkNode>(
          std::move(children));
    case ADDRESS_HOME_LINE1:
    case ADDRESS_HOME_LINE2:
    case ADDRESS_HOME_LINE3:
    case ADDRESS_HOME_APT:
    case ADDRESS_HOME_APT_TYPE:
    case ADDRESS_HOME_OTHER_SUBUNIT:
    case ADDRESS_HOME_ADDRESS_WITH_NAME:
    case COMPANY_NAME:
    case DELIVERY_INSTRUCTIONS:
    case NAME_FIRST:
    case NAME_MIDDLE:
    case NAME_LAST:
    case NAME_MIDDLE_INITIAL:
    case NAME_FULL:
    case NAME_SUFFIX:
    case NAME_LAST_FIRST:
    case NAME_LAST_CONJUNCTION:
    case NAME_LAST_SECOND:
    case NAME_HONORIFIC_PREFIX:
    case NAME_FULL_WITH_HONORIFIC_PREFIX:
      return std::make_unique<AddressComponent>(type, std::move(children),
                                                MergeMode::kDefault);
    case NO_SERVER_DATA:
    case UNKNOWN_TYPE:
    case EMPTY_TYPE:
    case EMAIL_ADDRESS:
    case PHONE_HOME_NUMBER:
    case PHONE_HOME_CITY_CODE:
    case PHONE_HOME_COUNTRY_CODE:
    case PHONE_HOME_CITY_AND_NUMBER:
    case PHONE_HOME_WHOLE_NUMBER:
    case CREDIT_CARD_NAME_FULL:
    case CREDIT_CARD_NUMBER:
    case CREDIT_CARD_EXP_MONTH:
    case CREDIT_CARD_EXP_2_DIGIT_YEAR:
    case CREDIT_CARD_EXP_4_DIGIT_YEAR:
    case CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
    case CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR:
    case CREDIT_CARD_TYPE:
    case CREDIT_CARD_VERIFICATION_CODE:
    case FIELD_WITH_DEFAULT_VALUE:
    case MERCHANT_EMAIL_SIGNUP:
    case MERCHANT_PROMO_CODE:
    case PASSWORD:
    case ACCOUNT_CREATION_PASSWORD:
    case NOT_ACCOUNT_CREATION_PASSWORD:
    case USERNAME:
    case USERNAME_AND_EMAIL_ADDRESS:
    case NEW_PASSWORD:
    case PROBABLY_NEW_PASSWORD:
    case NOT_NEW_PASSWORD:
    case CREDIT_CARD_NAME_FIRST:
    case CREDIT_CARD_NAME_LAST:
    case PHONE_HOME_EXTENSION:
    case CONFIRMATION_PASSWORD:
    case AMBIGUOUS_TYPE:
    case SEARCH_TERM:
    case PRICE:
    case NOT_PASSWORD:
    case SINGLE_USERNAME:
    case NOT_USERNAME:
    case BIRTHDATE_DAY:
    case BIRTHDATE_MONTH:
    case BIRTHDATE_4_DIGIT_YEAR:
    case PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX:
    case PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX:
    case PHONE_HOME_NUMBER_PREFIX:
    case PHONE_HOME_NUMBER_SUFFIX:
    case IBAN_VALUE:
    case CREDIT_CARD_STANDALONE_VERIFICATION_CODE:
    case NUMERIC_QUANTITY:
    case ONE_TIME_CODE:
    case SINGLE_USERNAME_FORGOT_PASSWORD:
    case SINGLE_USERNAME_WITH_INTERMEDIATE_VALUES:
    case MAX_VALID_FIELD_TYPE:
      return nullptr;
  }
  NOTREACHED_NORETURN();
}

std::unique_ptr<AddressComponent> BuildSubTree(const TreeDefinition& tree_def,
                                               ServerFieldType root) {
  std::vector<std::unique_ptr<AddressComponent>> children;
  // Leaf nodes do not have an entry in the tree_def.
  if (tree_def.contains(root)) {
    children.reserve(tree_def.at(root).size());
    for (ServerFieldType child_type : tree_def.at(root)) {
      children.push_back(BuildSubTree(tree_def, child_type));
    }
  }
  return BuildTreeNode(root, std::move(children));
}

TreeEdgesList GetTreeEdges(AddressCountryCode country_code) {
  // Always use legacy rules while `kAutofillUseI18nAddressModel` is not rolled
  // out.
  if (!base::FeatureList::IsEnabled(features::kAutofillUseI18nAddressModel)) {
    return kAutofillModelRules.find(kLegacyHierarchyCountryCode.value())
        ->second;
  }

  auto* it = kAutofillModelRules.find(country_code.value());

  // If the entry is not defined, use the legacy rules.
  return it == kAutofillModelRules.end()
             ? kAutofillModelRules.find(kLegacyHierarchyCountryCode.value())
                   ->second
             : it->second;
}

}  // namespace

std::unique_ptr<AddressComponent> CreateAddressComponentModel(
    AddressCountryCode country_code) {
  TreeEdgesList tree_edges = GetTreeEdges(country_code);

  // Convert the list of node properties into an adjacency lookup table.
  // For each field type it stores the list of children of the field type.
  TreeDefinition tree_def =
      base::MakeFlatMap<ServerFieldType, base::span<const ServerFieldType>>(
          tree_edges, {}, [](const auto& item) {
            return std::make_pair(item.field_type, item.children);
          });

  auto result = BuildSubTree(tree_def, ADDRESS_HOME_ADDRESS);

  if (!country_code->empty() && country_code != kLegacyHierarchyCountryCode) {
    // Set the address model country to the one requested.
    result->SetValueForType(ADDRESS_HOME_COUNTRY,
                            base::UTF8ToUTF16(country_code.value()),
                            VerificationStatus::kObserved);
  }
  return result;
}

std::u16string GetFormattingExpression(ServerFieldType field_type,
                                       AddressCountryCode country_code) {
  if (base::FeatureList::IsEnabled(features::kAutofillUseI18nAddressModel) &&
      GroupTypeOfServerFieldType(field_type) == FieldTypeGroup::kAddress) {
    // If `country_code` is specified, return the corresponding formatting
    // expression if they exist. Note that it should not fallback to a legacy
    // expression, as these ones refer to a different hierarchy.
    if (IsCustomHierarchyAvailableForCountry(country_code)) {
      auto* it =
          kAutofillFormattingRulesMap.find({country_code.value(), field_type});

      return it != kAutofillFormattingRulesMap.end()
                 ? std::u16string(it->second)
                 : u"";
    }

    // Otherwise return a legacy formatting expression that exists.
    auto* legacy_it = kAutofillFormattingRulesMap.find(
        {kLegacyHierarchyCountryCode.value(), field_type});
    return legacy_it != kAutofillFormattingRulesMap.end()
               ? std::u16string(legacy_it->second)
               : u"";
  }

  auto* pattern_provider = StructuredAddressesFormatProvider::GetInstance();
  CHECK(pattern_provider);
  return pattern_provider->GetPattern(field_type, country_code.value());
}

i18n_model_definition::ValueParsingResults ParseValueByI18nRegularExpression(
    std::string_view value,
    ServerFieldType field_type,
    AddressCountryCode country_code) {
  CHECK(GroupTypeOfServerFieldType(field_type) == FieldTypeGroup::kAddress);
  // If `country_code` is specified, attempt to parse the `value` using a
  // custom parsing structure (if exist).
  // Otherwise try using a legacy parsing expression (if exist).
  AddressCountryCode country_code_for_parsing =
      IsCustomHierarchyAvailableForCountry(country_code)
          ? country_code
          : kLegacyHierarchyCountryCode;

  auto* it = kAutofillParsingRulesMap.find(
      {country_code_for_parsing.value(), field_type});
  return it != kAutofillParsingRulesMap.end() ? it->second->Parse(value)
                                              : absl::nullopt;
}

bool IsTypeEnabledForCountry(ServerFieldType field_type,
                             AddressCountryCode country_code) {
  auto* it = kAutofillModelRules.find(country_code.value());
  if (it == kAutofillModelRules.end()) {
    return false;
  }

  if (kAddressComputedTypes.contains(field_type)) {
    return true;
  }

  return base::ranges::any_of(
      it->second, [field_type](const FieldTypeDescription& description) {
        return description.field_type == field_type ||
               base::Contains(description.children, field_type);
      });
}

bool IsCustomHierarchyAvailableForCountry(AddressCountryCode country_code) {
  if (country_code->empty() || country_code == kLegacyHierarchyCountryCode ||
      !base::FeatureList::IsEnabled(features::kAutofillUseI18nAddressModel)) {
    return false;
  }
  return kAutofillModelRules.find(country_code.value()) !=
         kAutofillModelRules.end();
}

}  // namespace autofill::i18n_model_definition
