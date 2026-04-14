// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/annotation_reducer/sync_bridge_data_provider.h"

#include <algorithm>
#include <vector>

#include "base/functional/bind.h"
#include "components/accessibility_annotator/core/annotation_reducer/entry_type.h"
#include "components/accessibility_annotator/core/annotation_reducer/entry_type_util.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_search_result.h"
#include "components/accessibility_annotator/core/data_models/entity_converter.h"
#include "components/accessibility_annotator/core/storage/accessibility_annotator_backend.h"
#include "components/sync/protocol/accessibility_annotation_specifics.pb.h"

namespace accessibility_annotator {
namespace {

void SortAnnotationsByTimestamp(
    std::vector<sync_pb::AccessibilityAnnotationSpecifics>& annotations) {
  std::sort(
      annotations.begin(), annotations.end(),
      [](const sync_pb::AccessibilityAnnotationSpecifics& a,
         const sync_pb::AccessibilityAnnotationSpecifics& b) {
        auto get_timestamp = [](const sync_pb::AccessibilityAnnotationSpecifics&
                                    specifics) {
          int64_t max_timestamp = 0;
          for (const auto& source : specifics.sources()) {
            switch (source.source_case()) {
              case sync_pb::AccessibilityAnnotationSpecifics::Source::
                  kGmailSource:
                max_timestamp = std::max(
                    max_timestamp,
                    source.gmail_source().received_time_unix_epoch_seconds());
                break;
              case sync_pb::AccessibilityAnnotationSpecifics::Source::
                  kCalendarSource:
                max_timestamp = std::max(
                    max_timestamp, source.calendar_source()
                                       .modified_time_unix_epoch_seconds());
                break;
              case sync_pb::AccessibilityAnnotationSpecifics::Source::
                  kPhotosSource:
                max_timestamp = std::max(
                    max_timestamp,
                    source.photos_source().creation_time_unix_epoch_seconds());
                break;
              case sync_pb::AccessibilityAnnotationSpecifics::Source::
                  SOURCE_NOT_SET:
                break;
            }
          }
          return max_timestamp;
        };
        return get_timestamp(a) > get_timestamp(b);
      });
}

}  // namespace

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
            SortAnnotationsByTimestamp(annotations);

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
