// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_DATA_TYPE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_DATA_TYPE_H_

#include <optional>
#include <variant>

#include "components/accessibility_annotator/annotation_reducer/query_intent_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

// Represents the semantic type of a user's @memory query, identifying the
// specific type of requested information, either referring to a specific
// attribute or to a broader category (e.g. a vehicle, or vehicle's VIN).
// This decouples the query engine from specific data sources by using a
// variant of existing, well-defined types.
using AtMemoryDataType = std::variant<FieldType, EntityType, AttributeType>;

// Converts an annotation_reducer::QueryIntentType to an AtMemoryDataType.
std::optional<AtMemoryDataType> ToAtMemoryDataType(
    annotation_reducer::QueryIntentType query_intent_type);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_DATA_TYPE_H_
