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

#include "base/containers/contains.h"
#include "base/containers/extend.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/strings/string_util.h"
#include "base/types/zip.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"

namespace autofill {

namespace {

// The maximum number of entity values/labels that can be used when
// disambiguating suggestions/entities. Used by suggestion generation and the
// settings page.
constexpr size_t kMaxNumberOfLabels = 2;

std::u16string GetInfo(const EntityInstance& entity,
                       AttributeType type,
                       const std::string& app_locale) {
  base::optional_ref<const AttributeInstance> attribute =
      entity.attribute(type);
  return attribute ? attribute->GetCompleteInfo(app_locale) : std::u16string();
}

// Joins the non-empty values `attributes` in `entity` with the `delimiter` and
// returns the resulting string.
std::u16string JoinAttributes(const EntityInstance& entity,
                              base::span<const AttributeType> attributes,
                              std::u16string_view separator,
                              const std::string& app_locale) {
  std::vector<std::u16string> combination;
  for (const AttributeType at : attributes) {
    if (std::u16string value = GetInfo(entity, at, app_locale);
        !value.empty()) {
      combination.push_back(std::move(value));
    }
  }
  return base::JoinString(combination, separator);
}

// Returns the value that should be added as label to `entity`, given a `type`
// and formatted according to `app_locale`. Also return the set of
// `AttributeType`s used to build that label, as sometimes many types come into
// play.
std::pair<std::u16string, DenseSet<AttributeType>> GetValueAndTypesForLabel(
    const EntityInstance& entity,
    AttributeType type,
    const std::string& app_locale) {
  using enum AttributeTypeName;
  static constexpr std::array kAirports = {
      AttributeType(kFlightReservationDepartureAirport),
      AttributeType(kFlightReservationArrivalAirport)};
  if (base::Contains(kAirports, type)) {
    // The label for flight airport information should be:
    // - Empty if no airport information is available.
    // - "DEPARTURE–ARRIVAL" if both the departure and arrival airports are
    //   available in the `entity`.
    // - The one that is available otherwise.
    return {JoinAttributes(entity, kAirports, u"\u2013", app_locale),
            DenseSet<AttributeType>(kAirports)};
  }
  return {GetInfo(entity, type, app_locale), {type}};
}

// An AttributeType is disambiguating in value if two entities disagree on the
// label derived from it. Ignores entities unrelated to the AttributeType.
bool AtLeastTwoEntityInstancesDifferInAttribute(
    AttributeType type,
    base::span<const EntityInstance*> entities,
    const std::string& app_locale) {
  std::optional<std::optional<std::u16string>> seen_value;
  for (const EntityInstance* entity : entities) {
    if (!entity->type().attributes().contains(type)) {
      continue;
    }
    std::u16string value =
        GetValueAndTypesForLabel(*entity, type, app_locale).first;
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
// - `tried_types` contains `AttributeType`s for which we already tried adding
//   information for in some of `labels`.
// - If `only_add_to_empty_labels` is true, the function adds a new label only
//   to entities that currently have an empty label.
void ExpandEntityLabels(AttributeType type,
                        base::span<const EntityInstance*> entities,
                        base::span<EntityLabel> labels,
                        DenseSet<AttributeType>& tried_types,
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
    const auto [value, used_types] =
        GetValueAndTypesForLabel(*entity, type, app_locale);
    if (!value.empty()) {
      tried_types.insert_all(used_types);
      label.push_back(value);
    }
  }
}

// Iterates over `ordered_attributes` once and tries to find `AttributeType`s
// for which labels can be added.
// - `tried_types` contains `AttributeType`s for which we already tried adding
//   information for in some of `labels`.
// - If `require_disambiguating_types` is true, the function will only add
//   `AttributeType`s that are disambiguating.
// - If `require_disambiguating_values` is true and `entities` contain more than
//   one `EntityInstance` for which an `AttributeType` is relevant, the function
//   will only add that type if it differentiates at least two of those
//   entities.
void AddLabelsRound(base::span<const EntityInstance*> entities,
                    base::span<const AttributeType> ordered_attributes,
                    base::span<EntityLabel> labels,
                    DenseSet<AttributeType>& tried_types,
                    bool require_disambiguating_types,
                    bool require_disambiguating_values,
                    bool only_add_to_empty_labels,
                    const std::string& app_locale) {
  if (only_add_to_empty_labels &&
      std::ranges::all_of(labels, std::not_fn(&EntityLabel::empty))) {
    return;
  }
  for (AttributeType type : ordered_attributes) {
    if (tried_types.contains(type)) {
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
    ExpandEntityLabels(type, entities, labels, tried_types,
                       only_add_to_empty_labels, app_locale);
  }
}

}  // namespace

std::vector<EntityLabel> GetLabelsForEntities(
    base::span<const EntityInstance*> entities,
    DenseSet<AttributeType> attribute_types_to_ignore,
    bool prioritize_disambiguating_types,
    const std::string& app_locale) {
  std::vector<EntityLabel> labels(entities.size());
  DenseSet<AttributeType> tried_types;

  std::vector<AttributeType> ordered_attributes =
      GetOrderedAttributeTypesForDisambiguation(entities,
                                                attribute_types_to_ignore);

  if (prioritize_disambiguating_types) {
    AddLabelsRound(entities, ordered_attributes, labels, tried_types,
                   /*require_disambiguating_types=*/true,
                   /*require_disambiguating_values=*/true,
                   /*only_add_to_empty_labels=*/false, app_locale);
  }

  AddLabelsRound(entities, ordered_attributes, labels, tried_types,
                 /*require_disambiguating_types=*/false,
                 /*require_disambiguating_values=*/true,
                 // This should only be false in the first call to the function.
                 /*only_add_to_empty_labels=*/prioritize_disambiguating_types,
                 app_locale);

  AddLabelsRound(entities, ordered_attributes, labels, tried_types,
                 /*require_disambiguating_types=*/false,
                 /*require_disambiguating_values=*/false,
                 /*only_add_to_empty_labels=*/true, app_locale);

  return labels;
}

}  // namespace autofill
