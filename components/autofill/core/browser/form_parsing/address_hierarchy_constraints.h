// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_ADDRESS_HIERARCHY_CONSTRAINTS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_ADDRESS_HIERARCHY_CONSTRAINTS_H_

#include <iosfwd>

#include "base/containers/flat_map.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

class AddressComponent;

// Stores precalculated supported field types and pairwise co-occurrence
// constraints for the address hierarchy of a specific country to prevent
// repetitive work during form parsing.
class AddressHierarchyConstraints {
 public:
  explicit AddressHierarchyConstraints(AddressCountryCode country_code);
  AddressHierarchyConstraints(const AddressHierarchyConstraints&) = delete;
  AddressHierarchyConstraints& operator=(const AddressHierarchyConstraints&) =
      delete;
  ~AddressHierarchyConstraints();

  // Returns the set of field types supported by the address hierarchy of
  // the country passed in the constructor.
  FieldTypeSet supported_field_types() const { return supported_field_types_; }

  // Returns the set of field types that must not occur if `type` is already
  // assigned to a field:
  // - If a type T was assigned to a field, no ancestor or descendant of T
  //   should be assigned to one of the following fields in the parsing run
  //   because one would contain the other.
  //   E.g. a street name and a street address are incompatible because a
  //   street name is contained in the street address.
  // - Each type T is incompatible with itself (we want to classify only one
  //   field as a type in a single parse run).
  //   E.g. we don't want to classify two postal code fields.
  // - For synthesized field types, we also consider two types T and T2 as
  //   incompatible if T and T2 share any descendants.
  //   E.g. a landmark+street-location is incompatible to a landmark+locality
  //   because they share a landmark.
  // - Structured address details (street name, house number, ...) are
  //   incompatible with address lines.
  // The function must only be called on `FieldType`s that exist in
  // `supported_field_types()`.
  FieldTypeSet incompatible_field_types(FieldType type) const {
    return incompatible_.at(type);
  }

 private:
  void InitializeFieldTypesAndDescendants(
      AddressComponent* node,
      base::flat_map<FieldType, FieldTypeSet>& descendants_and_self);
  void InitializeIncompatibilities(
      const base::flat_map<FieldType, FieldTypeSet>& descendants_and_self,
      bool is_custom_hierarchy_available_for_country);

  // All field types existing in the address model of the specified country.
  FieldTypeSet supported_field_types_;

  // All field types that are incompatible with the key field type meaning that
  // those field types must not be produced by the same execution of
  // `AddressFieldParserNG::Parse()` (e.g. because a ClassifiedFieldSequence
  // should not contain multiple instances of the same field type or because
  // address lines 1, 2, 3 should not co-exist with a landmark; see
  // `incompatible_field_types()`).
  base::flat_map<FieldType, FieldTypeSet> incompatible_;

  friend std::ostream& operator<<(
      std::ostream& os,
      const AddressHierarchyConstraints& constraints);
};

std::ostream& operator<<(std::ostream& os,
                         const AddressHierarchyConstraints& constraints);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_ADDRESS_HIERARCHY_CONSTRAINTS_H_
