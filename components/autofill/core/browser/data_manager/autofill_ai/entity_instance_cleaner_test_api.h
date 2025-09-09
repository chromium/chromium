// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_ENTITY_INSTANCE_CLEANER_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_ENTITY_INSTANCE_CLEANER_TEST_API_H_

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_instance_cleaner.h"

namespace autofill {

class EntityInstanceCleanerTestApi {
 public:
  explicit EntityInstanceCleanerTestApi(EntityInstanceCleaner* cleaner)
      : cleaner_(*cleaner) {}

  bool AreCleanupsPending() const { return cleaner_->are_cleanups_pending_; }

  void MaybeCleanupLocalEntityInstancesData() {
    cleaner_->MaybeCleanupLocalEntityInstancesData();
  }

 private:
  const raw_ref<EntityInstanceCleaner> cleaner_;
};

inline EntityInstanceCleanerTestApi test_api(EntityInstanceCleaner& cleaner) {
  return EntityInstanceCleanerTestApi(&cleaner);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_ENTITY_INSTANCE_CLEANER_TEST_API_H_
