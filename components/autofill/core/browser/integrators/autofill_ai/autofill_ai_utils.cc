// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_utils.h"

#include <ranges>
#include <string>
#include <string_view>
#include <vector>

#include "base/strings/string_util.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"

namespace autofill {

namespace {

// Arbitrary delimiter to user when concatenating labels to decide whether
// a series of labels for different entities are unique.
constexpr char16_t kLabelsDelimiter[] = u" - - ";

// Returns the types for which the given entities differ.
//
// The returned types are sorted so that the attributes with the highest
// priority in the disambiguation order come first.
std::vector<AttributeType> GetDisambiguatingTypes(
    base::span<const EntityInstance*> entities,
    bool allow_only_disambiguating_types,
    bool return_at_least_one_label,
    const std::string& app_locale) {
  // The `i`th element holds the attributes and values of the `i`th element of
  // `entities`.
  std::vector<std::vector<std::pair<AttributeType, std::u16string>>>
      attributes_of_all_entities;
  attributes_of_all_entities.reserve(entities.size());

  // Maps attributes and values to the number of times they occur in `entities`.
  // We do not want to include values in the labels that don't differentiate one
  // entity from another.
  std::map<std::pair<AttributeType, std::u16string>, size_t> occurrences;

  // Go over each entity and its attributes and values.
  for (const EntityInstance* entity : entities) {
    attributes_of_all_entities.emplace_back();
    for (const AttributeInstance& attribute : entity->attributes()) {
      if (allow_only_disambiguating_types &&
          !attribute.type().is_disambiguation_type()) {
        continue;
      }

      std::u16string value = attribute.GetCompleteInfo(app_locale);
      ++occurrences[{attribute.type(), value}];
      attributes_of_all_entities.back().emplace_back(attribute.type(),
                                                     std::move(value));
    }
  }

  // The disambiguation types. The `types` vector must not contain duplicates.
  std::vector<AttributeType> types;
  DenseSet<AttributeType> seen;

  // Ignore attributes whose value does not differentiate the entity from any
  // other entity (i.e., all entities have the same value).
  for (std::vector<std::pair<AttributeType, std::u16string>>& attributes :
       attributes_of_all_entities) {
    for (const auto& [attribute_type, value] : attributes) {
      if (occurrences[{attribute_type, value}] == entities.size()) {
        continue;
      }

      if (seen.insert(attribute_type).second) {
        types.push_back(attribute_type);
      }
    }
  }

  // Highest priority first.
  std::ranges::sort(types, AttributeType::DisambiguationOrder);

  if (types.empty() && return_at_least_one_label) {
    // Take the attribute with highest priority for the entity instance type.
    types.push_back(attributes_of_all_entities[0][0].first);
  }
  return types;
}

}  // namespace

bool IsFormEligibleForFilling(const FormStructure& form) {
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
EntitiesLabels GetLabelsForEntities(base::span<const EntityInstance*> entities,
                                    bool allow_only_disambiguating_types,
                                    bool return_at_least_one_label,
                                    const std::string& app_locale) {
  EntitiesLabels entities_labels_output =
      EntitiesLabels(std::vector<std::vector<std::u16string>>(
          entities.size(), std::vector<std::u16string>()));

  // Step 4#
  // Go over the list of disambiguating attributes and use their values to
  // generate labels for each entity. Stop when the concatenation of labels for
  // each entity is unique.
  size_t max_number_of_labels =
      std::min(kMaxNumberOfLabels, entities_labels_output->size());
  for (AttributeType attribute_type_to_use_as_label :
       GetDisambiguatingTypes(entities, allow_only_disambiguating_types,
                              return_at_least_one_label, app_locale)) {
    // Used to check whether the list of labels for the entities is unique.
    std::set<std::u16string> current_labels;
    for (size_t i = 0; i < entities_labels_output->size(); i++) {
      const EntityInstance& entity = *entities[i];
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
    if (non_empty_labels_count == entities.size()) {
      return entities_labels_output;
    }
  }
  return entities_labels_output;
}

}  // namespace autofill
