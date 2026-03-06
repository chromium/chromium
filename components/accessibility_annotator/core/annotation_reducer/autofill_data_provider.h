// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_AUTOFILL_DATA_PROVIDER_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_AUTOFILL_DATA_PROVIDER_H_

#include <vector>

#include "components/accessibility_annotator/core/annotation_reducer/memory_search_result.h"
#include "components/accessibility_annotator/core/annotation_reducer/query_intent_type.h"

namespace accessibility_annotator {

// Interface for providing data from various Autofill backends (e.g.
// addresses, payments, Autofill AI entities) and serves them in a standardized
// format suitable for @memory search results.
class AutofillDataProvider {
 public:
  virtual ~AutofillDataProvider() = default;

  // Retrieves all data entries for a given query intent type.
  virtual std::vector<MemorySearchResult> RetrieveAll(QueryIntentType type) = 0;
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_AUTOFILL_DATA_PROVIDER_H_
