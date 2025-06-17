// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_utils.h"

#include <ranges>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/to_vector.h"
#include "base/strings/string_util.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"

namespace autofill {

namespace {

// Returns the types for which the given entities differ.
//
// The returned types are sorted so that the attributes with the highest
// priority in the disambiguation order come first.
std::vector<AttributeType> GetDisambiguatingTypes(
    base::span<const EntityInstance*> entities,
    bool allow_only_disambiguating_types,
    bool return_at_least_one_label,
    const std::string& app_locale) {
  // Only relevant AttributeTypes are considered for disambiguation.
  auto is_relevant = [&](AttributeType type) {
    return !allow_only_disambiguating_types || type.is_disambiguation_type();
  };

  auto get_info = [&app_locale](const EntityInstance& entity,
                                AttributeType type) {
    base::optional_ref<const AttributeInstance> attribute =
        entity.attribute(type);
    return attribute ? std::optional(attribute->GetCompleteInfo(app_locale))
                     : std::nullopt;
  };

  // An AttributeType is disambiguating if two entities disagree on its value.
  // Ignores entities unrelated to the AttributeType.
  auto is_disambiguating = [&entities, &get_info](AttributeType type) {
    std::optional<std::optional<std::u16string>> seen_value;
    for (const EntityInstance* entity : entities) {
      if (!entity->type().attributes().contains(type)) {
        continue;
      }
      std::optional<std::u16string> value = get_info(*entity, type);
      if (!seen_value) {
        seen_value = value;
      } else if (*seen_value != value) {
        return true;
      }
    }
    return false;
  };

  DenseSet<AttributeType> types;

  for (const EntityInstance* entity : entities) {
    for (const AttributeInstance& attribute : entity->attributes()) {
      AttributeType type = attribute.type();
      if (is_relevant(type) && !types.contains(type) &&
          is_disambiguating(type)) {
        types.insert(type);
      }
    }
  }

  if (return_at_least_one_label) {
    // We fill up `types` with types so that every EntityInstance defines a
    // value for at least one AttributeType.
    DenseSet<EntityType> unsatisfied_entity_types = DenseSet<EntityType>(
        entities, [](const EntityInstance* entity) { return entity->type(); });
    unsatisfied_entity_types.erase_all(DenseSet<EntityType>(
        types,
        [](AttributeType attribute) { return attribute.entity_type(); }));
    for (const EntityInstance* entity : entities) {
      if (!unsatisfied_entity_types.contains(entity->type())) {
        continue;
      }
      if (auto attributes = entity->attributes(); !attributes.empty()) {
        AttributeType type = attributes[0].type();
        if (is_relevant(type)) {
          types.insert(type);
        }
      }
    }
  }

  // Highest priority first.
  std::vector<AttributeType> vec = base::ToVector(types);
  std::ranges::sort(vec, AttributeType::DisambiguationOrder);
  return vec;
}

size_t CountUniqueNonEmptyLabels(const EntitiesLabels& labels) {
  // During label computation, every entity's label is a vector of non-empty
  // strings (which the UI later concatenates).
  using EntityLabel = std::vector<std::u16string>;

  // For space efficiency, we only store pointers (but compare the pointees).
  auto set = base::MakeFlatSet<const EntityLabel*>(
      *labels,
      [](const EntityLabel* lhs, const EntityLabel* rhs) {
        return *lhs < *rhs;
      },
      [](const EntityLabel& label) { return &label; });

  const EntityLabel empty_label;
  return set.size() - set.count(&empty_label);
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
  const size_t max_number_of_labels =
      std::min(kMaxNumberOfLabels, entities_labels_output->size());
  for (AttributeType attribute_type_to_use_as_label :
       GetDisambiguatingTypes(entities, allow_only_disambiguating_types,
                              return_at_least_one_label, app_locale)) {
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
    }

    // If every EntityInstance has a unique non-empty label, we're done.
    if (CountUniqueNonEmptyLabels(entities_labels_output) == entities.size()) {
      break;
    }
  }
  return entities_labels_output;
}

}  // namespace autofill
