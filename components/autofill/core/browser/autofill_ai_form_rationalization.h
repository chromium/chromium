// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_AI_FORM_RATIONALIZATION_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_AI_FORM_RATIONALIZATION_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"

namespace autofill {

class AutofillField;
class Section;
struct AutofillFieldWithAttributeType;

// RationalizeAndDetermineAttributeTypes() computes the static and dynamic
// AttributeType assignments of a form (by calling `DetermineAttributeTypes()`)
// and rationalize these attribute types assignments.
//
// The rationalization steps are:
// - Enforce the required fields constraints for the matching entity.
//   For example, if the requirement for a passport entity is that a form
//   contains either number or expiry date and the determined types are passport
//   name and country, the rationalization completely filters out this output.
// - Delete adjacent license plate number fields because.
//   Such fields are likely split plate number fields, and Autofill AI currently
//   does not have heuristics for splitting the values accordingly.
//
// The overloads are just specializations for performance
// reasons. The following expressions are equivalent:
// - `RationalizeAndDetermineAttributeTypes(fields, section, entity)`
// - `RationalizeAndDetermineAttributeTypes(fields, section)[entity]`
// - `RationalizeAndDetermineAttributeTypes(fields)[section][entity]`

std::vector<AutofillFieldWithAttributeType>
RationalizeAndDetermineAttributeTypes(
    base::span<const std::unique_ptr<AutofillField>> fields LIFETIME_BOUND,
    const Section& section_of_interest,
    EntityType entity_of_interest);

base::flat_map<EntityType, std::vector<AutofillFieldWithAttributeType>>
RationalizeAndDetermineAttributeTypes(
    base::span<const std::unique_ptr<AutofillField>> fields LIFETIME_BOUND,
    const Section& section_of_interest);

base::flat_map<
    Section,
    base::flat_map<EntityType, std::vector<AutofillFieldWithAttributeType>>>
RationalizeAndDetermineAttributeTypes(
    base::span<const std::unique_ptr<AutofillField>> fields LIFETIME_BOUND);

std::vector<AutofillFieldWithAttributeType> RationalizeAttributeTypesForTesting(
    std::vector<AutofillFieldWithAttributeType> fields,
    EntityType entity_of_interest);

// Returns the entity types for which at least one of `fields` have a
// corresponding AttributeType.
[[nodiscard]] DenseSet<EntityType> GetRelevantEntityTypesForFields(
    base::span<const std::unique_ptr<AutofillField>> fields);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_AI_FORM_RATIONALIZATION_H_
