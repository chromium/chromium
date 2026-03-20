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
#include "components/accessibility_annotator/core/annotation_reducer/memory_search_result.h"
#include "components/accessibility_annotator/core/annotation_reducer/query_classifier.h"
#include "components/keyed_service/core/keyed_service.h"

namespace accessibility_annotator {

class AutofillDataProvider;

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

  // Executes a query and returns suggestions via `update_callback`.
  virtual void Query(
      std::u16string_view query,
      base::RepeatingCallback<void(std::vector<MemorySearchResult>)>
          update_callback);

 private:
  std::unique_ptr<AutofillDataProvider> data_provider_;
  QueryClassifier classifier_;
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_QUERY_SERVICE_H_
