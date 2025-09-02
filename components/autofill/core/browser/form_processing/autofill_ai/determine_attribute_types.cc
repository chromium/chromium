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
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/field_types.h"
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
static constexpr FieldTypeSet kNonInjectiveFieldTypes =
    FieldTypesOfGroup(FieldTypeGroup::kName);

// Some plausibility checks.
static_assert(kNonInjectiveFieldTypes.contains_all({NAME_FULL, NAME_FIRST,
                                                    NAME_LAST, NAME_MIDDLE}));
static_assert(!kNonInjectiveFieldTypes.contains_any(
    {ADDRESS_HOME_STATE, ADDRESS_HOME_ZIP, CREDIT_CARD_NUMBER}));
static_assert(!kNonInjectiveFieldTypes.contains_any(
    {DRIVERS_LICENSE_EXPIRATION_DATE, PASSPORT_NUMBER, VEHICLE_MODEL}));

// Checks that AttributeType::field_type() is mostly injective:
// distinct AttributeTypes other than those having field_type() in
// `kNonInjectiveFieldTypes` must be mapped to distinct FieldTypes.
consteval bool IsMostlyInjective() {
  FieldTypeSet field_types;

  for (AttributeType at : DenseSet<AttributeType>::all()) {
    auto [_, inserted] = field_types.insert(at.field_type());
    if (!inserted && !kNonInjectiveFieldTypes.contains(at.field_type())) {
      return false;
    }
  }

  return true;
}

// AttributeType::field_type() must be mostly injective.
static_assert(IsMostlyInjective(),
              "AttributeType::field_type() is not mostly injective.");

// A FieldType's static AttributeType is the unique AttributeType whose
// AttributeType::field_type() is the field's FieldType.
std::optional<AttributeType> GetStaticAttributeType(FieldType ft) {
  // Returns `at` if its entity is enabled and std::nullopt otherwise.
  auto if_enabled = [](std::optional<AttributeType> at) {
    return at && at->entity_type().enabled() ? at : std::nullopt;
  };

  // This lookup table is the inverse of AttributeType::field_type(), except
  // for the `kNonInjectiveFieldTypes`.
  static auto kTable = []() {
    std::array<std::optional<AttributeType>, MAX_VALID_FIELD_TYPE> arr{};
    for (AttributeType at : DenseSet<AttributeType>::all()) {
      if (!kNonInjectiveFieldTypes.contains(at.field_type())) {
        arr[at.field_type()] = at;
      }
    }
    return arr;
  }();
  return 0 <= ft && ft < kTable.size() ? if_enabled(kTable[ft]) : std::nullopt;
}

// A field is assignable a dynamic AttributeType iff it is a name field.
bool IsAssignableDynamicAttributeType(const FieldTypeSet& fts) {
  return kNonInjectiveFieldTypes.contains_any(fts);
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

// Returns the static AttributeTypes.
// The `i`th value in the returned vector is the static type of `fields[i]`,
// except if there is none, in which case the vector is empty.
std::vector<DenseSet<AttributeType>> GetStaticAttributeTypes(
    base::span<const std::unique_ptr<AutofillField>> fields) {
  std::vector<DenseSet<AttributeType>> attributes_by_field;
  for (size_t i = 0; i < fields.size(); ++i) {
    const AutofillField& field = *fields[i];
    if (!IsRelevant(field)) {
      continue;
    }
    for (FieldType ft : field.Type().GetStaticAutofillAiTypes()) {
      std::optional<AttributeType> at = GetStaticAttributeType(ft);
      if (!at) {
        continue;
      }
      attributes_by_field.resize(fields.size());
      attributes_by_field[i].insert(*at);
    }
  }
  return attributes_by_field;
}

// Adds the dynamic types of `fields[i]` to `attributes_by_field[i]`.
void AddDynamicAttributeTypes(
    base::span<const std::unique_ptr<AutofillField>> fields,
    base::span<DenseSet<AttributeType>> attributes_by_field) {
  DCHECK_EQ(fields.size(), attributes_by_field.size());
  DCHECK(!std::ranges::all_of(attributes_by_field,
                              &DenseSet<AttributeType>::empty));

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
    const FieldTypeSet field_types = field.Type().GetTypes();
    if (IsAssignableDynamicAttributeType(field_types)) {
      for (const auto& [p, entity_offset] : last_seen) {
        const auto& [entity_section, entity] = p;
        if (std::abs(entity_offset - offset) > kMaxPropagationDistance ||
            entity_section != field.section()) {
          continue;
        }
        for (const FieldType field_type : field_types) {
          if (const std::optional<AttributeType> attribute =
                  GetAttributeType(entity, field_type)) {
            attributes.insert(*attribute);
          }
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
// The `i`th value in the returned vector is the type of `fields[i]`.
std::vector<DenseSet<AttributeType>> GetAttributeTypes(
    base::span<const std::unique_ptr<AutofillField>> fields) {
  std::vector<DenseSet<AttributeType>> attributes_by_field =
      GetStaticAttributeTypes(fields);
  if (!attributes_by_field.empty()) {
    AddDynamicAttributeTypes(fields, attributes_by_field);
  }
  return attributes_by_field;
}

}  // namespace

std::vector<AutofillFieldWithAttributeType> DetermineAttributeTypes(
    base::span<const std::unique_ptr<AutofillField>> fields LIFETIME_BOUND,
    const Section& section_of_interest,
    EntityType entity_of_interest,
    DetermineAttributeTypesPassKey pass_key) {
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
    const Section& section_of_interest,
    DetermineAttributeTypesPassKey pass_key) {
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
    base::span<const std::unique_ptr<AutofillField>> fields LIFETIME_BOUND,
    DetermineAttributeTypesPassKey pass_key) {
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
