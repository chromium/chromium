// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_ai/core/browser/autofill_ai_utils.h"

#include <ranges>
#include <string>
#include <vector>

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"

namespace autofill_ai {

namespace {

using autofill::AttributeInstance;
using autofill::AttributeType;
using autofill::AutofillField;
using autofill::EntityInstance;
using autofill::FieldTypeGroup;

// For a list of entities, this defines all attributes for each entity, together
// with their value.
using AttributesAndValues = std::vector<
    std::vector<std::pair<autofill::AttributeType, std::u16string>>>;

}  // namespace

bool IsFormEligibleForFilling(const autofill::FormStructure& form) {
  return std::ranges::any_of(
      form.fields(), [](const std::unique_ptr<AutofillField>& field) {
        return field->GetAutofillAiServerTypePredictions().has_value();
      });
}

// Generates all labels that can be used to disambiguate a list of entities. The
// vector of labels for each entity is sorted from lowest to highest priority.
//
// This function retrieves the available labels by doing the following:
//
// 1. Builds a list of attribute types and values for each entity. Removing any
//    type include in `attribute_types_to_exclude`
//
// 2. For each entity, sorts the available labels based on their respective
//    attribute type disambiguation order priority.
//
// 3. Counts the occurrences of each attribute type and its value, removing if
//    any combination of these two repeats across all entities.
EntitiesLabels GetLabelsForEntities(
    base::span<const EntityInstance*> entity_instances,
    const autofill::DenseSet<AttributeType>& attribute_types_to_exclude,
    const std::string& app_locale) {
  const size_t n_entities = entity_instances.size();

  // Step 1#
  // Retrieve entities values and skip those in `attribute_types_to_exclude`.
  autofill_ai::AttributesAndValues entities_attributes_and_values;
  for (const EntityInstance* entity : entity_instances) {
    // Retrieve all entity values, this will be used to generate labels.
    std::vector<std::pair<AttributeType, std::u16string>>&
        attribute_types_and_values =
            entities_attributes_and_values.emplace_back();
    for (const AttributeInstance& attribute : entity->attributes()) {
      if (attribute_types_to_exclude.contains(attribute.type())) {
        continue;
      }
      std::u16string full_attribute_value =
          attribute.GetCompleteInfo(app_locale);
      if (full_attribute_value.empty()) {
        continue;
      }
      attribute_types_and_values.emplace_back(attribute.type(),
                                              std::move(full_attribute_value));
    }
  }

  // Step 2#
  // Stores for all entities all of its attributes, sorted based on their
  // disambiguation order.
  std::vector<std::vector<std::pair<AttributeType, std::u16string>>>
      attribute_types_and_values_available_for_entities;
  attribute_types_and_values_available_for_entities.reserve(n_entities);

  // Used to determine whether a certain attribute and value pair repeats across
  // all entities. In this case, using a label for this value is
  // redundant.
  // This will be used in the step 2# of this method documentation.
  std::map<std::pair<AttributeType, std::u16string>, size_t>
      attribute_type_and_value_occurrences;

  // Go over each entity and its attributes and values.
  for (std::vector<std::pair<AttributeType, std::u16string>>&
           entity_attributes_and_values :
       std::move(entities_attributes_and_values)) {
    std::ranges::sort(entity_attributes_and_values,
                      std::not_fn(AttributeType::DisambiguationOrder),
                      &std::pair<AttributeType, std::u16string>::first);

    for (const auto& [attribute_type, entity_value] :
         entity_attributes_and_values) {
      ++attribute_type_and_value_occurrences[{attribute_type, entity_value}];
    }

    attribute_types_and_values_available_for_entities.push_back(
        std::move(entity_attributes_and_values));
  }

  // The output of this method.
  EntitiesLabels labels_available_for_entities;
  labels_available_for_entities->reserve(n_entities);

  // Step 3#
  // Now remove the redundant values from
  // `attribute_types_and_values_available_for_entities` and generate the
  // output. A value is considered redundant if it repeats across all
  // entities for the same attribute type.
  for (std::vector<std::pair<AttributeType, std::u16string>>&
           entity_attribute_types_and_values :
       attribute_types_and_values_available_for_entities) {
    std::vector<std::u16string> labels_for_entities;
    for (auto& [attribute_type, value] : entity_attribute_types_and_values) {
      // The label is the same for all entities and has no differentiation
      // value.
      if (attribute_type_and_value_occurrences[{attribute_type, value}] ==
          n_entities) {
        continue;
      }
      labels_for_entities.push_back(std::move(value));
    }
    // At least one label should exist, even if it repeats in other suggestions.
    // This is because labels also have descriptive value.
    if (labels_for_entities.empty() &&
        !entity_attribute_types_and_values.empty()) {
      // Take the last value because it is the one with highest priority.
      labels_for_entities.push_back(
          entity_attribute_types_and_values.back().second);
    }
    labels_available_for_entities->push_back(std::move(labels_for_entities));
  }

  return labels_available_for_entities;
}

}  // namespace autofill_ai
