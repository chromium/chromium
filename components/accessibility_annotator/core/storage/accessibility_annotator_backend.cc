// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/storage/accessibility_annotator_backend.h"

#include "base/containers/map_util.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/notimplemented.h"
#include "base/task/thread_pool.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"
#include "components/accessibility_annotator/core/storage/accessibility_annotation_sync_bridge.h"
#include "components/accessibility_annotator/core/storage/accessibility_annotator_database.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"

namespace accessibility_annotator {

AccessibilityAnnotatorBackend::AccessibilityAnnotatorBackend(
    version_info::Channel channel,
    history::HistoryService* history_service,
    syncer::RepeatingDataTypeStoreFactory data_type_store_factory,
    const base::FilePath& db_path)
    : db_path_(db_path),
      db_(base::ThreadPool::CreateSequencedTaskRunnerForResource(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
          db_path_)),
      content_annotations_cache_(kContentAnnotatorMaxCacheAnnotations.Get()) {
  auto processor = std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
      syncer::ACCESSIBILITY_ANNOTATION,
      base::BindRepeating(&syncer::ReportUnrecoverableError, channel));
  accessibility_annotation_sync_bridge_ =
      std::make_unique<AccessibilityAnnotationSyncBridge>(
          std::move(processor), data_type_store_factory);
  sync_bridge_observation_.Observe(accessibility_annotation_sync_bridge_.get());
  if (history_service) {
    history_service_observation_.Observe(history_service);
  }
}

AccessibilityAnnotatorBackend::~AccessibilityAnnotatorBackend() = default;

void AccessibilityAnnotatorBackend::Init() {
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
AccessibilityAnnotatorBackend::GetAccessibilityAnnotationControllerDelegate() {
  return accessibility_annotation_sync_bridge_->change_processor()
      ->GetControllerDelegate();
}

void AccessibilityAnnotatorBackend::OnAccessibilityAnnotationChanged() {
  // TODO(crbug.com/486856790): Implement logic to handle changed annotations.
}

void AccessibilityAnnotatorBackend::
    OnAccessibilityAnnotationSyncBridgeLoaded() {
  // TODO(crbug.com/486856790): Implement logic to handle sync bridge loaded.
}

void AccessibilityAnnotatorBackend::OnURLVisited(
    history::HistoryService* history_service,
    const history::VisitedURLInfo& visited_url_info) {
  // TODO(crbug.com/489690454): Ingest new visit for intent clustering.
}

void AccessibilityAnnotatorBackend::OnHistoryDeletions(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  // TODO(crbug.com/489690454): Purge associated intents/clusters from the
  // persistent SQLite database.
}

void AccessibilityAnnotatorBackend::OnHistoryServiceLoaded(
    history::HistoryService* history_service) {
  // TODO(crbug.com/489690454): Query the history service for historical data.
}

std::optional<std::string>
AccessibilityAnnotatorBackend::GetContentAnnotationsCacheData(
    const GURL& url) const {
  auto it = content_annotations_cache_.Peek(url);
  if (it != content_annotations_cache_.end()) {
    return it->second;
  }
  return std::nullopt;
}

void AccessibilityAnnotatorBackend::SetContentAnnotationsCacheData(
    const GURL& url,
    std::string annotations) {
  // This automatically handles eviction of the oldest entries if full.
  content_annotations_cache_.Put(url, std::move(annotations));
}

std::string AccessibilityAnnotatorBackend::GetDebugUIFormattedCacheData()
    const {
  // TODO(b/488355081): Pull data from the cache here and format it for UI
  // display.
  return "Cache data not yet available for the debug UI.";
}

AccessibilityAnnotationSyncBridge*
AccessibilityAnnotatorBackend::accessibility_annotation_sync_bridge() {
  return accessibility_annotation_sync_bridge_.get();
}

}  // namespace accessibility_annotator
