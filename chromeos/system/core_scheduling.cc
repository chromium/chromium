// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/system/core_scheduling.h"

#include <errno.h>
#include <sys/prctl.h>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"

// Downstream core scheduling interface for 4.19, 5.4 kernels.
// TODO(b/152605392): Remove once those kernel versions are obsolete.
#ifndef PR_SET_CORE_SCHED
#define PR_SET_CORE_SCHED 0x200
#endif

// Upstream interface for core scheduling.
#ifndef PR_SCHED_CORE
#define PR_SCHED_CORE 62
#define PR_SCHED_CORE_GET 0
#define PR_SCHED_CORE_CREATE 1
#define PR_SCHED_CORE_SHARE_TO 2
#define PR_SCHED_CORE_SHARE_FROM 3
#define PR_SCHED_CORE_MAX 4
#endif

enum pid_type { PIDTYPE_PID = 0, PIDTYPE_TGID, PIDTYPE_PGID };

namespace chromeos {
namespace system {

namespace {
const base::Feature kCoreScheduling{"CoreSchedulingEnabled",
                                    base::FEATURE_ENABLED_BY_DEFAULT};
}

void EnableCoreSchedulingIfAvailable() {
  if (!base::FeatureList::IsEnabled(kCoreScheduling)) {
    return;
  }

  // prctl(2) will return EINVAL for unknown functions. We're tolerant to this
  // and will log an error message for non EINVAL errnos.
  if (prctl(PR_SET_CORE_SCHED, 1) == -1 &&
      prctl(PR_SCHED_CORE, PR_SCHED_CORE_CREATE, 0, PIDTYPE_PID, 0) == -1) {
    PLOG_IF(WARNING, errno != EINVAL) << "Unable to set core scheduling";
  }
}

}  // namespace system
}  // namespace chromeos
