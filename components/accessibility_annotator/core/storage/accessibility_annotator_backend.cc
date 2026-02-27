// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/storage/accessibility_annotator_backend.h"

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "components/accessibility_annotator/core/storage/accessibility_annotation_sync_bridge.h"
#include "components/accessibility_annotator/core/storage/accessibility_annotator_database.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"

namespace accessibility_annotator {

AccessibilityAnnotatorBackend::AccessibilityAnnotatorBackend(
    version_info::Channel channel,
    syncer::RepeatingDataTypeStoreFactory data_type_store_factory)
    : db_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})) {
  auto processor = std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
      syncer::ACCESSIBILITY_ANNOTATION,
      base::BindRepeating(&syncer::ReportUnrecoverableError, channel));
  accessibility_annotation_sync_bridge_ =
      std::make_unique<AccessibilityAnnotationSyncBridge>(
          std::move(processor), data_type_store_factory);
}

AccessibilityAnnotatorBackend::~AccessibilityAnnotatorBackend() = default;

void AccessibilityAnnotatorBackend::Init(const base::FilePath& db_path) {
  db_.AsyncCall(&AccessibilityAnnotatorDatabase::Init)
      .WithArgs(db_path)
      .Then(base::BindOnce([](bool success) {
        DVLOG_IF(1, !success)
            << "Failed to initialize AccessibilityAnnotatorDatabase.";
      }));
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
AccessibilityAnnotatorBackend::GetAccessibilityAnnotationControllerDelegate() {
  return accessibility_annotation_sync_bridge_->change_processor()
      ->GetControllerDelegate();
}

}  // namespace accessibility_annotator
