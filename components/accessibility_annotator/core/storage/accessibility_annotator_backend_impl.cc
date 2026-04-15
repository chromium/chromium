// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/storage/accessibility_annotator_backend_impl.h"

#include <algorithm>
#include <vector>

#include "base/containers/lru_cache.h"
#include "base/containers/map_util.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/notimplemented.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"
#include "components/accessibility_annotator/core/storage/accessibility_annotation_sync_bridge.h"
#include "components/accessibility_annotator/core/storage/accessibility_annotator_database.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/optimization_guide/proto/features/content_annotation.to_value.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace accessibility_annotator {

namespace {

std::string GetEtldPlusOne(const GURL& url) {
  return net::registry_controlled_domains::GetDomainAndRegistry(
      url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

}  // namespace

AccessibilityAnnotatorBackendImpl::AccessibilityAnnotatorBackendImpl(
    history::HistoryService* history_service,
    syncer::RepeatingDataTypeStoreFactory data_type_store_factory,
    const base::FilePath& db_path)
    : db_path_(db_path),
      db_(base::ThreadPool::CreateSequencedTaskRunnerForResource(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
          db_path_)),
      content_annotations_cache_(
          features::kContentAnnotatorMaxCacheAnnotations.Get()) {
  auto processor = std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
      syncer::ACCESSIBILITY_ANNOTATION,
      /*dump_stack=*/base::DoNothing());
  accessibility_annotation_sync_bridge_ =
      std::make_unique<AccessibilityAnnotationSyncBridge>(
          std::move(processor), data_type_store_factory);
  sync_bridge_observation_.Observe(accessibility_annotation_sync_bridge_.get());
  if (history_service) {
    history_service_observation_.Observe(history_service);
  }
}

AccessibilityAnnotatorBackendImpl::~AccessibilityAnnotatorBackendImpl() =
    default;

void AccessibilityAnnotatorBackendImpl::Shutdown() {
  history_service_observation_.Reset();
}

void AccessibilityAnnotatorBackendImpl::Init() {
  db_.AsyncCall(&AccessibilityAnnotatorDatabase::Init)
      .WithArgs(db_path_)
      .Then(base::BindOnce([](bool status) {
        if (!status) {
          // TODO(crbug.com/489690454): Replace this with a non-local histogram
          // once metrics are finalized and setup as needed.
          LOCAL_HISTOGRAM_BOOLEAN("AccessibilityAnnotator.DatabaseInitFailed",
                                  true);
        }
      }));
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
AccessibilityAnnotatorBackendImpl::
    GetAccessibilityAnnotationControllerDelegate() {
  return accessibility_annotation_sync_bridge_->change_processor()
      ->GetControllerDelegate();
}

void AccessibilityAnnotatorBackendImpl::AddObserver(
    AccessibilityAnnotatorBackend::Observer* observer) {
  observers_.AddObserver(observer);
}

void AccessibilityAnnotatorBackendImpl::RemoveObserver(
    AccessibilityAnnotatorBackend::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AccessibilityAnnotatorBackendImpl::OnAccessibilityAnnotationChanged() {
  // TODO(crbug.com/486856790): Implement logic to handle changed annotations.
}

void AccessibilityAnnotatorBackendImpl::
    OnAccessibilityAnnotationSyncBridgeLoaded() {
  // TODO(crbug.com/486856790): Implement logic to handle sync bridge loaded.
}

void AccessibilityAnnotatorBackendImpl::OnURLVisited(
    history::HistoryService* history_service,
    const history::VisitedURLInfo& visited_url_info) {
  // TODO(crbug.com/489690454): Ingest new visit for intent clustering.
}

void AccessibilityAnnotatorBackendImpl::OnHistoryDeletions(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  // TODO(crbug.com/489690454): Purge associated intents/clusters from the
  // persistent SQLite database.
}

void AccessibilityAnnotatorBackendImpl::OnHistoryServiceLoaded(
    history::HistoryService* history_service) {
  // TODO(crbug.com/489690454): Query the history service for historical data.
}

base::optional_ref<const AccessibilityAnnotatorBackend::ContentAnnotationsData>
AccessibilityAnnotatorBackendImpl::GetContentAnnotationsCacheData(
    history::VisitID visit_id) const {
  auto it = content_annotations_cache_.Peek(visit_id);
  if (it != content_annotations_cache_.end()) {
    return it->second;
  }
  return std::nullopt;
}

void AccessibilityAnnotatorBackendImpl::SetContentAnnotationsCacheData(
    history::VisitID visit_id,
    ContentAnnotationsData data) {
  bool is_confirmed = false;

  if (data.content_annotation.has_value()) {
    is_confirmed = data.content_annotation->status() ==
                   optimization_guide::proto::ContentAnnotation::CONFIRMED;
  }

  if (is_confirmed && data.tab_id) {
    ProcessConfirmedStatusLookback(data);
  }

  // This automatically handles eviction of the oldest entries if full.
  base::LRUCache<history::VisitID, ContentAnnotationsData>::iterator it =
      content_annotations_cache_.Put(visit_id, std::move(data));
  observers_.Notify(
      &AccessibilityAnnotatorBackend::Observer::OnContentAnnotationsAdded,
      visit_id, it->second);
}

void AccessibilityAnnotatorBackendImpl::ProcessConfirmedStatusLookback(
    const ContentAnnotationsData& data) {
  if (data.navigation_timestamp.is_null() ||
      !data.content_annotation.has_value()) {
    return;
  }

  base::Time time_range_cutoff =
      base::Time::Now() -
      features::kContentAnnotatorConfirmedStatusLookbackWindow.Get();

  std::vector<const ContentAnnotationsData*> multipage_entries;

  // Iterate through `content_annotations_cache_` and determine other entries
  // with the same `tab_id` and eTLD+1.
  for (const auto& [cached_visit_id, cached_data] :
       content_annotations_cache_) {
    if (!cached_data.tab_id.has_value() ||
        *cached_data.tab_id != *data.tab_id ||
        GetEtldPlusOne(cached_data.url) != GetEtldPlusOne(data.url)) {
      continue;
    }

    // Only consider entries that happened at or before the triggering entry,
    // and within the lookback window.
    if (!cached_data.navigation_timestamp.is_null() &&
        cached_data.navigation_timestamp <= data.navigation_timestamp &&
        cached_data.navigation_timestamp >= time_range_cutoff) {
      multipage_entries.push_back(&cached_data);
    }
  }

  // Sort the entries by timestamp in descending order (most recent first).
  std::sort(
      multipage_entries.begin(), multipage_entries.end(),
      [](const ContentAnnotationsData* a, const ContentAnnotationsData* b) {
        if (a->navigation_timestamp.is_null() ||
            b->navigation_timestamp.is_null()) {
          return false;
        }
        return a->navigation_timestamp > b->navigation_timestamp;
      });

  // Initialize with the triggering data (the most recent one).
  optimization_guide::proto::ContentAnnotation merged_annotation =
      *data.content_annotation;

  // Perform the lookback over multiple pages in descending chronological order
  // (most recent first), merging annotations into `merged_annotation`.
  // When we hit the next CONFIRMED entry, that is our cutoff since it
  // represents a separate event that has been processed by
  // `ProcessConfirmedStatusLookback()` already.
  for (const ContentAnnotationsData* entry : multipage_entries) {
    if (!entry->content_annotation) {
      continue;
    }

    const optimization_guide::proto::ContentAnnotation& src =
        *entry->content_annotation;

    if (src.status() ==
        optimization_guide::proto::ContentAnnotation::CONFIRMED) {
      break;
    }

    if (src.has_structured_data()) {
      MergeContentAnnotationStructuredData(
          merged_annotation.mutable_structured_data(), src.structured_data());
    }
  }

  merged_multipage_annotations_.push_back(std::move(merged_annotation));
  if (merged_multipage_annotations_.size() >
      static_cast<size_t>(
          features::kContentAnnotatorMaxCacheAnnotations.Get())) {
    merged_multipage_annotations_.pop_front();
  }
}

void AccessibilityAnnotatorBackendImpl::MergeContentAnnotationStructuredData(
    optimization_guide::proto::StructuredData* target_structured_data,
    const optimization_guide::proto::StructuredData& source_structured_data) {
  // TODO(crbug.com/492303942): Implement logic to merge structured data.
}

void AccessibilityAnnotatorBackendImpl::RemoveContentAnnotationsCacheData(
    base::span<const history::VisitID> visit_ids) {
  std::vector<history::VisitID> deleted_ids;
  for (history::VisitID visit_id : visit_ids) {
    auto it = content_annotations_cache_.Peek(visit_id);
    if (it != content_annotations_cache_.end()) {
      content_annotations_cache_.Erase(it);
      deleted_ids.push_back(visit_id);
    }
  }

  if (!deleted_ids.empty()) {
    observers_.Notify(
        &AccessibilityAnnotatorBackend::Observer::OnContentAnnotationsDeleted,
        deleted_ids);
  }
}

void AccessibilityAnnotatorBackendImpl::ClearContentAnnotationsCache() {
  if (content_annotations_cache_.empty()) {
    return;
  }
  content_annotations_cache_.Clear();

  observers_.Notify(
      &AccessibilityAnnotatorBackend::Observer::OnContentAnnotationsCleared);
}

base::Value AccessibilityAnnotatorBackendImpl::GetDebugUICacheData() const {
  base::ListValue result;
  for (const std::pair<history::VisitID, ContentAnnotationsData>& item :
       content_annotations_cache_) {
    base::DictValue entry;
    entry.Set("visit_id", base::NumberToString(item.first));
    entry.Set("navigation_timestamp",
              base::UTF16ToUTF8(base::TimeFormatShortDateAndTime(
                  item.second.navigation_timestamp)));
    entry.Set("url", item.second.url.spec());
    entry.Set("title", item.second.page_title);
    entry.Set("classifier_results", item.second.classifier_results.Clone());
    if (item.second.tab_id) {
      entry.Set("tab_id", *item.second.tab_id);
    }
    if (item.second.annotations) {
      entry.Set("annotations", item.second.annotations->Clone());
    }
    if (item.second.content_annotation) {
      entry.Set("content_annotation", optimization_guide::proto::ToValue(
                                          *item.second.content_annotation));
    }
    result.Append(std::move(entry));
  }
  return base::Value(std::move(result));
}

void AccessibilityAnnotatorBackendImpl::GetSyncAnnotationsByTypes(
    EntityTypeEnumSet types,
    base::OnceCallback<void(
        std::vector<sync_pb::AccessibilityAnnotationSpecifics>)> callback) {
  std::move(callback).Run(
      accessibility_annotation_sync_bridge_->GetAnnotationsByTypes(types));
}

AccessibilityAnnotationSyncBridge*
AccessibilityAnnotatorBackendImpl::accessibility_annotation_sync_bridge() {
  return accessibility_annotation_sync_bridge_.get();
}

}  // namespace accessibility_annotator
