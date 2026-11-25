// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/address_hierarchy_constraints.h"

#include <ostream>
#include <sstream>
#include <utility>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_i18n_api.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_component.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_component_store.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

namespace {

constexpr FieldType kAddressLines[] = {ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE2,
                                       ADDRESS_HOME_LINE3};
constexpr FieldTypeSet kAddressLinesFieldTypeSet({ADDRESS_HOME_LINE1,
                                                  ADDRESS_HOME_LINE2,
                                                  ADDRESS_HOME_LINE3});

}  // namespace

AddressHierarchyConstraints::AddressHierarchyConstraints(
    AddressCountryCode country_code) {
  const bool is_custom_hierarchy_available_for_country =
      i18n_model_definition::IsCustomHierarchyAvailableForCountry(country_code);
  AddressComponentsStore model =
      i18n_model_definition::CreateAddressComponentModel(country_code);

  base::flat_map<FieldType, FieldTypeSet> descendants_and_self;
  InitializeFieldTypesAndDescendants(model.Root(), descendants_and_self);

  // Address lines 1, 2, 3 are not part of the `model` (they are derived from a
  // ADDRESS_HOME_STREET_ADDRESS). Therefore, they are not considered by
  // `InitializeFieldTypesAndDescendants()`. For the purpose of classifying
  // fields in an address form, they should still be listed in
  // `supported_field_types_` and `descendants_and_self`.
  for (FieldType child_type : kAddressLines) {
    supported_field_types_.insert(child_type);
    descendants_and_self[child_type].insert(child_type);
    descendants_and_self[ADDRESS_HOME_STREET_ADDRESS].insert(child_type);
  }

  // The COMPANY_NAME is not part of the address model but classified by the
  // AddressFieldParser.
  supported_field_types_.insert(COMPANY_NAME);
  descendants_and_self[COMPANY_NAME].insert(COMPANY_NAME);
  incompatible_[COMPANY_NAME] = FieldTypeSet({COMPANY_NAME});

  // UNKNOWN_TYPE is a non-standard field type that is repurposed for internal
  // logic. It is used to skip certain fields in the classification that may
  // occur at arbitrary locations in the address form but don't belong to the
  // address hierarchy. For example an email field belongs into this category.
  // UNKNOWN_TYPE is only used internally by the `AddressFieldParserNG` and
  // never returned to the caller of the parse function.
  supported_field_types_.insert(UNKNOWN_TYPE);
  descendants_and_self[UNKNOWN_TYPE].insert(UNKNOWN_TYPE);

  for (FieldType t : supported_field_types_) {
    CHECK(descendants_and_self.contains(t)) << FieldTypeToStringView(t);
  }

  InitializeIncompatibilities(descendants_and_self,
                              is_custom_hierarchy_available_for_country);

  DVLOG(1) << "AddressHierarchyConstraints for " << country_code.value();
  DVLOG(1) << [this, &descendants_and_self] {
    std::ostringstream os;
    os << "AddressHierarchyConstraints::descendants_and_self:\n";
    for (FieldType child_type : supported_field_types_) {
      if (!descendants_and_self.at(child_type).empty()) {
        os << FieldTypeToStringView(child_type) << ":";
        for (FieldType desc : descendants_and_self.at(child_type)) {
          os << " " << FieldTypeToStringView(desc);
        }
        os << "\n";
      }
    }
    os << *this;
    return os.str();
  }();
}

AddressHierarchyConstraints::~AddressHierarchyConstraints() = default;

void AddressHierarchyConstraints::InitializeFieldTypesAndDescendants(
    AddressComponent* node,
    base::flat_map<FieldType, FieldTypeSet>& descendants_and_self) {
  // This function is a recursive descend through the address model tree for a
  // specific country to collect all field types that occur in the address model
  // in `supported_field_types_` and determine for each node which descendants
  // exist.
  FieldType field_type = node->GetStorageType();

  if (descendants_and_self.contains(field_type)) {
    return;
  }
  descendants_and_self[field_type].insert(field_type);
  supported_field_types_.insert(field_type);

  auto InitializeChild = [&](AddressComponent* child) {
    InitializeFieldTypesAndDescendants(child, descendants_and_self);
    // Invariant: All children have already updated their
    // `descendants_and_self`.
    // Note: Don't inline the following line because two operator[]() calls lead
    // to undefined behavior. Each one may modify the underlying map and return
    // invalid references.
    FieldTypeSet values_of_child =
        descendants_and_self[child->GetStorageType()];
    descendants_and_self[field_type].insert_all(values_of_child);
  };

  for (AddressComponent* child : node->Subcomponents()) {
    InitializeChild(child);
  }

  for (AddressComponent* child : node->SynthesizedSubcomponents()) {
    InitializeChild(child);
  }
}

void AddressHierarchyConstraints::InitializeIncompatibilities(
    const base::flat_map<FieldType, FieldTypeSet>& descendants_and_self,
    bool is_custom_hierarchy_available_for_country) {
  // Each field type may be assigned only once and is therefore incompatible to
  // itself.
  for (FieldType field_type : supported_field_types_) {
    incompatible_[field_type] = FieldTypeSet();
  }

  // The descendants of a ADDRESS_HOME_STREET_ADDRESS in the address model are
  // (except for address lines 1, 2, 3) the structured street address components
  // like street name, house number, etc.

  // If a country is explicitly modeled for the i18n address hierarchy, these
  // structured address components are incompatible with address lines 1, 2, 3.
  // For the legacy model we allow an address line 2 to be paired with a street
  // name and house number.
  // Because address lines 1, 2, 3 are already attached as children of
  // ADDRESS_HOME_STREET_ADDRESS, they need to be removed first to get true set
  // of structured address components.
  FieldTypeSet structured_address_components =
      descendants_and_self.at(ADDRESS_HOME_STREET_ADDRESS);
  structured_address_components.erase_all(kAddressLinesFieldTypeSet);
  for (FieldType child_type : structured_address_components) {
    if (is_custom_hierarchy_available_for_country) {
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
      if (descendants_and_self.at(c1).contains_any(
              descendants_and_self.at(c2))) {
        incompatible_[c1].insert(c2);
        incompatible_[c2].insert(c1);
      }
    }
  }
}

std::ostream& operator<<(std::ostream& os,
                         const AddressHierarchyConstraints& constraints) {
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
