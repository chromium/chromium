// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_ai_form_rationalization.h"

#include <vector>

#include "base/containers/span.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/form_processing/autofill_ai/determine_attribute_types.h"

namespace autofill {

namespace {

// Checks whether `fields` matches `entity_of_interest`'s required fields,
// returns an empty vector if not.
void EnforceRequiredFieldsConstraints(
    std::vector<AutofillFieldWithAttributeType>& fields,
    EntityType entity_of_interest) {
  DenseSet<AttributeType> types =
      DenseSet<AttributeType>(fields, &AutofillFieldWithAttributeType::type);
  bool attributes_satisfy_required_fields =
      std::ranges::any_of(entity_of_interest.required_fields(),
                          [&](DenseSet<AttributeType> constraint) {
                            return types.contains_all(constraint);
                          });
  if (!attributes_satisfy_required_fields) {
    fields.clear();
  }
}

// Looks for adjacent kVehicleLicensePlate fields. Since we don't have
// heuristics to split license plates reliably, we do not want to fill them.
void RemoveAdjacentLicensePlateNumbers(
    std::vector<AutofillFieldWithAttributeType>& fields,
    EntityType entity_of_interest) {
  if (entity_of_interest != EntityType(EntityTypeName::kVehicle)) {
    return;
  }

  // The rank is the field's index in the form. Since `fields` only contains
  // fields that belong to `entity_of_interest` and they appear in the same
  // order as in the form, their std::distance() in `fields` is less than or
  // equal to their rank_distance().
  auto rank_distance = [](auto begin, auto end) {
    CHECK(begin != end);
    ptrdiff_t lo = begin->field->rank();
    ptrdiff_t hi = std::prev(end)->field->rank();
    return hi - lo + 1;
  };

  // Erases all subranges where all fields `pred`, provided that these subranges
  // have more than one field and the ranks of the fields are reasonably close
  // (i.e., there aren't too many fields in between those).
  auto pred = [](const AutofillFieldWithAttributeType& f) {
    return f.type == AttributeType(AttributeTypeName::kVehiclePlateNumber);
  };
  auto begin = fields.begin();
  auto end = fields.begin();
  while ((begin = std::find_if(end, fields.end(), pred)) != fields.end()) {
    end = std::find_if_not(std::next(begin), fields.end(), pred);
    if (std::distance(begin, end) > 1 &&
        std::distance(begin, end) * 3 / 2 >= rank_distance(begin, end)) {
      end = fields.erase(begin, end);
    }
  }
}

std::vector<AutofillFieldWithAttributeType> RationalizeAttributeTypes(
    std::vector<AutofillFieldWithAttributeType> fields,
    EntityType entity_of_interest) {
  EnforceRequiredFieldsConstraints(fields, entity_of_interest);
  RemoveAdjacentLicensePlateNumbers(fields, entity_of_interest);
  return fields;
}

}  // namespace

std::vector<AutofillFieldWithAttributeType>
RationalizeAndDetermineAttributeTypes(
    base::span<const std::unique_ptr<AutofillField>> fields LIFETIME_BOUND,
    const Section& section_of_interest,
    EntityType entity_of_interest) {
  return RationalizeAttributeTypes(
      DetermineAttributeTypes(fields, section_of_interest, entity_of_interest,
                              DetermineAttributeTypesPassKey()),
      entity_of_interest);
}

base::flat_map<EntityType, std::vector<AutofillFieldWithAttributeType>>
RationalizeAndDetermineAttributeTypes(
    base::span<const std::unique_ptr<AutofillField>> fields LIFETIME_BOUND,
    const Section& section_of_interest) {
  base::flat_map<EntityType, std::vector<AutofillFieldWithAttributeType>>
      entity_map = DetermineAttributeTypes(fields, section_of_interest,
                                           DetermineAttributeTypesPassKey());
  for (auto& [entity_type, autofill_fields_with_attribute] : entity_map) {
    entity_map[entity_type] =
        RationalizeAttributeTypes(autofill_fields_with_attribute, entity_type);
  }
  return entity_map;
}

base::flat_map<
    Section,
    base::flat_map<EntityType, std::vector<AutofillFieldWithAttributeType>>>
RationalizeAndDetermineAttributeTypes(
    base::span<const std::unique_ptr<AutofillField>> fields LIFETIME_BOUND) {
  base::flat_map<
      Section,
      base::flat_map<EntityType, std::vector<AutofillFieldWithAttributeType>>>
      section_map =
          DetermineAttributeTypes(fields, DetermineAttributeTypesPassKey());

  for (auto& [section, entity_map] : section_map) {
    for (auto& [entity_type, autofill_field_with_attribute_types] :
         entity_map) {
      entity_map[entity_type] = RationalizeAttributeTypes(
          autofill_field_with_attribute_types, entity_type);
    }
  }
  return section_map;
}

std::vector<AutofillFieldWithAttributeType>
RationalizeAttributeTypesForTesting(  // IN-TEST
    std::vector<AutofillFieldWithAttributeType> fields,
    EntityType entity_of_interest) {
  return RationalizeAttributeTypes(fields, entity_of_interest);
}

DenseSet<EntityType> GetRelevantEntityTypesForFields(
    base::span<const std::unique_ptr<AutofillField>> fields) {
  DenseSet<EntityType> entity_types;
  for (const auto& [section, entity_map] :
       RationalizeAndDetermineAttributeTypes(fields)) {
    for (const auto& [entity_type, autofill_field_with_attribute_types] :
         entity_map) {
      if (!autofill_field_with_attribute_types.empty()) {
        entity_types.insert(entity_type);
      }
    }
  }
  return entity_types;
}

}  // namespace autofill
