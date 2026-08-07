// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_ENTITY_SUPPRESSION_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_ENTITY_SUPPRESSION_MANAGER_H_

#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/keyed_service/core/keyed_service.h"

namespace autofill {

// Manages entity suggestion suppression in Autofill AI.
// Enables users to dismiss specific entity suggestions without deleting the
// underlying source data. Entity matching is based on satisfied merge
// constraints.
class EntitySuppressionManager : public KeyedService {
 public:
  ~EntitySuppressionManager() override = default;

  // Suppresses an entity by recording its satisfied merge constraints.
  // Returns true if suppression status was modified.
  virtual bool SuppressEntity(const EntityInstance& entity) = 0;

  // Removes suppression for an entity's merge constraints.
  // Returns true if suppression status was modified.
  virtual bool UnsuppressEntity(const EntityInstance& entity) = 0;

  // Returns true if the entity matches at least one suppressed merge
  // constraint.
  virtual bool IsSuppressed(const EntityInstance& entity) const = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_ENTITY_SUPPRESSION_MANAGER_H_
