// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_DATA_MODELS_INTENT_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_DATA_MODELS_INTENT_H_

#include <string>

#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_row.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace accessibility_annotator {

using TaskIntentId = base::IdType64<class TaskIntentTag>;

// The enumerated type of task intent.
// This enum is persisted, so values should not be changed, revised, or reused.
// Append new values only to the end of the list.
enum class TaskIntentType {
  kUnknown = 0,
};

// The enumerated type of task intent status.
// This enum is persisted, so values should not be changed, revised, or reused.
// Append new values only to the end of the list.
enum class TaskIntentStatusType {
  kUnknown = 0,
  kActive = 1,
  kCompleted = 2,
  kExpired = 3,
};

// A value object that represents a task-level intent inferred from a single
// cluster of browsed history and the inferred status of the intent (whether
// it might still be updated in the near future)
struct TaskIntent {
  TaskIntent();
  TaskIntent(const TaskIntent& other);
  TaskIntent(TaskIntent&& other);
  TaskIntent& operator=(const TaskIntent& other);
  TaskIntent& operator=(TaskIntent&& other);
  ~TaskIntent();

  TaskIntentId id;
  absl::flat_hash_map<history::VisitID, history::URLID>
      source_visit_to_url_id_map;
  base::Time cluster_most_recent_visit_time;
  TaskIntentType task_intent_type;
  std::u16string task_intent;
  TaskIntentStatusType task_intent_status_type;
  std::u16string task_intent_status;
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_DATA_MODELS_INTENT_H_
