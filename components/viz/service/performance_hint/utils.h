// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_PERFORMANCE_HINT_UTILS_H_
#define COMPONENTS_VIZ_SERVICE_PERFORMANCE_HINT_UTILS_H_

#include <vector>

#include "base/containers/flat_set.h"
#include "base/process/process_handle.h"
#include "base/threading/platform_thread.h"

namespace viz {

// Check that thread IDs in `thread_ids_from_sandboxed_process` are not in any
// of process in `privileged_process_ids`. Android OS will check that thread IDs
// used for performance hint belongs to a chromium process. So this check
// ensures that thread IDs reported by sandboxed processes are indeed in
// sandboxed processes.
bool CheckThreadIdsDoNotBelongToProcessIds(
    const std::vector<base::ProcessId>& privileged_process_ids,
    const base::flat_set<base::PlatformThreadId>&
        thread_ids_from_sandboxed_process);

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_PERFORMANCE_HINT_UTILS_H_
