// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_background_task.h"

#include "components/offline_pages/core/prefetch/prefetch_background_task_handler.h"
#include "components/offline_pages/core/prefetch/prefetch_dispatcher.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"

namespace offline_pages {

PrefetchBackgroundTask::PrefetchBackgroundTask(PrefetchService* service)
    : service_(service) {}

PrefetchBackgroundTask::~PrefetchBackgroundTask() {
  PrefetchBackgroundTaskHandler* handler =
      service_->GetPrefetchBackgroundTaskHandler();
  if (!handler)
    return;
  switch (reschedule_type_) {
    case PrefetchBackgroundTaskRescheduleType::NO_RESCHEDULE:
      break;
    case PrefetchBackgroundTaskRescheduleType::RESCHEDULE_WITHOUT_BACKOFF:
      handler->ResetBackoff();
      break;
    case PrefetchBackgroundTaskRescheduleType::RESCHEDULE_WITH_BACKOFF:
      handler->Backoff();
      break;
    case PrefetchBackgroundTaskRescheduleType::RESCHEDULE_DUE_TO_SYSTEM:
      // If the task is killed due to the system, it should be rescheduled
      // without backoff even when it is in effect because we want to rerun the
      // task asap.
      handler->PauseBackoffUntilNextRun();
      break;
    case PrefetchBackgroundTaskRescheduleType::SUSPEND:
      handler->Suspend();
      break;
  }

  if (reschedule_type_ != PrefetchBackgroundTaskRescheduleType::NO_RESCHEDULE)
    handler->EnsureTaskScheduled();
}

void PrefetchBackgroundTask::SetReschedule(
    PrefetchBackgroundTaskRescheduleType type) {
  switch (type) {
    // |SUSPEND| takes highest precendence.
    case PrefetchBackgroundTaskRescheduleType::SUSPEND:
      reschedule_type_ = PrefetchBackgroundTaskRescheduleType::SUSPEND;
      break;
    // |RESCHEDULE_DUE_TO_SYSTEM| takes 2nd highest precendence and thus it
    // can't overwrite |SUSPEND|.
    case PrefetchBackgroundTaskRescheduleType::RESCHEDULE_DUE_TO_SYSTEM:
      if (reschedule_type_ != PrefetchBackgroundTaskRescheduleType::SUSPEND) {
        reschedule_type_ =
            PrefetchBackgroundTaskRescheduleType::RESCHEDULE_DUE_TO_SYSTEM;
      }
      break;
    // |RESCHEDULE_WITH_BACKOFF| takes 3rd highest precendence and thus it can't
    // overwrite |SUSPEND| and |RESCHEDULE_DUE_TO_SYSTEM|.
    case PrefetchBackgroundTaskRescheduleType::RESCHEDULE_WITH_BACKOFF:
      if (reschedule_type_ != PrefetchBackgroundTaskRescheduleType::SUSPEND &&
          reschedule_type_ !=
              PrefetchBackgroundTaskRescheduleType::RESCHEDULE_DUE_TO_SYSTEM) {
        reschedule_type_ =
            PrefetchBackgroundTaskRescheduleType::RESCHEDULE_WITH_BACKOFF;
      }
      break;
    // |RESCHEDULE_WITHOUT_BACKOFF| takes 4th highest precendence and thus it
    // only overwrites the lowest precendence |NO_RESCHEDULE|.
    case PrefetchBackgroundTaskRescheduleType::RESCHEDULE_WITHOUT_BACKOFF:
      if (reschedule_type_ ==
          PrefetchBackgroundTaskRescheduleType::NO_RESCHEDULE) {
        reschedule_type_ =
            PrefetchBackgroundTaskRescheduleType::RESCHEDULE_WITHOUT_BACKOFF;
      }
      break;
    case PrefetchBackgroundTaskRescheduleType::NO_RESCHEDULE:
      break;
  }
}

}  // namespace offline_pages
