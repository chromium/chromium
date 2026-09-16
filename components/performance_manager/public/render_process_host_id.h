// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RENDER_PROCESS_HOST_ID_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RENDER_PROCESS_HOST_ID_H_

#include "base/types/strong_alias.h"
#include "content/public/common/child_process_id.h"

namespace performance_manager {

// A unique identifier for a renderer process. This is represented by a
// ChildProcessId but wrapped in order to distinguish it from a
// BrowserChildProcessHostId which is fundamentally the same type but a
// different type of process.
using RenderProcessHostId =
    base::StrongAlias<class RenderProcessHostIdTag, content::ChildProcessId>;

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RENDER_PROCESS_HOST_ID_H_
