// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/address_hierarchy_constraints.h"

#include <ostream>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_i18n_api.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_component.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_component_store.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

namespace {

constexpr FieldType kAddressLines[] = {ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE2,
                                       ADDRESS_HOME_LINE3};
constexpr FieldTypeSet kAddressLinesFieldTypeSet({ADDRESS_HOME_LINE1,
                                                  ADDRESS_HOME_LINE2,
                                                  ADDRESS_HOME_LINE3});

}  // namespace

AddressHierarchyConstraints::AddressHierarchyConstraints(
    AddressCountryCode country_code)
    : is_custom_hierarchy_available_for_country(
          i18n_model_definition::IsCustomHierarchyAvailableForCountry(
              country_code)) {
  AddressComponentsStore model =
      i18n_model_definition::CreateAddressComponentModel(country_code);

  InitializeFieldTypesAndDescendants(model.Root());

  // Address lines 1, 2, 3 are not part of the `model` (they are derived from a
  // ADDRESS_HOME_STREET_ADDRESS). Therefore, they are not considered by
  // `InitializeFieldTypesAndDescendants()`. For the purpose of classifying
  // fields in an address form, they should still be listed in
  // `supported_field_types_` and `descendants_and_self_`.
  for (FieldType child_type : kAddressLines) {
    supported_field_types_.insert(child_type);
    descendants_and_self_[child_type].insert(child_type);
    descendants_and_self_[ADDRESS_HOME_STREET_ADDRESS].insert(child_type);
  }

  // The COMPANY_NAME is not part of the address model but classified by the
  // AddressFieldParser.
  supported_field_types_.insert(COMPANY_NAME);
  descendants_and_self_[COMPANY_NAME].insert(COMPANY_NAME);
  incompatible_[COMPANY_NAME] = FieldTypeSet({COMPANY_NAME});

  // UNKNOWN_TYPE is a non-standard field type that is repurposed for internal
  // logic. It is used to skip certain fields in the classification that may
  // occur at arbitrary locations in the address form but don't belong to the
  // address hierarchy. For example an email field belongs into this category.
  // UNKNOWN_TYPE is only used internally by the `AddressFieldParserNG` and
  // never returned to the caller of the parse function.
  supported_field_types_.insert(UNKNOWN_TYPE);
  descendants_and_self_[UNKNOWN_TYPE].insert(UNKNOWN_TYPE);

  for (FieldType t : supported_field_types_) {
    CHECK(descendants_and_self_.contains(t)) << FieldTypeToStringView(t);
  }

  InitializeIncompatibilities();

  DVLOG(1) << "AddressHierarchyConstraints for " << country_code.value();
  DVLOG(1) << *this;
}

AddressHierarchyConstraints::~AddressHierarchyConstraints() = default;

void AddressHierarchyConstraints::InitializeFieldTypesAndDescendants(
    AddressComponent* node) {
  // This function is a recursive descend through the address model tree for a
  // specific country to collect all field types that occur in the address model
  // in `supported_field_types_` and determine for each node which descendants
  // exist.
  FieldType field_type = node->GetStorageType();

  if (descendants_and_self_.contains(field_type)) {
    return;
  }
  descendants_and_self_[field_type].insert(field_type);
  supported_field_types_.insert(field_type);

  auto InitializeChild = [&](AddressComponent* child) {
    InitializeFieldTypesAndDescendants(child);
    // Invariant: All children have already updated their
    // `descendants_and_self_`.
    // Note: Don't inline the following line because two operator[]() calls lead
    // to undefined behavior. Each one may modify the underlying map and return
    // invalid references.
    FieldTypeSet values_of_child =
        descendants_and_self_[child->GetStorageType()];
    descendants_and_self_[field_type].insert_all(values_of_child);
  };

  for (AddressComponent* child : node->Subcomponents()) {
    InitializeChild(child);
  }

  for (AddressComponent* child : node->SynthesizedSubcomponents()) {
    InitializeChild(child);
  }
}

