// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/performance_hint_utils.h"

#include <vector>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/linux_util.h"
#endif

namespace viz {

bool CheckThreadIdsDoNotBelongToCurrentProcess(
    const base::flat_set<base::PlatformThreadId>&
        thread_ids_from_sandboxed_process) {
#if BUILDFLAG(IS_ANDROID)
  // This function is similar to the one above, but it's based on /proc/self
  // instead of /proc/<pid> under the hood. Unlike /proc/<pid>, /proc/self
  // should be always accessible for the Browser and the GPU process on
  // Android.
  std::vector<pid_t> privileged_thread_ids;
  if (!base::GetThreadsForCurrentProcess(&privileged_thread_ids)) {
    return false;
  }
  for (const auto& tid : thread_ids_from_sandboxed_process) {
    if (base::Contains(privileged_thread_ids, tid)) {
      return false;
    }
  }
  return true;
#else
  return false;
#endif
}

}  // namespace viz
