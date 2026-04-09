// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_SYNC_BRIDGE_DATA_PROVIDER_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_SYNC_BRIDGE_DATA_PROVIDER_H_

#include <vector>

#include "base/memory/raw_ref.h"
#include "components/accessibility_annotator/core/annotation_reducer/entry_type.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_data_provider.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_search_result.h"
#include "components/accessibility_annotator/core/data_models/entity.h"
#include "components/accessibility_annotator/core/data_models/entity_types.h"

namespace accessibility_annotator {

class AccessibilityAnnotatorBackend;

// Provides data from the sync backend and serves them in a standardized format
// suitable for
// @memory search results. Owned by AccessibilityQueryService and depends on
// AccessibilityAnnotatorBackend.
class SyncBridgeDataProvider : public MemoryDataProvider {
 public:
  explicit SyncBridgeDataProvider(AccessibilityAnnotatorBackend& backend);
  SyncBridgeDataProvider(const SyncBridgeDataProvider&) = delete;
  SyncBridgeDataProvider& operator=(const SyncBridgeDataProvider&) = delete;
  ~SyncBridgeDataProvider() override;

  // Returns all memory search results of the given type from the sync backend.
  void RetrieveAll(EntryType type,
                   base::OnceCallback<void(std::vector<MemorySearchResult>)>
                       callback) override;

  std::string_view GetHistogramSuffix() const override;

 private:
  raw_ref<AccessibilityAnnotatorBackend> backend_;
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_SYNC_BRIDGE_DATA_PROVIDER_H_
