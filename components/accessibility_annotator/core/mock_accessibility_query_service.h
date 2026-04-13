// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_MOCK_ACCESSIBILITY_QUERY_SERVICE_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_MOCK_ACCESSIBILITY_QUERY_SERVICE_H_

#include <vector>

#include "components/accessibility_annotator/core/accessibility_query_service.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_search_result.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace accessibility_annotator {

class MockAccessibilityQueryService
    : public accessibility_annotator::AccessibilityQueryService {
 public:
  MockAccessibilityQueryService();
  ~MockAccessibilityQueryService() override;

  MOCK_METHOD(
      void,
      Query,
      (std::u16string_view query,
       bool full_search,
       base::RepeatingCallback<
           void(accessibility_annotator::MemorySearchResults)> update_callback),
      (override));
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_MOCK_ACCESSIBILITY_QUERY_SERVICE_H_
