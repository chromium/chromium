// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_QUERY_SERVICE_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_QUERY_SERVICE_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/accessibility_annotator/core/accessibility_query_service_delegate.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_data_provider.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_search_result.h"
#include "components/accessibility_annotator/core/annotation_reducer/one_p_resolver.h"
#include "components/accessibility_annotator/core/annotation_reducer/query_classifier.h"
#include "components/keyed_service/core/keyed_service.h"

namespace optimization_guide {
class RemoteModelExecutor;
}

namespace accessibility_annotator {

class MemoryDataProvider;

// Service for querying @memory suggestions.
class AccessibilityQueryService : public KeyedService {
 public:
  AccessibilityQueryService(
      std::unique_ptr<AccessibilityQueryServiceDelegate> delegate,
      std::vector<std::unique_ptr<MemoryDataProvider>> data_providers,
      std::unique_ptr<OnePResolver> one_p_resolver,
      optimization_guide::RemoteModelExecutor* remote_model_executor);
  AccessibilityQueryService(const AccessibilityQueryService&) = delete;
  AccessibilityQueryService& operator=(const AccessibilityQueryService&) =
      delete;
  ~AccessibilityQueryService() override;

  // KeyedService:
  void Shutdown() override;

  // Executes a query and returns suggestions via `update_callback`.
  // @param query The search string provided by the user.
  // @param update_callback Invoked with search results. May be called multiple
  // times for streaming updates, providing results from different data sources.
  virtual void Query(
      std::u16string_view query,
      base::RepeatingCallback<void(MemorySearchResults)> update_callback);

 private:
  void OnClassificationComplete(
      std::u16string query,
      base::RepeatingCallback<void(MemorySearchResults)> update_callback,
      ClassifiedQuery classified_query);

  void OnDataRetrieved(
      std::u16string query,
      ClassifiedQuery classified_query,
      base::RepeatingCallback<void(MemorySearchResults)> update_callback,
      std::vector<std::vector<MemorySearchResult>> entries_list);

  void QueryOnePResolver(
      std::u16string query,
      base::RepeatingCallback<void(MemorySearchResults)> update_callback,
      std::vector<MemorySearchResult> fallback_entries,
      MemorySearchStatus fallback_status);

  void OnOnePResolverComplete(
      base::RepeatingCallback<void(MemorySearchResults)> update_callback,
      std::vector<MemorySearchResult> fallback_entries,
      MemorySearchStatus fallback_status,
      std::vector<MemorySearchResult> one_p_entries);

  std::unique_ptr<AccessibilityQueryServiceDelegate> delegate_;
  std::vector<std::unique_ptr<MemoryDataProvider>> data_providers_;
  std::unique_ptr<OnePResolver> one_p_resolver_;
  QueryClassifier classifier_;

  base::WeakPtrFactory<AccessibilityQueryService> weak_ptr_factory_{this};
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_QUERY_SERVICE_H_
