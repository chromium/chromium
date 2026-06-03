// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_MOCK_PERSONAL_CONTEXT_ACCESS_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_MOCK_PERSONAL_CONTEXT_ACCESS_MANAGER_H_

#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/network/autofill_ai/personal_context_access_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockPersonalContextAccessManager : public PersonalContextAccessManager {
 public:
  MockPersonalContextAccessManager();
  ~MockPersonalContextAccessManager() override;

  MOCK_METHOD(void,
              PrefetchAmbientAutofillContext,
              (base::span<const EntityType> requested_types),
              (override));
  MOCK_METHOD(std::optional<EntityInstance>,
              GetCachedEntity,
              (const EntityInstance::EntityId& id),
              (const, override));
  MOCK_METHOD(void,
              GetUnmaskedSpiiEntity,
              (const EntityInstance::EntityId& id,
               GetUnmaskedSpiiEntityCallback callback),
              (override));
  MOCK_METHOD(std::vector<EntityInstance>,
              GetCachedEntities,
              (),
              (const, override));
  MOCK_METHOD(bool,
              IsTypeCached,
              (EntityTypeName type_name),
              (const, override));
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_MOCK_PERSONAL_CONTEXT_ACCESS_MANAGER_H_
