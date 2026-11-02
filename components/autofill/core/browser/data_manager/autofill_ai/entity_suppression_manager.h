// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_ENTITY_SUPPRESSION_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_ENTITY_SUPPRESSION_MANAGER_H_

#include "components/autofill/core/browser/data_manager/autofill_ai/personal_context_entity_metadata_manager.h"

namespace autofill {

// TODO(crbug.com/478879216): Remove this compatibility class once all callers
// are migrated to `PersonalContextEntityMetadataManager`.
class EntitySuppressionManager : public PersonalContextEntityMetadataManager {
 public:
  using PersonalContextEntityMetadataManager::
      PersonalContextEntityMetadataManager;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_AUTOFILL_AI_ENTITY_SUPPRESSION_MANAGER_H_
