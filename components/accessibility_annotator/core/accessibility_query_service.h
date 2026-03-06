// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_QUERY_SERVICE_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_QUERY_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "components/accessibility_annotator/core/annotation_reducer/memory_search_result.h"
#include "components/keyed_service/core/keyed_service.h"

namespace accessibility_annotator {

class AutofillDataProvider;
class QueryClassifier;

// Service for querying @memory suggestions.
class AccessibilityQueryService : public KeyedService {
 public:
  explicit AccessibilityQueryService(
      std::unique_ptr<AutofillDataProvider> data_provider);
  AccessibilityQueryService(const AccessibilityQueryService&) = delete;
  AccessibilityQueryService& operator=(const AccessibilityQueryService&) =
      delete;
  ~AccessibilityQueryService() override;

  // KeyedService:
  void Shutdown() override;

  // Executes a query and returns suggestions.
  virtual std::vector<MemorySearchResult> Query(const std::u16string& query);

 private:
  std::unique_ptr<AutofillDataProvider> data_provider_;
  std::unique_ptr<QueryClassifier> classifier_;
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_QUERY_SERVICE_H_
