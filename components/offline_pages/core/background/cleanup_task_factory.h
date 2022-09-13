// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_CLEANUP_TASK_FACTORY_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_CLEANUP_TASK_FACTORY_H_

#include <stdint.h>

#include <set>

#include "base/memory/raw_ptr.h"
#include "components/offline_pages/core/background/cleanup_task.h"

namespace offline_pages {

class OfflinerPolicy;
class RequestCoordinatorEventLogger;
class RequestNotifier;
class RequestQueueStore;

class CleanupTaskFactory {
 public:
  CleanupTaskFactory(OfflinerPolicy* policy,
                     RequestNotifier* notifier,
                     RequestCoordinatorEventLogger* event_logger);

  ~CleanupTaskFactory();

  std::unique_ptr<CleanupTask> CreateCleanupTask(RequestQueueStore* store);

 private:
  // Unowned pointer to the Policy
  raw_ptr<OfflinerPolicy> policy_;
  // Unowned pointer to the notifier
  raw_ptr<RequestNotifier> notifier_;
  // Unowned pointer to the EventLogger
  raw_ptr<RequestCoordinatorEventLogger> event_logger_;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_CLEANUP_TASK_FACTORY_H_
