// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/performance_hint/utils.h"

#include "base/containers/contains.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/linux_util.h"
#endif

namespace viz {

bool CheckThreadIdsDoNotBelongToProcessIds(
    const std::vector<base::ProcessId>& privileged_process_ids,
    const base::flat_set<base::PlatformThreadId>&
        thread_ids_from_sandboxed_process) {
#if BUILDFLAG(IS_ANDROID)
  // This is inherently racy as threads or processes can die and the IDs can be
  // reused at any time. But this is the best we can do.
  for (auto& process_id : privileged_process_ids) {
    std::vector<pid_t> privileged_thread_ids;
    if (!base::GetThreadsForProcess(process_id, &privileged_thread_ids)) {
      return false;
    }
    for (auto& tid : thread_ids_from_sandboxed_process) {
      if (base::Contains(privileged_thread_ids, tid)) {
        return false;
      }
    }
  }
  return true;
#else
  return false;
#endif
}

}  // namespace viz
