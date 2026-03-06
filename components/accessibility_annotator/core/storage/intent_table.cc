// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/storage/intent_table.h"

#include "base/notimplemented.h"
#include "components/accessibility_annotator/core/data_models/intent.h"
#include "components/history/core/browser/history_types.h"

namespace accessibility_annotator {

IntentTable::IntentTable() = default;
IntentTable::~IntentTable() = default;

bool IntentTable::Init(sql::Database* database) {
  if (!database) {
    return false;
  }

  database_ = database;
  return true;
}

bool IntentTable::AddOrUpdateTaskIntent(const TaskIntent& task_intent) {
  NOTIMPLEMENTED();
  return false;
}

std::vector<TaskIntent> IntentTable::GetTaskIntentsByStatusType(
    TaskIntentStatusType status_type) {
  NOTIMPLEMENTED();
  return {};
}

bool IntentTable::InvalidateTaskIntentsForDeletedHistory(
    const history::DeletionInfo& deletion_info) {
  NOTIMPLEMENTED();
  return false;
}

bool IntentTable::DeleteAllTaskIntents() {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace accessibility_annotator
