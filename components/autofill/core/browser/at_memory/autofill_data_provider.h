// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AUTOFILL_DATA_PROVIDER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AUTOFILL_DATA_PROVIDER_H_

#include <string_view>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/integrators/at_memory/memory_data_type.h"
#include "components/autofill/core/browser/integrators/at_memory/memory_search_result.h"

namespace autofill {

// Provides data from various Autofill backends (e.g. addresses, payments,
// Autofill AI entities) and serves them in a standardized format suitable for
// @memory search results.
class AutofillDataProvider {
 public:
  AutofillDataProvider(const PersonalDataManager* personal_data_manager,
                       const EntityDataManager* entity_data_manager);
  AutofillDataProvider(const AutofillDataProvider&) = delete;
  AutofillDataProvider& operator=(const AutofillDataProvider&) = delete;
  virtual ~AutofillDataProvider();

  // Retrieves all data entries for the given entry types.
  virtual void RetrieveAll(
      const std::vector<MemoryDataType>& types,
      base::OnceCallback<void(std::vector<MemorySearchResult>)> callback);

 private:
  // Retrieves all entities for a given Autofill data type.
  std::vector<MemorySearchResult> GetAutofillData(
      MemoryDataType memory_data_type);

  // Fetches IBAN data from `personal_data_manager_`.
  std::vector<MemorySearchResult> FetchIbanData();

  // Fetches credit card data from `personal_data_manager_`.
  std::vector<MemorySearchResult> FetchCreditCardData(
      FieldType field_type,
      MemoryDataType memory_data_type);

  raw_ptr<const PersonalDataManager> personal_data_manager_;
  raw_ptr<const EntityDataManager> entity_data_manager_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AUTOFILL_DATA_PROVIDER_H_
