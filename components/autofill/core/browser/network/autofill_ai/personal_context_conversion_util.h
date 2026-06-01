// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_PERSONAL_CONTEXT_CONVERSION_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_PERSONAL_CONTEXT_CONVERSION_UTIL_H_

#include <optional>

namespace personal_context::proto {
class Entity;
}  // namespace personal_context::proto

namespace autofill {

class EntityInstance;

// Converts a generic `personal_context::proto::Entity` to an `EntityInstance`.
std::optional<EntityInstance> PersonalContextEntityToEntityInstance(
    const personal_context::proto::Entity& entity);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_PERSONAL_CONTEXT_CONVERSION_UTIL_H_
