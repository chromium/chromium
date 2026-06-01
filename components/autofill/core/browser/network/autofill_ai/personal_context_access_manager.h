// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_PERSONAL_CONTEXT_ACCESS_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_PERSONAL_CONTEXT_ACCESS_MANAGER_H_

#include <string>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/personal_context/core/context_memory_error.h"

namespace autofill {

class EntityType;

// Manages access to personal context data for autofill.
// Instantiated once per profile/context.
class PersonalContextAccessManager : public KeyedService {
 public:
  // Callback invoked when the ambient autofill context fetch is complete.
  using FetchAmbientAutofillContextCallback = base::OnceCallback<void(
      base::expected<std::string /*serialized_response*/,
                     personal_context::ContextMemoryError>)>;

  ~PersonalContextAccessManager() override = default;

  // Fetches ambient autofill context from the personal context service.
  virtual void FetchAmbientAutofillContext(
      base::span<const EntityType> requested_types,
      FetchAmbientAutofillContextCallback callback) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_PERSONAL_CONTEXT_ACCESS_MANAGER_H_
