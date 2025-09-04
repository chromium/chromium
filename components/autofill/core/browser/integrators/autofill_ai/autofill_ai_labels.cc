// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_labels.h"

#include <algorithm>
#include <functional>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/extend.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/strings/string_util.h"
#include "base/types/zip.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"

namespace autofill {

namespace {

// The maximum number of entity values/labels that can be used when
// disambiguating suggestions/entities. Used by suggestion generation and the
// settings page.
constexpr size_t kMaxNumberOfLabels = 2;

// An AttributeType is disambiguating in value if two entities disagree on its
// value. Ignores entities unrelated to the AttributeType.
bool AtLeastTwoEntityInstancesDifferInAttribute(
    AttributeType type,
    base::span<const EntityInstance*> entities,
    const std::string& app_locale) {
  auto get_info = [&app_locale](const EntityInstance& entity,
                                AttributeType type) {
    base::optional_ref<const AttributeInstance> attribute =
        entity.attribute(type);
    return attribute ? std::optional(attribute->GetCompleteInfo(app_locale))
                     : std::nullopt;
  };
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
}

// Given `entities`, returns a list of `AttributeType` with the required order
// for disambiguation:
// - Types belonging to the same `EntityType` are next to each other and sorted
//   according to `AttributeType::DisambiguationOrder`.
// - The order between types of different `EntityType`'s is irrelevant.
// - `attribute_types_to_ignore` are excluded from the list.
std::vector<AttributeType> GetOrderedAttributeTypesForDisambiguation(
    base::span<const EntityInstance*> entities,
    DenseSet<AttributeType> attribute_types_to_ignore) {
  std::vector<AttributeType> ordered_attributes;
  for (EntityType entity_type :
       base::MakeFlatSet<EntityType>(entities, {}, &EntityInstance::type)) {
    DenseSet<AttributeType> entity_attributes = entity_type.attributes();
    entity_attributes.erase_all(attribute_types_to_ignore);
    std::vector<AttributeType> sorted_attributes =
        base::ToVector(entity_attributes);
    std::ranges::sort(sorted_attributes, AttributeType::DisambiguationOrder);
    base::Extend(ordered_attributes, std::move(sorted_attributes));
  }
  return ordered_attributes;
}

// Given a `type`, expands `labels` of each of `entities` that support `type`
// with the information stored in its corresponding `AttributeInstance`.
// - If `only_add_to_empty_labels` is true, the function adds a new label only
//   to entities that currently have an empty label.
void ExpandEntityLabels(AttributeType type,
                        base::span<const EntityInstance*> entities,
                        base::span<EntityLabel> labels,
                        bool only_add_to_empty_labels,
                        const std::string& app_locale) {
  const size_t max_number_of_labels =
      std::min(kMaxNumberOfLabels, labels.size());
  for (auto [entity, label] : base::zip(entities, labels)) {
    if (entity->type() != type.entity_type()) {
      // Unrelated entity.
      continue;
    }
    if (label.size() == max_number_of_labels) {
      // No more labels can be added for this particular entity.
      continue;
    }
    if (only_add_to_empty_labels && !label.empty()) {
      // The entity doesn't need more labels.
      continue;
    }
    base::optional_ref<const AttributeInstance> attribute =
        entity->attribute(type);
    std::u16string label_value =
        attribute ? attribute->GetCompleteInfo(app_locale) : std::u16string();
    if (!label_value.empty()) {
      label.push_back(label_value);
    }
  }
}

// Iterates over `ordered_attributes` once and tries to find `AttributeType`s
// for which labels can be added.
// - `added_types` contains `AttributeType`s for which information is already
//   present in some of `labels`.
// - If `require_disambiguating_types` is true, the function will only add
//   `AttributeType`s that are disambiguating.
// - If `require_disambiguating_values` is true and `entities` contain more than
//   one `EntityInstance` for which an `AttributeType` is relevant, the function
//   will only add that type if it differentiates at least two of those
//   entities.
void AddLabelsRound(base::span<const EntityInstance*> entities,
                    base::span<const AttributeType> ordered_attributes,
                    base::span<EntityLabel> labels,
                    DenseSet<AttributeType>& added_types,
                    bool require_disambiguating_types,
                    bool require_disambiguating_values,
                    bool only_add_to_empty_labels,
                    const std::string& app_locale) {
  if (only_add_to_empty_labels &&
      std::ranges::all_of(labels,
                          std::not_fn(&std::vector<std::u16string>::empty))) {
    return;
  }
  for (AttributeType type : ordered_attributes) {
    if (added_types.contains(type)) {
      // The current type was already added as label.
      continue;
    }
    if (require_disambiguating_types && !type.is_disambiguation_type()) {
      continue;
    }
    if (require_disambiguating_values &&
        std::ranges::count(entities, type.entity_type(),
                           &EntityInstance::type) > 1 &&
        !AtLeastTwoEntityInstancesDifferInAttribute(type, entities,
                                                    app_locale)) {
      continue;
    }
    added_types.insert(type);
    ExpandEntityLabels(type, entities, labels, only_add_to_empty_labels,
                       app_locale);
  }
}

}  // namespace

std::vector<EntityLabel> GetLabelsForEntities(
    base::span<const EntityInstance*> entities,
    DenseSet<AttributeType> attribute_types_to_ignore,
    bool prioritize_disambiguating_types,
    const std::string& app_locale) {
  std::vector<EntityLabel> labels(entities.size());
  DenseSet<AttributeType> added_types;

  std::vector<AttributeType> ordered_attributes =
      GetOrderedAttributeTypesForDisambiguation(entities,
                                                attribute_types_to_ignore);

  if (prioritize_disambiguating_types) {
    AddLabelsRound(entities, ordered_attributes, labels, added_types,
                   /*require_disambiguating_types=*/true,
                   /*require_disambiguating_values=*/true,
                   /*only_add_to_empty_labels=*/false, app_locale);
  }

  AddLabelsRound(entities, ordered_attributes, labels, added_types,
                 /*require_disambiguating_types=*/false,
                 /*require_disambiguating_values=*/true,
                 // This should only be false in the first call to the function.
                 /*only_add_to_empty_labels=*/prioritize_disambiguating_types,
                 app_locale);

  AddLabelsRound(entities, ordered_attributes, labels, added_types,
                 /*require_disambiguating_types=*/false,
                 /*require_disambiguating_values=*/false,
                 /*only_add_to_empty_labels=*/true, app_locale);

  return labels;
}

}  // namespace autofill
