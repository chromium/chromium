// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_BACKGROUND_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_BACKGROUND_TASK_H_

#include "components/offline_pages/core/prefetch/prefetch_types.h"

namespace offline_pages {

class PrefetchService;

// A task managing the background activity of the offline page prefetcher.
class PrefetchBackgroundTask {
 public:
  explicit PrefetchBackgroundTask(PrefetchService* service);
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
  PrefetchService* service_;

  DISALLOW_COPY_AND_ASSIGN(PrefetchBackgroundTask);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_BACKGROUND_TASK_H_
