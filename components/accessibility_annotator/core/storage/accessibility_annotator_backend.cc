// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/storage/accessibility_annotator_backend.h"

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "components/accessibility_annotator/core/storage/accessibility_annotator_database.h"

namespace accessibility_annotator {

AccessibilityAnnotatorBackend::AccessibilityAnnotatorBackend()
    : db_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})) {}

AccessibilityAnnotatorBackend::~AccessibilityAnnotatorBackend() = default;

void AccessibilityAnnotatorBackend::Init(const base::FilePath& db_path) {
  db_.AsyncCall(&AccessibilityAnnotatorDatabase::Init)
      .WithArgs(db_path)
      .Then(base::BindOnce([](bool success) {
        DVLOG_IF(1, !success)
            << "Failed to initialize AccessibilityAnnotatorDatabase.";
      }));
}

}  // namespace accessibility_annotator
