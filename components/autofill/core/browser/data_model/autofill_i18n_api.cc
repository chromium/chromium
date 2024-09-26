// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_i18n_api.h"

#include <memory>
#include <string>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/autofill_i18n_formatting_expressions.h"
#include "components/autofill/core/browser/data_model/autofill_i18n_hierarchies.h"
#include "components/autofill/core/browser/data_model/autofill_i18n_parsing_expressions.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_component.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_format_provider.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_name.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/data_model/autofill_synthesized_address_component.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill::i18n_model_definition {

namespace {
using i18n_model_definition::kAutofillFormattingRulesMap;
using i18n_model_definition::kAutofillModelRules;
using i18n_model_definition::kAutofillParsingRulesMap;

// Adjacency mapping, stores for each field type X the list of field types
// which are children of X.
using TreeDefinition = base::flat_map<FieldType, base::span<const FieldType>>;

using TreeEdgesList =
    base::span<const autofill::i18n_model_definition::FieldTypeDescription>;

// Address lines are currently the only computed types. These are are shared by
// all countries.
constexpr FieldTypeSet kAddressComputedTypes = {
    ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE2, ADDRESS_HOME_LINE3};

std::u16string GetFormattingExpressionOverrides(
    FieldType field_type,
    AddressCountryCode country_code) {
  // The list of countries for which the street location is composed of the
  // house number followed by the street name. The default value returned by the
  // formatting API is the opposite (i.e. street name followed by house number).
  static constexpr auto kHouseNumberFirstCountriesSet =
      base::MakeFixedFlatSet<std::string_view>(
          {"AU", "CA", "CN", "FR", "IE", "IL", "MY", "NZ", "PK", "PH", "SA",
           "SG", "LK", "TH", "GB", "US", "VN", "ZA"});

  if (field_type == ADDRESS_HOME_STREET_LOCATION) {
    if (base::Contains(kHouseNumberFirstCountriesSet, country_code.value())) {
      return u"${ADDRESS_HOME_HOUSE_NUMBER;;} ${ADDRESS_HOME_STREET_NAME;;}";
    }
  }

  if (field_type == ADDRESS_HOME_STREET_ADDRESS &&
      country_code.value() == "ES") {
    // TODO(crbug.com/40275657): Remove once an address model for Spain is
    // introduced.
    return u"${ADDRESS_HOME_STREET_NAME} ${ADDRESS_HOME_HOUSE_NUMBER}"
           u"${ADDRESS_HOME_FLOOR;, ;º}${ADDRESS_HOME_APT_NUM;, ;ª}";
  }

  return u"";
}

// Returns an instance of the `AddressComponent` implementation that matches
// the corresponding FieldType if exists. Otherwise, returns a default
// `AddressComponent`.
// Note that nodes do not own their children, rather pointers to them. All
// `AddressComponent` nodes are owned by the `AddressComponentsStore`.
std::unique_ptr<AddressComponent> BuildTreeNode(
    autofill::FieldType type,
    std::vector<AddressComponent*> children) {
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
    case ADDRESS_HOME_HOUSE_NUMBER_AND_APT:
    case ADDRESS_HOME_OTHER_SUBUNIT:
    case ADDRESS_HOME_ADDRESS_WITH_NAME:
    case ADDRESS_HOME_STREET_LOCATION_AND_LOCALITY:
    case ADDRESS_HOME_STREET_LOCATION_AND_LANDMARK:
    case ADDRESS_HOME_DEPENDENT_LOCALITY_AND_LANDMARK:
    case DELIVERY_INSTRUCTIONS:
      return std::make_unique<AddressComponent>(type, std::move(children),
                                                MergeMode::kDefault);
    case NO_SERVER_DATA:
    case UNKNOWN_TYPE:
    case EMPTY_TYPE:
    case EMAIL_ADDRESS:
    case COMPANY_NAME:
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
    case IMPROVED_PREDICTION:
    case MAX_VALID_FIELD_TYPE:
      return nullptr;
  }
  NOTREACHED();
}

