// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PROCESSING_AUTOFILL_AI_DETERMINE_ATTRIBUTE_TYPES_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PROCESSING_AUTOFILL_AI_DETERMINE_ATTRIBUTE_TYPES_H_

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"

namespace autofill {

class AutofillField;
class Section;

struct AutofillFieldWithAttributeType {
  AutofillFieldWithAttributeType(const AutofillField& field LIFETIME_BOUND,
                                 AttributeType type)
      : field(field), type(type) {}

  raw_ref<const AutofillField> field;
  AttributeType type;
};

// DetermineAttributeTypes() computes the static and dynamic AttributeType
// assignments of a form.
//
// Static AttributeTypes are determined by the Autofill AI FieldType
// (AutofillField::GetAutofillAiServerTypePredictions()).
// Every field has at most one static AttributeType.
//
// Dynamic types are determined by looking at the surrounding fields.
// (AutofillField::GetAutofillAiServerTypePredictions()).
// Every field has at most one static AttributeType.
//
// Dynamic types are only determined if `features::kAutofillAiNoTagTypes` is
// enabled.
//
// The overloads are just specializations of one another for performance
// reasons. The following expressions are equivalent:
// - `DetermineAttributeTypes(fields, section, entity)`
// - `DetermineAttributeTypes(fields, section)[entity]`
// - `DetermineAttributeTypes(fields)[section][entity]`

std::vector<AutofillFieldWithAttributeType> DetermineAttributeTypes(
    base::span<const std::unique_ptr<AutofillField>> fields LIFETIME_BOUND,
    const Section& section_of_interest,
    EntityType entity_of_interest);

base::flat_map<EntityType, std::vector<AutofillFieldWithAttributeType>>
DetermineAttributeTypes(base::span<const std::unique_ptr<AutofillField>> fields
                            LIFETIME_BOUND,
                        const Section& section_of_interest);

base::flat_map<
    Section,
    base::flat_map<EntityType, std::vector<AutofillFieldWithAttributeType>>>
DetermineAttributeTypes(
    base::span<const std::unique_ptr<AutofillField>> fields LIFETIME_BOUND);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PROCESSING_AUTOFILL_AI_DETERMINE_ATTRIBUTE_TYPES_H_
