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

// Checks whether `fields` matches `entity_of_interst`'s required fields,
// returns an empty vector if not.
std::vector<AutofillFieldWithAttributeType> RationalizeAttributeTypes(
    std::vector<AutofillFieldWithAttributeType> fields,
    EntityType entity_of_interest) {
  bool attributes_satisfy_required_fields = [&]() {
    DenseSet<AttributeType> types =
        DenseSet<AttributeType>(fields, &AutofillFieldWithAttributeType::type);
    return std::ranges::any_of(entity_of_interest.required_fields(),
                               [&](DenseSet<AttributeType> constraint) {
                                 return types.contains_all(constraint);
                               });
  }();
  if (attributes_satisfy_required_fields) {
    return fields;
  }
  return std::vector<AutofillFieldWithAttributeType>();
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
