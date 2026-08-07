// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_IN_MEMORY_ENTITY_SUPPRESSION_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_IN_MEMORY_ENTITY_SUPPRESSION_MANAGER_H_

#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"

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
  // Constructs representation keys for all satisfied merge constraint sets of
  // the given entity.
  std::vector<std::string> GetCanonicalStrings(
      const EntityInstance& entity) const;

  // Set storing representations of suppressed merge constraint sets.
  base::flat_set<std::string> suppressed_keys_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_IN_MEMORY_ENTITY_SUPPRESSION_MANAGER_H_
