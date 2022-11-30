// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_BACKGROUND_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_BACKGROUND_TASK_H_

#include "base/memory/raw_ptr.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"

namespace offline_pages {

class PrefetchService;

// A task managing the background activity of the offline page prefetcher.
class PrefetchBackgroundTask {
 public:
  explicit PrefetchBackgroundTask(PrefetchService* service);

  PrefetchBackgroundTask(const PrefetchBackgroundTask&) = delete;
  PrefetchBackgroundTask& operator=(const PrefetchBackgroundTask&) = delete;

  virtual ~PrefetchBackgroundTask();

  // Tells the system how to reschedule the running of next background task when
  // this background task completes.
  // Overridden for testing only.
  virtual void SetReschedule(PrefetchBackgroundTaskRescheduleType type);

  PrefetchBackgroundTaskRescheduleType reschedule_type() const {
    return reschedule_type_;
  }

  PrefetchService* service() const { return service_; }

 private:
  PrefetchBackgroundTaskRescheduleType reschedule_type_ =
      PrefetchBackgroundTaskRescheduleType::NO_RESCHEDULE;

  // The PrefetchService owns |this|, so a raw pointer is OK.
  raw_ptr<PrefetchService> service_;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_BACKGROUND_TASK_H_
