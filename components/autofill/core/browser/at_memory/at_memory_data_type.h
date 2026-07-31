// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_DATA_TYPE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_DATA_TYPE_H_

#include <optional>
#include <variant>

#include "components/autofill/core/browser/at_memory/at_memory_enablement_utils.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/integrators/at_memory/memory_data_type.h"

namespace autofill {

// Represents the semantic type of a user's @memory query, identifying the
// specific type of requested information, either referring to a specific
// attribute or to a broader category (e.g. a vehicle, or vehicle's VIN).
using AtMemoryDataType = std::variant<FieldType, AttributeType>;

// Translates a MemoryDataType to an Autofill-specific data type.
std::optional<AtMemoryDataType> ToAtMemoryDataType(
    MemoryDataType memory_data_type);

// Maps AtMemoryDataType to AutofillPolicyDataCategory.
std::optional<AutofillClient::AutofillPolicyDataCategory>
ToAutofillPolicyDataCategory(const AtMemoryDataType& type);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_DATA_TYPE_H_
