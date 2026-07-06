// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AT_MEMORY_MOCK_AT_MEMORY_QUERY_SERVICE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AT_MEMORY_MOCK_AT_MEMORY_QUERY_SERVICE_H_

#include <vector>

#include "components/accessibility_annotator/core/annotation_reducer/memory_search_result.h"
#include "components/autofill/core/browser/integrators/at_memory/at_memory_query_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockAtMemoryQueryService : public AtMemoryQueryService {
 public:
  MockAtMemoryQueryService();
  ~MockAtMemoryQueryService() override;

  MOCK_METHOD(
      void,
      Query,
      (std::u16string_view query,
       base::RepeatingCallback<
           void(accessibility_annotator::MemorySearchResults)> update_callback),
      (override));
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AT_MEMORY_MOCK_AT_MEMORY_QUERY_SERVICE_H_
