// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AT_MEMORY_AT_MEMORY_QUERY_SERVICE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AT_MEMORY_AT_MEMORY_QUERY_SERVICE_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_search_result.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/personal_context/core/context_memory_error.h"
#include "components/personal_context/core/personal_context_types.h"

namespace personal_context {
class PersonalContextService;
}

namespace autofill {

class AutofillDataProvider;
class AtMemoryQueryServiceDelegate;

// Service for querying @memory suggestions. Owned by the Profile, one per
// profile.
class AtMemoryQueryService : public KeyedService {
 public:
  AtMemoryQueryService(
      std::unique_ptr<AtMemoryQueryServiceDelegate> delegate,
      std::unique_ptr<AutofillDataProvider> data_provider,
      personal_context::PersonalContextService* personal_context_service,
      const std::string& locale);
  AtMemoryQueryService(const AtMemoryQueryService&) = delete;
  AtMemoryQueryService& operator=(const AtMemoryQueryService&) = delete;
  ~AtMemoryQueryService() override;

  // KeyedService:
  void Shutdown() override;

  // Executes a server query, using user provided `query` and returns search
  // results via `callback`.
  virtual void Query(
      std::u16string_view query,
      base::RepeatingCallback<
          void(accessibility_annotator::MemorySearchResults)> callback);

 private:
  // Called when the PersonalContextService query returns.
  void OnPersonalContextRetrieved(
      base::RepeatingCallback<
          void(accessibility_annotator::MemorySearchResults)> callback,
      personal_context::FetchContextResult result);

  // Called when the local data provider finishes retrieving local memory
  // entries specified in the fetch plan. It filters these local entries,
  // ranks them with the remote results, deduplicates them, and reports the
  // final results via `callback`.
  void OnLocalDataRetrieved(
      base::RepeatingCallback<
          void(accessibility_annotator::MemorySearchResults)> callback,
      std::vector<accessibility_annotator::MemorySearchResult> remote_results,
      base::flat_set<std::u16string> filter_words,
      std::string server_request_id,
      std::vector<accessibility_annotator::MemorySearchResult> local_results);
  std::unique_ptr<AtMemoryQueryServiceDelegate> delegate_;
  std::unique_ptr<AutofillDataProvider> data_provider_;
  raw_ptr<personal_context::PersonalContextService> personal_context_service_ =
      nullptr;
  std::string locale_;

  base::WeakPtrFactory<AtMemoryQueryService> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AT_MEMORY_AT_MEMORY_QUERY_SERVICE_H_
