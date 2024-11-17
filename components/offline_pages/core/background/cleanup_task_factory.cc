// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/background/cleanup_task_factory.h"

#include "components/offline_pages/core/background/offliner_policy.h"
#include "components/offline_pages/core/background/request_coordinator_event_logger.h"
#include "components/offline_pages/core/background/request_notifier.h"

namespace offline_pages {

// Capture the common parameters that we will need to make a pick task,
// and use them when making tasks.  Create this once each session, and
// use it to build a picker task when needed.
CleanupTaskFactory::CleanupTaskFactory(
    OfflinerPolicy* policy,
    RequestNotifier* notifier,
    RequestCoordinatorEventLogger* event_logger)
    : policy_(policy), notifier_(notifier), event_logger_(event_logger) {
  DCHECK(policy);
  DCHECK(notifier);
  DCHECK(event_logger);
}

CleanupTaskFactory::~CleanupTaskFactory() = default;

std::unique_ptr<CleanupTask> CleanupTaskFactory::CreateCleanupTask(
    RequestQueueStore* store) {
  std::unique_ptr<CleanupTask> task(
      new CleanupTask(store, policy_, notifier_, event_logger_));
  return task;
}

}  // namespace offline_pages
