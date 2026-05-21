// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_UTILS_H_

#include <optional>

#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"

namespace autofill {

class EntityInstance;

// Returns the type of the primary attribute of `entity`, falling back to the
// first non-empty attribute if the primary attribute is missing or empty.
//
// A "primary" attribute is defined to resolve search-and-fill ambiguity when a
// search matches the overall entity (e.g., a query for "passport" matching a
// Passport `EntityType`) rather than a specific attribute (e.g., "passport
// number" matching the passport number attribute value, or "passport name"
// matching the passport name attribute value). In such cases, a single most
// useful attribute is chosen.
std::optional<AttributeType> GetPrimaryAttributeType(
    const EntityInstance& entity);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_UTILS_H_
