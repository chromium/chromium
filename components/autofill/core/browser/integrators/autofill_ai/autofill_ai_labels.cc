// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_labels.h"

#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

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

// Returns the types for which at least two of the given `entities` define
// distinct values.
//
// The returned types are sorted so that the attributes with the highest
// priority in the disambiguation order come first.
//
// If `allow_only_disambiguating_values` is true and the entities do not differ
// in any type, then we fall back to types for which they define a non-empty
// value.
std::vector<AttributeType> GetDisambiguatingTypes(
    base::span<const EntityInstance*> entities,
    bool allow_only_disambiguating_types,
    bool allow_only_disambiguating_values,
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

  if (!allow_only_disambiguating_values) {
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

size_t CountUniqueNonEmptyLabels(const std::vector<EntityLabel>& labels) {
  // For space efficiency, we only store pointers (but compare the pointees).
  auto set = base::MakeFlatSet<const EntityLabel*>(
      labels,
      [](const EntityLabel* lhs, const EntityLabel* rhs) {
        return *lhs < *rhs;
      },
      [](const EntityLabel& label) { return &label; });

  const EntityLabel empty_label;
  return set.size() - set.count(&empty_label);
}

}  // namespace

std::vector<EntityLabel> GetLabelsForEntities(
    base::span<const EntityInstance*> entities,
    bool allow_only_disambiguating_types,
    bool allow_only_disambiguating_values,
    const std::string& app_locale) {
  std::vector<EntityLabel> labels;
  labels.resize(entities.size());

  const size_t max_number_of_labels =
      std::min(kMaxNumberOfLabels, labels.size());
  for (AttributeType type :
       GetDisambiguatingTypes(entities, allow_only_disambiguating_types,
                              allow_only_disambiguating_values, app_locale)) {
    // Potentially add `entity`'s value for `type` to the label.
    for (auto [entity, label] : base::zip(entities, labels)) {
      if (entity->type() != type.entity_type() ||
          label.size() == max_number_of_labels) {
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

    // If every EntityInstance has a unique non-empty label, we're done.
    if (CountUniqueNonEmptyLabels(labels) == entities.size()) {
      break;
    }
  }

  DCHECK_EQ(entities.size(), labels.size());
  DCHECK(std::ranges::all_of(labels, [](const EntityLabel& label) {
    return std::ranges::all_of(
        label, [](const std::u16string& str) { return !str.empty(); });
  }));
  return labels;
}

}  // namespace autofill
