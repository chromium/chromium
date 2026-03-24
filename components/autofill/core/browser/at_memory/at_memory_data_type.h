// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_DATA_TYPE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_DATA_TYPE_H_

#include <optional>
#include <variant>

#include "components/accessibility_annotator/core/annotation_reducer/query_intent_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

// Represents the semantic type of a user's @memory query, identifying the
// specific type of requested information, either referring to a specific
// attribute or to a broader category (e.g. a vehicle, or vehicle's VIN).
using AtMemoryDataType = std::variant<FieldType, EntityType, AttributeType>;

// Translates a query intent from the accessibility annotator to an
// Autofill-specific data type.
std::optional<AtMemoryDataType> ToAtMemoryDataType(
    accessibility_annotator::QueryIntentType intent_type);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_DATA_TYPE_H_
