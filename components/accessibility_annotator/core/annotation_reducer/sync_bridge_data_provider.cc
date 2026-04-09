// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/annotation_reducer/sync_bridge_data_provider.h"

#include <vector>

#include "base/functional/bind.h"
#include "components/accessibility_annotator/core/annotation_reducer/entry_type.h"
#include "components/accessibility_annotator/core/annotation_reducer/entry_type_util.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_search_result.h"
#include "components/accessibility_annotator/core/data_models/entity_converter.h"
#include "components/accessibility_annotator/core/storage/accessibility_annotator_backend.h"

namespace accessibility_annotator {

SyncBridgeDataProvider::SyncBridgeDataProvider(
    AccessibilityAnnotatorBackend& backend)
    : backend_(backend) {}

SyncBridgeDataProvider::~SyncBridgeDataProvider() = default;

std::string_view SyncBridgeDataProvider::GetHistogramSuffix() const {
  return "SyncBridgeDataProvider";
}

void SyncBridgeDataProvider::RetrieveAll(
    EntryType type,
    base::OnceCallback<void(std::vector<MemorySearchResult>)> callback) {
  backend_->GetSyncAnnotationsByTypes(
      GetEntityTypesForEntryType(type),
      base::BindOnce(
          [](EntryType type,
             base::OnceCallback<void(std::vector<MemorySearchResult>)> callback,
             std::vector<sync_pb::AccessibilityAnnotationSpecifics>
                 annotations) {
            std::vector<MemorySearchResult> results;
            for (const sync_pb::AccessibilityAnnotationSpecifics& specifics :
                 annotations) {
              std::optional<Entity> entity =
                  CreateEntityFromSpecifics(specifics);
              if (entity.has_value()) {
                results.push_back(CreateResultFromEntity(type, *entity));
              }
            }
            std::move(callback).Run(std::move(results));
          },
          type, std::move(callback)));
}

}  // namespace accessibility_annotator
