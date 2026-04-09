// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_MEMORY_DATA_PROVIDER_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_MEMORY_DATA_PROVIDER_H_

#include <vector>

#include "base/functional/callback.h"
#include "components/accessibility_annotator/core/annotation_reducer/entry_type.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_search_result.h"

namespace accessibility_annotator {

// Interface for providing data from various backends such as Autofill (e.g.
// addresses, payments, Autofill AI entities) and serves them in a standardized
// format suitable for @memory search results.
class MemoryDataProvider {
 public:
  virtual ~MemoryDataProvider() = default;

  // Retrieves all data entries for a given entry type.
  virtual void RetrieveAll(
      EntryType type,
      base::OnceCallback<void(std::vector<MemorySearchResult>)> callback) = 0;

  // Returns a unique suffix to identify the provider in histograms. If this
  // value is changed, the histogram variant
  // "AccessibilityAnnotator.MemoryDataProvider" in
  // tools/metrics/histograms/metadata/accessibility_annotator/histograms.xml
  // should also be updated.
  virtual std::string_view GetHistogramSuffix() const = 0;
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_MEMORY_DATA_PROVIDER_H_
