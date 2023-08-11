// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RENDER_PROCESS_HOST_ID_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RENDER_PROCESS_HOST_ID_H_

#include "base/types/id_type.h"
#include "content/public/browser/child_process_host.h"

namespace performance_manager {

// A strongly typed wrapper for the id returned by RenderProcessHost::GetID().
//
// This uses ChildProcessHost::kInvalidUniqueId (-1) as the default invalid id,
// but also recognizes 0 as an invalid id because there is existing code that
// uses 0 as an invalid value. It starts generating id's at 1.
using RenderProcessHostId =
    base::IdType<class RenderProcessHostIdTag,
                 int32_t,
                 content::ChildProcessHost::kInvalidUniqueID,
                 /*kFirstGeneratedId=*/1,
                 /*kExtraInvalidValues=*/0>;

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RENDER_PROCESS_HOST_ID_H_
