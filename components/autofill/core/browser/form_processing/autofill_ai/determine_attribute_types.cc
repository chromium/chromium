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

using EntityMap =
    base::flat_map<EntityType, std::vector<AutofillFieldWithAttributeType>>;
using SectionMap = base::flat_map<Section, EntityMap>;

std::optional<AttributeType> GetStaticAttributeType(
    const AutofillField& field) {
  std::optional<FieldType> ft = field.GetAutofillAiServerTypePredictions();
  return ft ? AttributeType::FromFieldType(*ft) : std::nullopt;
}

// The `i`th element of the returned vector contains the type of `fields[i]`.
std::vector<std::optional<AttributeType>> GetStaticAttributeTypes(
    base::span<const std::unique_ptr<AutofillField>> fields,
    base::optional_ref<const Section> section_of_interest,
    base::optional_ref<const EntityType> entity_of_interest) {
  std::vector<std::optional<AttributeType>> field_to_type;
  field_to_type.resize(fields.size());
  for (size_t i = 0; i < fields.size(); ++i) {
    if (section_of_interest && *section_of_interest != fields[i]->section()) {
      continue;
    }
    std::optional<AttributeType> at = GetStaticAttributeType(*fields[i]);
    if (!at) {
      continue;
    }
    if (entity_of_interest && *entity_of_interest != at->entity_type()) {
      continue;
    }
    field_to_type[i] = at;
  }
  return field_to_type;
}

// The `i`th element of the returned vector contains the type of `fields[i]`.
std::vector<std::vector<AttributeType>> GetDynamicAttributeTypes(
    base::span<const std::unique_ptr<AutofillField>> fields,
    base::span<const std::optional<AttributeType>> stc_types,
    base::optional_ref<const Section> section_of_interest,
    base::optional_ref<const EntityType> entity_of_interest) {
  std::vector<std::vector<AttributeType>> dyn_types;
  dyn_types.resize(fields.size());

  if (!base::FeatureList::IsEnabled(features::kAutofillAiNoTagTypes)) {
    return dyn_types;
  }

  // TODO(crbug.com/422563282): Implement.
  return dyn_types;
}

// Merges the static and dynamic AttributeTypes and calls `cb(field, type)` for
// every possible `type` of `field`.
template <typename Callback>
void GetAttributeTypes(base::span<const std::unique_ptr<AutofillField>> fields,
                       base::optional_ref<const Section> section_of_interest,
                       base::optional_ref<const EntityType> entity_of_interest,
                       Callback cb) {
  std::vector<std::optional<AttributeType>> stc_types_by_field =
      GetStaticAttributeTypes(fields, section_of_interest, entity_of_interest);
  std::vector<std::vector<AttributeType>> dyn_types_by_field =
      GetDynamicAttributeTypes(fields, stc_types_by_field, section_of_interest,
                               entity_of_interest);
  for (auto [field, stc_type, dyn_types] :
       base::zip(fields, stc_types_by_field, dyn_types_by_field)) {
    if (stc_type) {
      DCHECK(dyn_types.empty());
      std::invoke(cb, *field, *stc_type);
    }
    for (AttributeType dyn_type : dyn_types) {
      DCHECK(!stc_type);
      std::invoke(cb, *field, dyn_type);
    }
  }
}

}  // namespace

std::vector<AutofillFieldWithAttributeType> DetermineAttributeTypes(
    base::span<const std::unique_ptr<AutofillField>> fields LIFETIME_BOUND,
    const Section& section_of_interest,
    EntityType entity_of_interest) {
  std::vector<AutofillFieldWithAttributeType> c;
  GetAttributeTypes(fields, section_of_interest, entity_of_interest,
                    [&](const AutofillField& field, AttributeType type) {
                      c.emplace_back(field, type);
                    });
  return c;
}

EntityMap DetermineAttributeTypes(
    base::span<const std::unique_ptr<AutofillField>> fields LIFETIME_BOUND,
    const Section& section_of_interest) {
  EntityMap c;
  GetAttributeTypes(fields, section_of_interest,
                    /*entity_of_interest=*/std::nullopt,
                    [&](const AutofillField& field, AttributeType type) {
                      c[type.entity_type()].emplace_back(field, type);
                    });
  return c;
}

SectionMap DetermineAttributeTypes(
    base::span<const std::unique_ptr<AutofillField>> fields LIFETIME_BOUND) {
  SectionMap c;
  GetAttributeTypes(fields, /*section_of_interest=*/std::nullopt,
                    /*entity_of_interest=*/std::nullopt,
                    [&](const AutofillField& field, AttributeType type) {
                      c[field.section()][type.entity_type()].emplace_back(field,
                                                                          type);
                    });
  return c;
}

}  // namespace autofill
