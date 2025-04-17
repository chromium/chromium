// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_ai/core/browser/autofill_ai_utils.h"

#include <ranges>
#include <string>
#include <string_view>
#include <vector>

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
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

// Arbitrary delimiter to user when concatenating labels to decide whether
// a series of labels for different entities are unique.
constexpr char16_t kLabelsDelimiter[] = u" - - ";

}  // namespace

bool IsFormEligibleForFilling(const autofill::FormStructure& form) {
  return std::ranges::any_of(
      form.fields(), [](const std::unique_ptr<AutofillField>& field) {
        return field->GetAutofillAiServerTypePredictions().has_value();
      });
}

// Generates all labels that can be used to disambiguate a list of entities. The
// vector of labels for each entity is sorted from highest to lowest priority.
//
// This function retrieves the available labels by doing the following:
//
// 1. Builds a list of attribute types and values for each entity. Uses
//    `allow_only_disambiguating_types` to maybe skip attribute types that are
//    not part of the entity disambiguating list.
//
// 2. For each entity, sorts the available labels based on their respective
//    attribute type disambiguation order priority.
//
// 3. Counts the occurrences of each attribute type and its value, removing if
//    any combination of these two repeats across all entities.
//
// 4. Go over the attribute types generated in the previous step and add their
// respective value to a final list of labels for each entity. Stops when the
// concatenation of all these labels are unique.
EntitiesLabels GetLabelsForEntities(
    base::span<const EntityInstance*> entity_instances,
    bool allow_only_disambiguating_types,
    bool return_at_least_one_label,
    const std::string& app_locale) {
  if (entity_instances.empty()) {
    return autofill_ai::EntitiesLabels(
        std::vector<std::vector<std::u16string>>());
  }
  // Step 1#
  // Retrieve entities attributes types and values, skipping those in
  // `attribute_types_to_exclude`.
  autofill_ai::AttributesAndValues entities_attributes_and_values;
  for (const EntityInstance* entity : entity_instances) {
    // Retrieve all entity values, this will be used to generate labels.
    std::vector<std::pair<AttributeType, std::u16string>>&
        attribute_types_and_values =
            entities_attributes_and_values.emplace_back();
    for (const AttributeInstance& attribute : entity->attributes()) {
      if (allow_only_disambiguating_types &&
          !attribute.type().is_disambiguation_type()) {
        continue;
      }
      attribute_types_and_values.emplace_back(
          attribute.type(), attribute.GetCompleteInfo(app_locale));
    }
  }

  // If every attribute was excluded, due to `attribute_types_to_exclude`,
  // return early.
  if (entities_attributes_and_values.empty()) {
    return autofill_ai::EntitiesLabels(
        std::vector<std::vector<std::u16string>>());
  }

  // Step 2#
  // Stores for all entities all of its attributes, sorted based on their
  // disambiguation order.
  std::vector<std::vector<std::pair<AttributeType, std::u16string>>>
      attribute_types_and_values_available_for_entities;
  attribute_types_and_values_available_for_entities.reserve(
      entity_instances.size());

  // Used to determine whether a certain attribute and value pair repeats across
  // all entities. In this case, using a label for this value is
  // redundant.
  // This will be used in the step 3# of this method documentation.
  std::map<std::pair<AttributeType, std::u16string>, size_t>
      attribute_type_and_value_occurrences;

  // Go over each entity and its attributes and values.
  for (std::vector<std::pair<AttributeType, std::u16string>>&
           entity_attributes_and_values :
       std::move(entities_attributes_and_values)) {
    for (const auto& [attribute_type, entity_value] :
         entity_attributes_and_values) {
      ++attribute_type_and_value_occurrences[{attribute_type, entity_value}];
    }

    attribute_types_and_values_available_for_entities.push_back(
        std::move(entity_attributes_and_values));
  }

  std::vector<AttributeType> disambiguating_attribute_types;
  autofill::DenseSet<AttributeType> disambiguating_attribute_types_added;

  // Step 3#
  // Now remove the redundant values from
  // `attribute_types_and_values_available_for_entities` and generate the
  // output. A value is considered redundant if it repeats across all
  // entities for the same attribute type.
  for (std::vector<std::pair<AttributeType, std::u16string>>&
           entity_attribute_types_and_values :
       attribute_types_and_values_available_for_entities) {
    for (auto& [attribute_type, value] : entity_attribute_types_and_values) {
      // The label is the same for all entities and has no differentiation
      // value.
      if (attribute_type_and_value_occurrences[{attribute_type, value}] ==
          entity_instances.size()) {
        continue;
      }

      if (disambiguating_attribute_types_added.contains(attribute_type)) {
        continue;
      }
      disambiguating_attribute_types.push_back(attribute_type);
      disambiguating_attribute_types_added.insert(attribute_type);
    }
  }

  std::ranges::sort(disambiguating_attribute_types,
                    AttributeType::DisambiguationOrder);
  if (disambiguating_attribute_types.empty() && return_at_least_one_label) {
    // Take the attribute with highest priority for the entity instance type.
    disambiguating_attribute_types.push_back(
        attribute_types_and_values_available_for_entities[0][0].first);
  }

  autofill_ai::EntitiesLabels entities_labels_output =
      autofill_ai::EntitiesLabels(std::vector<std::vector<std::u16string>>(
          entity_instances.size(), std::vector<std::u16string>()));

  // Step 4#
  // Go over the list of disambiguating attributes and use their values to
  // generate labels for each entity. Stop when the concatenation of labels for
  // each entity is unique.
  size_t max_number_of_labels =
      std::min(autofill_ai::kMaxNumberOfLabels, entities_labels_output->size());
  for (AttributeType attribute_type_to_use_as_label :
       disambiguating_attribute_types) {
    // Used to check whether the list of labels for the entities is unique.
    std::set<std::u16string> current_labels;
    for (size_t i = 0; i < entities_labels_output->size(); i++) {
      const autofill::EntityInstance& entity = *entity_instances[i];
      std::vector<std::u16string>& entity_labels_output =
          (*entities_labels_output)[i];
      if (entity_labels_output.size() == max_number_of_labels) {
        continue;
      }
      base::optional_ref<const AttributeInstance> attribute =
          entity.attribute(attribute_type_to_use_as_label);
      std::u16string label_value =
          attribute ? attribute->GetCompleteInfo(app_locale) : std::u16string();
      if (!label_value.empty()) {
        entity_labels_output.push_back(label_value);
      }

      current_labels.insert(
          base::JoinString(entity_labels_output, kLabelsDelimiter));
    }
    // Label uniqueness was reached if the number of unique labels
    // concatenated strings is same as the entities size, however we do not
    // take empty labels into account.
    const size_t non_empty_labels_count = std::ranges::count_if(
        current_labels,
        [](std::u16string_view label) { return !label.empty(); });
    if (non_empty_labels_count == entity_instances.size()) {
      return entities_labels_output;
    }
  }

  return entities_labels_output;
}

}  // namespace autofill_ai
