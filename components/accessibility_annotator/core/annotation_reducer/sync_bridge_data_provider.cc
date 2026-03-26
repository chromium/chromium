// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/annotation_reducer/sync_bridge_data_provider.h"

#include <vector>

#include "components/accessibility_annotator/core/annotation_reducer/memory_search_result.h"
#include "components/accessibility_annotator/core/annotation_reducer/query_intent_type.h"
#include "components/accessibility_annotator/core/annotation_reducer/query_intent_type_util.h"
#include "components/accessibility_annotator/core/data_models/entity_converter.h"
#include "components/accessibility_annotator/core/storage/accessibility_annotator_backend.h"

namespace accessibility_annotator {

SyncBridgeDataProvider::SyncBridgeDataProvider(
    AccessibilityAnnotatorBackend& backend)
    : backend_(backend) {}

SyncBridgeDataProvider::~SyncBridgeDataProvider() = default;

void SyncBridgeDataProvider::RetrieveAll(
    QueryIntentType type,
    base::OnceCallback<void(std::vector<MemorySearchResult>)> callback) {
  AccessibilityAnnotationSyncBridge* bridge =
      backend_->accessibility_annotation_sync_bridge();
  if (!bridge) {
    std::move(callback).Run({});
    return;
  }

  EntityTypeEnumSet types = GetEntityTypesForQueryIntentType(type);
  std::vector<MemorySearchResult> results;
  for (const sync_pb::AccessibilityAnnotationSpecifics& specifics :
       bridge->GetAnnotationsByTypes(types)) {
    std::optional<Entity> entity = CreateEntityFromSpecifics(specifics);
    if (entity.has_value()) {
      results.push_back(CreateResultFromEntity(type, *entity));
    }
  }
  std::move(callback).Run(std::move(results));
}

}  // namespace accessibility_annotator
