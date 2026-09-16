// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_BROWSER_CHILD_PROCESS_HOST_ID_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_BROWSER_CHILD_PROCESS_HOST_ID_H_

#include "base/types/strong_alias.h"
#include "content/public/common/child_process_id.h"

namespace performance_manager {

// A unique identifier for a child process of the browser process, e.g. a
// utility process. This is represented by a ChildProcessId but wrapped in
// order to distinguish it from a RenderProcessHostId which is fundamentally
// the same type but a different type of process.
using BrowserChildProcessHostId =
    base::StrongAlias<class BrowserChildProcessHostIdTag,
                      content::ChildProcessId>;

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_BROWSER_CHILD_PROCESS_HOST_ID_H_
