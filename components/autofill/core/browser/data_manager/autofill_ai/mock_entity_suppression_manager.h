// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_MOCK_ENTITY_SUPPRESSION_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_MOCK_ENTITY_SUPPRESSION_MANAGER_H_

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockEntitySuppressionManager : public EntitySuppressionManager {
 public:
  MockEntitySuppressionManager();
  ~MockEntitySuppressionManager() override;

  MOCK_METHOD(void, AddObserver, (Observer * observer), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer * observer), (override));
  MOCK_METHOD(bool, SuppressEntity, (EntityInstance entity), (override));
  MOCK_METHOD(bool,
              UndoInSessionSuppressedEntity,
              (const EntityInstance::EntityId& entity_id),
              (override));
  MOCK_METHOD(bool,
              ClearSuppressions,
              (base::Time delete_begin, base::Time delete_end),
              (override));
  MOCK_METHOD(bool,
              IsSuppressed,
              (const EntityInstance& entity),
              (const, override));
  MOCK_METHOD(base::WeakPtr<syncer::DataTypeControllerDelegate>,
              GetSyncControllerDelegate,
              (),
              (override));
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_MOCK_ENTITY_SUPPRESSION_MANAGER_H_
