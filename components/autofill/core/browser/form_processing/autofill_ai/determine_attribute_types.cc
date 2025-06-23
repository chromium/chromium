// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_processing/autofill_ai/determine_attribute_types.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/types/zip.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

namespace {

// The furthest distance between two fields so that one field's AttributeType
// may lead to a dynamic AttributeType assignment of the other.
constexpr int kMaxPropagationDistance = 5;

bool IsRelevant(const AutofillField& field) {
  return field.is_focusable() || field.IsSelectElement();
}

// The set of all FieldTypes that have **more** than one associated
// AttributeType.
static constexpr FieldTypeSet kNonInjectiveFieldTypes = [] {
  DenseSet<FieldType> hit;
  DenseSet<FieldType> hit_once;
  for (AttributeType a : DenseSet<AttributeType>::all()) {
    for (FieldType ft : a.field_subtypes()) {
      if (hit.contains(ft)) {
        hit_once.erase(ft);
      } else {
        hit_once.insert(ft);
      }
      hit.insert(ft);
    }
  }
  DenseSet<FieldType> hit_multiple = hit;
  hit_multiple.erase_all(hit_once);
  return hit_multiple;
}();

// Some plausibility checks.
static_assert(kNonInjectiveFieldTypes.contains_all({NAME_FULL, NAME_FIRST,
                                                    NAME_LAST, NAME_MIDDLE}));
static_assert(!kNonInjectiveFieldTypes.contains_any(
    {ADDRESS_HOME_STATE, ADDRESS_HOME_ZIP, CREDIT_CARD_NUMBER}));
static_assert(!kNonInjectiveFieldTypes.contains_any(
    {DRIVERS_LICENSE_EXPIRATION_DATE, PASSPORT_NUMBER, VEHICLE_MODEL}));

// If kAutofillAiNoTagTypes is disabled:
// AttributeType::field_type() must be injective: distinct AttributeTypes must
// be mapped to distinct FieldTypes.
static_assert(
    std::ranges::all_of(DenseSet<AttributeType>::all(), [](AttributeType a) {
      return std::ranges::all_of(
          DenseSet<AttributeType>::all(), [&a](AttributeType b) {
            return a == b || a.field_type_with_tag_types() !=
                                 b.field_type_with_tag_types();
          });
    }));

// If kAutofillAiNoTagTypes is enabled:
// AttributeType::field_type() must be mostly injective: distinct AttributeTypes
// other than `kNonInjectiveFieldTypes` must be mapped to distinct FieldTypes.
static_assert(
    std::ranges::all_of(DenseSet<AttributeType>::all(), [](AttributeType a) {
      return std::ranges::all_of(
          DenseSet<AttributeType>::all(), [&a](AttributeType b) {
            return a == b ||
                   a.field_type_without_tag_types() !=
                       b.field_type_without_tag_types() ||
                   kNonInjectiveFieldTypes.contains(
                       a.field_type_without_tag_types()) ||
                   kNonInjectiveFieldTypes.contains(
                       b.field_type_without_tag_types());
          });
    }));

// A field's static AttributeType is the unique AttributeType whose
// AttributeType::field_type() is the field's FieldType.
std::optional<AttributeType> GetStaticAttributeType(
    const AutofillField& field) {
  std::optional<FieldType> ft = field.GetAutofillAiServerTypePredictions();
  if (!ft) {
    return std::nullopt;
  }

  if (!base::FeatureList::IsEnabled(features::kAutofillAiNoTagTypes)) {
    static constexpr auto kTable = []() {
      std::array<std::optional<AttributeType>, MAX_VALID_FIELD_TYPE> arr{};
      for (AttributeType at : DenseSet<AttributeType>::all()) {
        arr[at.field_type_with_tag_types()] = at;
      }
      return arr;
    }();
    return 0 <= *ft && *ft < kTable.size() ? kTable[*ft] : std::nullopt;
  }

  // This lookup table is the inverse of AttributeType::field_type(), except
  // for the `kNonInjectiveFieldTypes`.
  static constexpr auto kTable = []() {
    std::array<std::optional<AttributeType>, MAX_VALID_FIELD_TYPE> arr{};
    for (AttributeType at : DenseSet<AttributeType>::all()) {
      if (!kNonInjectiveFieldTypes.contains(
              at.field_type_without_tag_types())) {
        arr[at.field_type_without_tag_types()] = at;
      }
    }
    return arr;
  }();
  return 0 <= *ft && *ft < kTable.size() ? kTable[*ft] : std::nullopt;
}

// A field is assignable a dynamic AttributeType if there are more than one
// AttributeTypes whose AttributeType::field_type() is the field's FieldType.
bool IsAssignableDynamicAttributeType(FieldType ft) {
  return kNonInjectiveFieldTypes.contains(ft);
}

std::optional<AttributeType> GetAttributeType(EntityType entity,
                                              FieldType field_type) {
  for (AttributeType at : entity.attributes()) {
    if (at.field_subtypes().contains(field_type)) {
      return at;
    }
  }
  return std::nullopt;
}

// Adds to `attributes_by_field[i]` the static types of `fields[i]`.
void AddStaticAttributeTypes(
    base::span<const std::unique_ptr<AutofillField>> fields,
    base::span<DenseSet<AttributeType>> attributes_by_field) {
  DCHECK_EQ(fields.size(), attributes_by_field.size());
  for (auto [field, attributes] : base::zip(fields, attributes_by_field)) {
    if (!IsRelevant(*field)) {
      continue;
    }
    std::optional<AttributeType> at = GetStaticAttributeType(*field);
    if (!at) {
      continue;
    }
    attributes.insert(*at);
  }
}

// Adds to `attributes_by_field[i]` the dynamic types of `fields[i]`.
void AddDynamicAttributeTypes(
    base::span<const std::unique_ptr<AutofillField>> fields,
    base::span<DenseSet<AttributeType>> attributes_by_field) {
  DCHECK_EQ(fields.size(), attributes_by_field.size());
  if (!base::FeatureList::IsEnabled(features::kAutofillAiNoTagTypes) ||
      std::ranges::all_of(attributes_by_field,
                          &DenseSet<AttributeType>::empty)) {
    return;
  }

  // Propagates the applicable EntityTypes in `last_seen` to the `attributes` of
  // `field`.
  //
  // This function is to be called in sequence for a range of AutofillFields.
  // `offset` counts how many relevant AutofillFields were encountered so far.
  // `last_seen` maps the EntityTypes and Sections to the maximum offset where
  // they were seen so far.
  auto loop_body = [](std::map<std::pair<Section, EntityType>, int>& last_seen,
                      int& offset, const AutofillField& field,
                      DenseSet<AttributeType>& attributes) {
    if (!IsRelevant(field)) {
      return;
    }
    ++offset;
    const FieldType field_type = field.Type().GetStorableType();
    if (IsAssignableDynamicAttributeType(field_type)) {
      for (const auto& [p, entity_offset] : last_seen) {
        const auto& [entity_section, entity] = p;
        if (std::abs(entity_offset - offset) > kMaxPropagationDistance ||
            entity_section != field.section()) {
          continue;
        }
        if (const std::optional<AttributeType> attribute =
                GetAttributeType(entity, field_type)) {
          attributes.insert(*attribute);
        }
      }
    }
    for (AttributeType attribute : attributes) {
      last_seen[{field.section(), attribute.entity_type()}] = offset;
    }
  };

  // Propagate types forward and backward.
  const int num_fields = static_cast<int>(fields.size());
  {
    std::map<std::pair<Section, EntityType>, int> last_seen;
    int offset = 0;
    for (int i = 0; i < num_fields; ++i) {
      loop_body(last_seen, offset, *fields[i], attributes_by_field[i]);
    }
  }
  {
    std::map<std::pair<Section, EntityType>, int> last_seen;
    int offset = 0;
    for (int i = num_fields - 1; i >= 0; --i) {
      loop_body(last_seen, offset, *fields[i], attributes_by_field[i]);
    }
  }
}

// Returns the static and dynamic AttributeTypes.
// The `i`th value in the returned vector is the dynamic type of `fields[i]`.
std::vector<DenseSet<AttributeType>> GetAttributeTypes(
    base::span<const std::unique_ptr<AutofillField>> fields) {
  std::vector<DenseSet<AttributeType>> attributes_by_field;
  attributes_by_field.resize(fields.size());
  AddStaticAttributeTypes(fields, attributes_by_field);
  AddDynamicAttributeTypes(fields, attributes_by_field);
  return attributes_by_field;
}

}  // namespace