void AddressHierarchyConstraints::InitializeIncompatibilities() {
  // Each field type may be assigned only once and is therefore incompatible to
  // itself.
  for (FieldType field_type : supported_field_types_) {
    incompatible_[field_type] = FieldTypeSet();
  }

  // The descendants of a ADDRESS_HOME_STREET_ADDRESS in the address model are
  // (except for address lines 1, 2, 3) the structured street address components
  // like street name, house number, etc.

  // If either kAutofillStructuredFieldsDisableAddressLines is enabled or a
  // country is explicitly modeled for the i18n address hierarchy, these
  // structured address components are incompatible with address lines 1, 2, 3.
  // For the legacy model we allow an address line 2 to be paired with a street
  // name and house number.
  const bool autofill_structured_fields_disable_address_lines =
      base::FeatureList::IsEnabled(
          features::kAutofillStructuredFieldsDisableAddressLines) ||
      is_custom_hierarchy_available_for_country;

  // Because address lines 1, 2, 3 are already attached as children of
  // ADDRESS_HOME_STREET_ADDRESS, they need to be removed first to get true set
  // of structured address components.
  FieldTypeSet structured_address_components =
      descendants_and_self_[ADDRESS_HOME_STREET_ADDRESS];
  structured_address_components.erase_all(kAddressLinesFieldTypeSet);
  for (FieldType child_type : structured_address_components) {
    if (autofill_structured_fields_disable_address_lines) {
      for (FieldType address_line : kAddressLines) {
        incompatible_[child_type].insert(address_line);
        incompatible_[address_line].insert(child_type);
      }
    } else {
      if (supported_field_types_.contains(ADDRESS_HOME_STREET_NAME)) {
        incompatible_[ADDRESS_HOME_LINE1].insert(ADDRESS_HOME_STREET_NAME);
        incompatible_[ADDRESS_HOME_STREET_NAME].insert(ADDRESS_HOME_LINE1);
      }
      if (supported_field_types_.contains(ADDRESS_HOME_HOUSE_NUMBER)) {
        incompatible_[ADDRESS_HOME_LINE1].insert(ADDRESS_HOME_HOUSE_NUMBER);
        incompatible_[ADDRESS_HOME_HOUSE_NUMBER].insert(ADDRESS_HOME_LINE1);
      }
    }
  }

  // A field type T is incompatible with its ancestors and descendants. For
  // synthesized nodes it's also possible that a field type T is incompatible
  // with a type T2 that is neither an ancestor nor a descendant but shares
  // some descendants.
  for (FieldType c1 : supported_field_types_) {
    for (FieldType c2 : supported_field_types_) {
      // Comparing the underlying integer values of c1 and c2 is a speed
      // optimization to avoid redundant work that would happen due to symmetry.
      if (std::to_underlying(c1) > std::to_underlying(c2)) {
        continue;
      }
      // Note: Don't inline the following line because two operator[]() calls
      // lead to undefined behavior. Each one may modify the underlying map and
      // return invalid references.
      FieldTypeSet c2_values = descendants_and_self_[c2];
      if (descendants_and_self_[c1].contains_any(c2_values)) {
        incompatible_[c1].insert(c2);
        incompatible_[c2].insert(c1);
      }
    }
  }
}

std::ostream& operator<<(std::ostream& os,
                         const AddressHierarchyConstraints& constraints) {
  os << "AddressHierarchyConstraints::descendants_and_self_:\n";
  for (FieldType child_type : constraints.supported_field_types_) {
    if (!constraints.descendants_and_self_.at(child_type).empty()) {
      os << FieldTypeToStringView(child_type) << ":";
      for (FieldType desc : constraints.descendants_and_self_.at(child_type)) {
        os << " " << FieldTypeToStringView(desc);
      }
      os << "\n";
    }
  }
  os << "AddressHierarchyConstraints::incompatible_:\n";
  for (FieldType child_type : constraints.supported_field_types_) {
    if (!constraints.incompatible_.at(child_type).empty()) {
      os << FieldTypeToStringView(child_type) << ":";
      for (FieldType incompatible : constraints.incompatible_.at(child_type)) {
        os << " " << FieldTypeToStringView(incompatible);
      }
      os << "\n";
    }
  }
  return os;
}

}  // namespace autofill
