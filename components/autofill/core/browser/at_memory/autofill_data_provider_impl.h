// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AUTOFILL_DATA_PROVIDER_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AUTOFILL_DATA_PROVIDER_IMPL_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/accessibility_annotator/core/annotation_reducer/autofill_data_provider.h"
#include "components/autofill/core/browser/at_memory/at_memory_data_type.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"

namespace autofill {

// Provides data from various Autofill backends (e.g. addresses, payments,
// Autofill AI entities) and serves them in a standardized format suitable for
// @memory search results.
class AutofillDataProviderImpl
    : public accessibility_annotator::AutofillDataProvider {
 public:
  AutofillDataProviderImpl(const PersonalDataManager* personal_data_manager,
                           const EntityDataManager* entity_data_manager);
  AutofillDataProviderImpl(const AutofillDataProviderImpl&) = delete;
  AutofillDataProviderImpl& operator=(const AutofillDataProviderImpl&) = delete;
  ~AutofillDataProviderImpl() override;

  // accessibility_annotator::AutofillDataProvider:
  std::vector<accessibility_annotator::MemorySearchResult> RetrieveAll(
      accessibility_annotator::QueryIntentType type) override;

 private:
  // Retrieves all entities for a given Autofill data type.
  std::vector<accessibility_annotator::MemorySearchResult> GetAutofillData(
      AtMemoryDataType type);

  raw_ptr<const PersonalDataManager> personal_data_manager_;
  raw_ptr<const EntityDataManager> entity_data_manager_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AUTOFILL_DATA_PROVIDER_IMPL_H_
