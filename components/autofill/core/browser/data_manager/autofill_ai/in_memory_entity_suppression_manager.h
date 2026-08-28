// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_IN_MEMORY_ENTITY_SUPPRESSION_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_IN_MEMORY_ENTITY_SUPPRESSION_MANAGER_H_

#include "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_entry.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace autofill {

// In-memory implementation of EntitySuppressionManager for testing, UI
// prototyping, and initial feature integration.
class InMemoryEntitySuppressionManager : public EntitySuppressionManager {
 public:
  InMemoryEntitySuppressionManager();
  InMemoryEntitySuppressionManager(const InMemoryEntitySuppressionManager&) =
      delete;
  InMemoryEntitySuppressionManager& operator=(
      const InMemoryEntitySuppressionManager&) = delete;
  ~InMemoryEntitySuppressionManager() override;

  // EntitySuppressionManager:
  bool SuppressEntity(const EntityInstance& entity) override;
  bool UnsuppressEntity(const EntityInstance& entity) override;
  bool IsSuppressed(const EntityInstance& entity) const override;

 private:
  // Set storing representations of suppressed merge constraint sets.
  absl::flat_hash_set<EntitySuppressionEntry> suppressed_entries_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_IN_MEMORY_ENTITY_SUPPRESSION_MANAGER_H_
