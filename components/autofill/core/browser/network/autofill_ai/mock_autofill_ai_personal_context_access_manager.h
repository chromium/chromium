// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_MOCK_AUTOFILL_AI_PERSONAL_CONTEXT_ACCESS_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_MOCK_AUTOFILL_AI_PERSONAL_CONTEXT_ACCESS_MANAGER_H_

#include "base/containers/span.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/network/autofill_ai/autofill_ai_personal_context_access_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockAutofillAiPersonalContextAccessManager
    : public AutofillAiPersonalContextAccessManager {
 public:
  MockAutofillAiPersonalContextAccessManager();
  ~MockAutofillAiPersonalContextAccessManager() override;

  MOCK_METHOD(void,
              PrefetchContext,
              (base::span<const EntityType> requested_types),
              (override));
  MOCK_METHOD(void,
              GetUnmaskedSpiiEntity,
              (const EntityInstance::EntityId& id,
               GetUnmaskedSpiiEntityCallback callback),
              (override));
  MOCK_METHOD(bool, IsTypePrefetched, (EntityType type), (const, override));
  MOCK_METHOD(void, AddObserver, (Observer * observer), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer * observer), (override));
  MOCK_METHOD(RequestStatus,
              GetPrefetchStatusByEntityType,
              (EntityType type),
              (const, override));
  MOCK_METHOD(bool,
              ServerHasDataAvailable,
              (EntityType type),
              (const, override));
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_MOCK_AUTOFILL_AI_PERSONAL_CONTEXT_ACCESS_MANAGER_H_