std::vector<AutofillFieldWithAttributeType> DetermineAttributeTypes(
    base::span<const std::unique_ptr<AutofillField>> fields LIFETIME_BOUND,
    const Section& section_of_interest,
    EntityType entity_of_interest) {
  const std::vector<DenseSet<AttributeType>> attributes_by_field =
      GetAttributeTypes(fields);
  std::vector<AutofillFieldWithAttributeType> r;
  for (auto [field, attributes] : base::zip(fields, attributes_by_field)) {
    if (!section_of_interest || field->section() == section_of_interest) {
      for (AttributeType attribute : attributes) {
        if (entity_of_interest == attribute.entity_type()) {
          r.emplace_back(*field, attribute);
        }
      }
    }
  }
  return r;
}

using EntityMap =
    base::flat_map<EntityType, std::vector<AutofillFieldWithAttributeType>>;

EntityMap DetermineAttributeTypes(
    base::span<const std::unique_ptr<AutofillField>> fields LIFETIME_BOUND,
    const Section& section_of_interest) {
  const std::vector<DenseSet<AttributeType>> attributes_by_field =
      GetAttributeTypes(fields);
  EntityMap r;
  for (auto [field, attributes] : base::zip(fields, attributes_by_field)) {
    if (!section_of_interest || field->section() == section_of_interest) {
      for (AttributeType attribute : attributes) {
        r[attribute.entity_type()].emplace_back(*field, attribute);
      }
    }
  }
  return r;
}

using SectionMap = base::flat_map<Section, EntityMap>;

SectionMap DetermineAttributeTypes(
    base::span<const std::unique_ptr<AutofillField>> fields LIFETIME_BOUND) {
  const std::vector<DenseSet<AttributeType>> attributes_by_field =
      GetAttributeTypes(fields);
  SectionMap r;
  for (auto [field, attributes] : base::zip(fields, attributes_by_field)) {
    for (AttributeType attribute : attributes) {
      r[field->section()][attribute.entity_type()].emplace_back(*field,
                                                                attribute);
    }
  }
  return r;
}

}  // namespace autofill
