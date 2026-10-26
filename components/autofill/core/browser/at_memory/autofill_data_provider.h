// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AUTOFILL_DATA_PROVIDER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AUTOFILL_DATA_PROVIDER_H_

#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/types/optional_ref.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/integrators/at_memory/memory_data_type.h"
#include "components/autofill/core/browser/integrators/at_memory/memory_search_result.h"

namespace autofill {

struct RawValueQuery;
class EntityDataManager;
class PersonalDataManager;
class ValuablesDataManager;

// Provides data from various Autofill backends (e.g. addresses, payments,
// Autofill AI entities, loyalty cards) and serves them in a standardized format
// suitable for AtMemory search results.
class AutofillDataProvider {
 public:
  AutofillDataProvider(const PersonalDataManager* personal_data_manager,
                       const EntityDataManager* entity_data_manager,
                       const ValuablesDataManager* valuables_data_manager);
  AutofillDataProvider(const AutofillDataProvider&) = delete;
  AutofillDataProvider& operator=(const AutofillDataProvider&) = delete;
  virtual ~AutofillDataProvider();

  // Retrieves all data entries for the given entry types.
  // TODO(crbug.com/566991902): Return the entries by value instead of taking a
  // callback, since retrieval is synchronous.
  virtual void RetrieveAll(
      const std::vector<MemoryDataType>& types,
      base::OnceCallback<void(std::vector<MemorySearchResult>)> callback);

  // Retrieves all data entries whose raw value matches `query` (content-based
  // search, see `MatchesRawValueQuery()`). A record yields an entry per
  // matching type; these are ordered by the types' priority, codes first.
  virtual void RetrieveAllByContent(
      std::u16string_view query,
      base::OnceCallback<void(std::vector<MemorySearchResult>)> callback);

 private:
  // Retrieves the entries of `memory_data_types`, sorted by confidence. If
  // `query` is set, only entries whose raw value matches it are returned.
  std::vector<MemorySearchResult> GetAutofillData(
      base::span<const MemoryDataType> memory_data_types,
      base::optional_ref<const RawValueQuery> query);

  // Fetches IBAN data from `personal_data_manager_`. If `query` is set,
  // only local IBANs whose value matches it are returned.
  std::vector<MemorySearchResult> FetchIbanData(
      base::optional_ref<const RawValueQuery> query);

  // Fetches credit card data from `personal_data_manager_`. If `query`
  // is set, only local cards whose raw value matches it are returned.
  std::vector<MemorySearchResult> FetchCreditCardData(
      FieldType field_type,
      MemoryDataType memory_data_type,
      base::optional_ref<const RawValueQuery> query);

  raw_ptr<const PersonalDataManager> personal_data_manager_;
  raw_ptr<const EntityDataManager> entity_data_manager_;
  raw_ptr<const ValuablesDataManager> valuables_data_manager_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AUTOFILL_DATA_PROVIDER_H_
