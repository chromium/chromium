// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AT_MEMORY_AT_MEMORY_QUERY_SERVICE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AT_MEMORY_AT_MEMORY_QUERY_SERVICE_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/integrators/at_memory/at_memory_eligibility_metrics_tracker.h"
#include "components/autofill/core/browser/integrators/at_memory/memory_search_result.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/personal_context/proto/features/at_memory.pb.h"
#include "url/gurl.h"

class PrefService;

namespace personal_context {
class PersonalContextEligibilityService;
class PersonalContextService;
}

namespace subscription_eligibility {
class SubscriptionEligibilityService;
}

namespace autofill {

class AutofillDataProvider;
class LogManager;
class LogRouter;

// Service for querying AtMemory suggestions. Owned by the Profile, one per
// profile.
class AtMemoryQueryService : public KeyedService {
 public:
  AtMemoryQueryService(
      std::unique_ptr<AutofillDataProvider> data_provider,
      personal_context::PersonalContextService* personal_context_service,
      const std::string& locale,
      personal_context::PersonalContextEligibilityService*
          personal_context_eligibility_service,
      subscription_eligibility::SubscriptionEligibilityService*
          subscription_eligibility_service,
      PrefService* pref_service,
      LogRouter* log_router);
  AtMemoryQueryService(const AtMemoryQueryService&) = delete;
  AtMemoryQueryService& operator=(const AtMemoryQueryService&) = delete;
  ~AtMemoryQueryService() override;

  // KeyedService:
  void Shutdown() override;

  // Executes a server query, using user provided `query` and returns search
  // results via `callback`.
  virtual void Query(
      std::u16string_view query,
      const GURL& url,
      std::u16string_view title,
      base::RepeatingCallback<void(MemorySearchResults)> callback);

 private:
  // Called when the PersonalContextService query returns.
  void OnPersonalContextRetrieved(
      base::RepeatingCallback<void(MemorySearchResults)> callback,
      personal_context::FetchContextResult result);

  // Called when the local data provider finishes retrieving local memory
  // entries specified in the fetch plan. It filters these local entries,
  // ranks them with the remote results, deduplicates them, and reports the
  // final results via `callback`.
  void OnLocalDataRetrieved(
      base::RepeatingCallback<void(MemorySearchResults)> callback,
      std::vector<MemorySearchResult> remote_results,
      std::vector<personal_context::proto::AutofillFetchSpecification>
          fetch_specifications,
      std::string server_request_id,
      std::vector<MemorySearchResult> local_results);

  std::unique_ptr<LogManager> log_manager_;
  std::unique_ptr<AutofillDataProvider> data_provider_;
  raw_ptr<personal_context::PersonalContextService> personal_context_service_ =
      nullptr;
  std::string locale_;
  AtMemoryEligibilityMetricsTracker eligibility_metrics_tracker_;
  base::WeakPtrFactory<AtMemoryQueryService> query_weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AT_MEMORY_AT_MEMORY_QUERY_SERVICE_H_
