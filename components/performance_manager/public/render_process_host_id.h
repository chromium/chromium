// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RENDER_PROCESS_HOST_ID_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RENDER_PROCESS_HOST_ID_H_

#include "content/public/browser/child_process_id.h"

namespace performance_manager {

// A typedef around the type returned by RenderProcessHost::GetID().
using RenderProcessHostId = content::ChildProcessId;

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RENDER_PROCESS_HOST_ID_H_
