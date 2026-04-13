// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/storage/accessibility_annotator_backend_impl.h"

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

namespace accessibility_annotator {

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
    const GURL& url) const {
  auto it = content_annotations_cache_.Peek(url);
  if (it != content_annotations_cache_.end()) {
    return it->second;
  }
  return std::nullopt;
}

void AccessibilityAnnotatorBackendImpl::SetContentAnnotationsCacheData(
    const GURL& url,
    ContentAnnotationsData data) {
  base::LRUCache<GURL, ContentAnnotationsData>::iterator it =
      content_annotations_cache_.Put(url, std::move(data));

  observers_.Notify(
      &AccessibilityAnnotatorBackend::Observer::OnContentAnnotationsAdded,
      it->second);
}

void AccessibilityAnnotatorBackendImpl::RemoveContentAnnotationsCacheData(
    base::span<const GURL> urls) {
  for (const GURL& url : urls) {
    auto it = content_annotations_cache_.Peek(url);
    if (it != content_annotations_cache_.end()) {
      content_annotations_cache_.Erase(it);
    }
  }
}

void AccessibilityAnnotatorBackendImpl::ClearContentAnnotationsCache() {
  content_annotations_cache_.Clear();
}

base::Value AccessibilityAnnotatorBackendImpl::GetDebugUICacheData() const {
  base::ListValue result;
  for (const std::pair<GURL, ContentAnnotationsData>& item :
       content_annotations_cache_) {
    base::DictValue entry;
    entry.Set("visit_id", base::NumberToString(item.second.visit_id));
    entry.Set("navigation_timestamp",
              base::UTF16ToUTF8(base::TimeFormatShortDateAndTime(
                  item.second.navigation_timestamp)));
    entry.Set("url", item.first.spec());
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
