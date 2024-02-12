// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_PERFORMANCE_HINT_UTILS_H_
#define COMPONENTS_VIZ_COMMON_PERFORMANCE_HINT_UTILS_H_

#include "base/containers/flat_set.h"
#include "base/process/process_handle.h"
#include "base/threading/platform_thread.h"
#include "components/viz/common/viz_common_export.h"

namespace viz {

// Check that thread IDs in `thread_ids_from_sandboxed_process` are not in
// current process. This function should be called from the Browser and the GPU
// processes. Android OS will check that thread IDs used for performance hint
// belongs to a chromium process. So this check ensures that thread IDs
// reported by sandboxed processes are indeed in sandboxed processes.
VIZ_COMMON_EXPORT bool CheckThreadIdsDoNotBelongToCurrentProcess(
    const base::flat_set<base::PlatformThreadId>&
        thread_ids_from_sandboxed_process);

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_PERFORMANCE_HINT_UTILS_H_
