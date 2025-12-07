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
#include "components/autofill/core/browser/data_model/data_model_utils.h"
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
  if (type == AttributeType(kFlightReservationDepartureDate)) {
    static constexpr char16_t date_format[] = u"MMM d";
    base::optional_ref<const AttributeInstance> attribute =
        entity.attribute(type);
    if (!attribute) {
      return {u"", {type}};
    }
    std::optional<std::u16string> localized_pattern =
        data_util::LocalizePattern(date_format, app_locale);
    AutofillFormatString format_string(
        data_util::LocalizePattern(date_format, app_locale)
            .value_or(date_format),
        FormatString_Type_ICU_DATE);
    return {attribute->GetInfo(type.field_type(), app_locale, format_string),
            {type}};
  }
  return {GetInfo(entity, type, app_locale), {type}};
}

// Given `entities`, having all the same `entity_type`, returns two lists
// containing `AttributeType`s with the required order for disambiguation:
// - The first list contains types to be prioritized. The second list contains
//   the other types to be considered. Later when generating labels, the first
//   list is considered first while the second is only used as a fallback (See
//   `GetLabelsForEntities()` below).
// - A type is prioritized if:
//   - The type satisfies `AttributeType::is_disambiguating_type()`.
//   - At least one entity supports that type.
//   - If multiple `entities` support that type: The entities do not all have
//     the same value for that type.
//   - If only a single entity supports that attribute: That entity does not
//     have an empty value for that attribute.
// - Each list is sorted by `AttributeType::DisambiguationOrder()`.
std::pair<std::vector<AttributeType>, std::vector<AttributeType>>
GetOrderedAttributeTypesForDisambiguation(
    EntityType entity_type,
    base::span<const EntityInstance* const> entities,
    bool only_disambiguating_types,
    const std::string& app_locale) {
  auto should_prioritize = [&](AttributeType attribute_type) {
    if (!attribute_type.is_disambiguation_type()) {
      return false;
    }
    std::vector<std::u16string> values;
    for (const EntityInstance* entity : entities) {
      auto [value, types_used] =
          GetValueAndTypesForLabel(*entity, attribute_type, app_locale);
      values.push_back(std::move(value));
    }
    return values.size() == 1
               ? !values.back().empty()
               : base::MakeFlatSet<std::u16string>(values).size() > 1;
  };

  std::vector<AttributeType> types = base::ToVector(entity_type.attributes());
  if (only_disambiguating_types) {
    std::erase_if(types, std::not_fn(&AttributeType::is_disambiguation_type));
  }
  std::ranges::sort(types, AttributeType::DisambiguationOrder);

  std::vector<AttributeType> high_priority_types;
  std::vector<AttributeType> low_priority_types;
  for (AttributeType attribute_type : types) {
    if (should_prioritize(attribute_type)) {
      high_priority_types.push_back(attribute_type);
    } else {
      low_priority_types.push_back(attribute_type);
    }
  }

  return {high_priority_types, low_priority_types};
}

// Given a `type`, expands `labels` of each of `entities` with the information
// stored in its corresponding `AttributeInstance`.
// - `tried_types` contains `AttributeType`s for which we already tried adding
//   information for in some of `labels`.
// - If `only_add_to_empty_labels` is true, the function adds a new label only
//   to entities that currently have an empty label.
void ExpandEntityLabels(AttributeType type,
                        base::span<const EntityInstance* const> entities,
                        base::span<EntityLabel> labels,
                        DenseSet<AttributeType>& tried_types,
                        bool only_add_to_empty_labels,
                        const std::string& app_locale) {
  for (auto [entity, label] : base::zip(entities, labels)) {
    if (label.size() == kMaxNumberOfLabels) {
      // No more labels can be added for this particular entity.
      continue;
    }
    if (only_add_to_empty_labels && !label.empty()) {
      // The entity doesn't need more labels.
      continue;
    }
    if (auto [value, used_types] =
            GetValueAndTypesForLabel(*entity, type, app_locale);
        !value.empty()) {
      tried_types.insert_all(std::move(used_types));
      label.push_back(std::move(value));
    }
  }
}

// Iterates over `ordered_attributes` once and tries to find `AttributeType`s
// for which labels can be added.
// - `tried_types` contains `AttributeType`s for which we already tried adding
//   information for in some of `labels`.
// - if `only_add_to_empty_labels` is true, label addition stops when all labels
//   are non-empty, otherwise label addition stops when all labels are unique
//   and non-empty.
void AddLabelsRound(base::span<const EntityInstance* const> entities,
                    base::span<const AttributeType> ordered_attributes,
                    base::span<EntityLabel> labels,
                    DenseSet<AttributeType>& tried_types,
                    bool only_add_to_empty_labels,
                    const std::string& app_locale) {
  auto labels_are_non_empty = [&] {
    return std::ranges::none_of(labels, &EntityLabel::empty);
  };
  auto labels_are_unique = [&] {
    return base::MakeFlatSet<EntityLabel>(labels).size() == labels.size();
  };

  for (AttributeType type : ordered_attributes) {
    if (tried_types.contains(type)) {
      continue;
    }
    if (labels_are_non_empty() &&
        (only_add_to_empty_labels || labels_are_unique())) {
      break;
    }
    ExpandEntityLabels(type, entities, labels, tried_types,
                       only_add_to_empty_labels, app_locale);
  }
}

}  // namespace

std::vector<EntityLabel> GetLabelsForEntities(
    base::span<const EntityInstance* const> entities,
    DenseSet<AttributeType> attribute_types_to_ignore,
    bool only_disambiguating_types,
    const std::string& app_locale) {
  std::map<EntityType, std::vector<const EntityInstance*>> entities_by_type;
  for (const EntityInstance* entity : entities) {
    entities_by_type[entity->type()].push_back(entity);
  }

  DenseSet<AttributeType> tried_types = attribute_types_to_ignore;
  std::map<const EntityInstance*, EntityLabel> labels_for_entity;
  for (const auto& [entity_type, entities_for_type] : entities_by_type) {
    auto [high_priority_types, low_priority_types] =
        GetOrderedAttributeTypesForDisambiguation(
            entity_type, entities_for_type, only_disambiguating_types,
            app_locale);

    std::vector<EntityLabel> labels(entities_for_type.size());

    // In the first round, labels are added from `high_priority_types` until
    // either all the types were considered or we reach a state where all
    // `labels` are unique and non-empty.
    AddLabelsRound(entities_for_type, high_priority_types, labels, tried_types,
                   /*only_add_to_empty_labels=*/false, app_locale);

    // The second round aims at adding labels to entities that still have an
    // empty label, if any. Since all `priority_types` were already considered,
    // this round considers adding labels from `low_priority_types`.
    AddLabelsRound(entities_for_type, low_priority_types, labels, tried_types,
                   /*only_add_to_empty_labels=*/true, app_locale);

    // Map the entity to its corresponding label so that the function is able to
    // reconstruct the list of `EntityLabel`s according to the ordering provided
    // by `entities`.
    for (auto [entity, label] : base::zip(entities_for_type, labels)) {
      labels_for_entity[entity] = std::move(label);
    }
  }

  return base::ToVector(entities,
                        [&labels_for_entity](const EntityInstance* entity) {
                          return labels_for_entity[entity];
                        });
}

}  // namespace autofill