std::unique_ptr<SynthesizedAddressComponent> BuildSynthesizedNode(
    FieldType type,
    const TreeDefinition& tree_def,
    const base::flat_map<FieldType, std::unique_ptr<AddressComponent>>&
        nodes_registry) {
  std::vector<AddressComponent*> children;
  children.reserve(tree_def.at(type).size());
  for (FieldType child_type : tree_def.at(type)) {
    children.push_back(nodes_registry.at(child_type).get());
  }
  return std::make_unique<SynthesizedAddressComponent>(
      type, std::move(children), MergeMode::kDefault);
}

AddressComponent* BuildSubTree(
    const TreeDefinition& tree_def,
    FieldType root,
    AddressCountryCode country_code,
    base::flat_map<FieldType, std::unique_ptr<AddressComponent>>&
        nodes_registry) {
  // Registers `node` in the nodes registry.
  auto RegisterNode =
      [&nodes_registry](std::unique_ptr<AddressComponent> node) {
        auto [it, inserted] =
            nodes_registry.emplace(node->GetStorageType(), std::move(node));
        CHECK(inserted);
        return it->second.get();
      };

  // Leaf nodes do not have an entry in the `tree_def`. By definition
  // they cannot have children nor be synthesized nodes.
  if (!tree_def.contains(root)) {
    return RegisterNode(BuildTreeNode(root, /*children=*/{}));
  }

  std::vector<AddressComponent*> children;
  children.reserve(tree_def.at(root).size());
  for (FieldType child_type : tree_def.at(root)) {
    if (!IsSynthesizedType(child_type, country_code)) {
      children.push_back(
          BuildSubTree(tree_def, child_type, country_code, nodes_registry));
    }
  }

  std::unique_ptr<AddressComponent> node =
      BuildTreeNode(root, std::move(children));

  // Synthesized nodes are owned by the lowest common ancestor of their
  // constituents. That means that at this point, all their constituents have
  // been built and stored in the nodes registry.
  for (FieldType child_type : tree_def.at(root)) {
    if (IsSynthesizedType(child_type, country_code)) {
      AddressComponent* synthesized_node = RegisterNode(
          BuildSynthesizedNode(child_type, tree_def, nodes_registry));
      node->RegisterSynthesizedSubcomponent(synthesized_node);
    }
  }

  return RegisterNode(std::move(node));
}

TreeEdgesList GetTreeEdges(AddressCountryCode country_code) {
  // Always use legacy rules if the country has no available custom address
  // model.
  if (!IsCustomHierarchyAvailableForCountry(country_code)) {
    return kAutofillModelRules.find(kLegacyHierarchyCountryCode.value())
        ->second;
  }

  auto it = kAutofillModelRules.find(country_code.value());

  // If the entry is not defined, use the legacy rules.
  return it == kAutofillModelRules.end()
             ? kAutofillModelRules.find(kLegacyHierarchyCountryCode.value())
                   ->second
             : it->second;
}

}  // namespace

AddressComponentsStore CreateAddressComponentModel(
    AddressCountryCode country_code) {
  TreeEdgesList tree_edges = GetTreeEdges(country_code);

  // Convert the list of node properties into an adjacency lookup table.
  // For each field type it stores the list of children of the field type.
  TreeDefinition tree_def =
      base::MakeFlatMap<FieldType, base::span<const FieldType>>(
          tree_edges, {}, [](const auto& item) {
            return std::make_pair(item.field_type, item.children);
          });

  base::flat_map<FieldType, std::unique_ptr<AddressComponent>> components;
  AddressComponent* root =
      BuildSubTree(tree_def, ADDRESS_HOME_ADDRESS, country_code, components);

  if (!country_code->empty() && country_code != kLegacyHierarchyCountryCode) {
    // Set the address model country to the one requested.
    root->SetValueForType(ADDRESS_HOME_COUNTRY,
                          base::UTF8ToUTF16(country_code.value()),
                          VerificationStatus::kObserved);
  }
  return AddressComponentsStore(std::move(components));
}

