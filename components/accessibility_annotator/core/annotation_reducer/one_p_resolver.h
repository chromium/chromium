// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_ONE_P_RESOLVER_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_ONE_P_RESOLVER_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_search_result.h"

namespace accessibility_annotator {

// Interface for resolving data from 1P source.
class OnePResolver {
 public:
  using QueryCallback =
      base::OnceCallback<void(std::vector<MemorySearchResult>)>;

  virtual ~OnePResolver() = default;

  // Retrieves accessibility annotations from the 1P service for a given
  // query and resolves it into memory search results.
  virtual void Query(const std::u16string& query, QueryCallback callback) = 0;
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_ONE_P_RESOLVER_H_
