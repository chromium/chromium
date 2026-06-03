// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_PERSONAL_CONTEXT_CONVERSION_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_PERSONAL_CONTEXT_CONVERSION_UTIL_H_

#include <optional>

#include "components/personal_context/proto/features/common_data.pb.h"

namespace autofill {

class EntityInstance;
class EntityType;

// Converts an Autofill AI EntityType to a Personal Context proto EntityType.
personal_context::proto::EntityType
AutofillEntityTypeToPersonalContextEntityType(EntityType type);

// Converts a generic `personal_context::proto::Entity` to an `EntityInstance`.
// If `is_masked` is true, sensitive attributes (passport, drivers license, and
// national ID number) are marked as masked. Otherwise, they are kept unmasked.
std::optional<EntityInstance> PersonalContextEntityToEntityInstance(
    const personal_context::proto::Entity& entity,
    bool is_masked = true);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_PERSONAL_CONTEXT_CONVERSION_UTIL_H_