bool IsSynthesizedType(FieldType field_type, AddressCountryCode country_code) {
  return kAutofillSynthesizeNodes.contains(
      {IsCustomHierarchyAvailableForCountry(country_code)
           ? country_code.value()
           : kLegacyHierarchyCountryCode.value(),
       field_type});
}

std::u16string GetFormattingExpression(FieldType field_type,
                                       AddressCountryCode country_code) {
  if (GroupTypeOfFieldType(field_type) == FieldTypeGroup::kAddress) {
    // If `country_code` is specified, return the corresponding formatting
    // expression if they exist. Note that it should not fallback to a legacy
    // expression, as these ones refer to a different hierarchy.
    if (IsCustomHierarchyAvailableForCountry(country_code)) {
      auto it =
          kAutofillFormattingRulesMap.find({country_code.value(), field_type});

      return it != kAutofillFormattingRulesMap.end()
                 ? std::u16string(it->second)
                 : u"";
    }

    if (std::u16string format_override =
            GetFormattingExpressionOverrides(field_type, country_code);
        !format_override.empty()) {
      return format_override;
    }
    // Otherwise return a legacy formatting expression that exists.
    auto legacy_it = kAutofillFormattingRulesMap.find(
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
    FieldType field_type,
    AddressCountryCode country_code) {
  CHECK(GroupTypeOfFieldType(field_type) == FieldTypeGroup::kAddress);
  // If `country_code` is specified, attempt to parse the `value` using a
  // custom parsing structure (if exist).
  // Otherwise try using a legacy parsing expression (if exist).
  AddressCountryCode country_code_for_parsing =
      IsCustomHierarchyAvailableForCountry(country_code)
          ? country_code
          : kLegacyHierarchyCountryCode;

  auto it = kAutofillParsingRulesMap.find(
      {country_code_for_parsing.value(), field_type});
  return it != kAutofillParsingRulesMap.end() ? it->second->Parse(value)
                                              : std::nullopt;
}

bool IsTypeEnabledForCountry(FieldType field_type,
                             AddressCountryCode country_code) {
  if (!IsCustomHierarchyAvailableForCountry(country_code)) {
    country_code = kLegacyHierarchyCountryCode;
  }

  if (kAddressComputedTypes.contains(field_type)) {
    return true;
  }

  auto it = kAutofillModelRules.find(country_code.value());
  return std::ranges::any_of(
      it->second, [field_type](const FieldTypeDescription& description) {
        return description.field_type == field_type ||
               base::Contains(description.children, field_type);
      });
}

bool IsCustomHierarchyAvailableForCountry(AddressCountryCode country_code) {
  if (country_code->empty() || country_code == kLegacyHierarchyCountryCode) {
    return false;
  }

  if (country_code == AddressCountryCode("AU") &&
      !base::FeatureList::IsEnabled(features::kAutofillUseAUAddressModel)) {
    return false;
  }

  if (country_code == AddressCountryCode("CA") &&
      !base::FeatureList::IsEnabled(features::kAutofillUseCAAddressModel)) {
    return false;
  }

  if (country_code == AddressCountryCode("DE") &&
      !base::FeatureList::IsEnabled(features::kAutofillUseDEAddressModel)) {
    return false;
  }

  if (country_code == AddressCountryCode("FR") &&
      !base::FeatureList::IsEnabled(features::kAutofillUseFRAddressModel)) {
    return false;
  }

  if (country_code == AddressCountryCode("IN") &&
      !base::FeatureList::IsEnabled(features::kAutofillUseINAddressModel)) {
    return false;
  }

  if (country_code == AddressCountryCode("IT") &&
      !base::FeatureList::IsEnabled(features::kAutofillUseITAddressModel)) {
    return false;
  }

  if (country_code == AddressCountryCode("PL") &&
      !base::FeatureList::IsEnabled(features::kAutofillUsePLAddressModel)) {
    return false;
  }

  return kAutofillModelRules.find(country_code.value()) !=
         kAutofillModelRules.end();
}

}  // namespace autofill::i18n_model_definition
