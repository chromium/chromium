// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RENDER_PROCESS_HOST_ID_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RENDER_PROCESS_HOST_ID_H_

#include "base/types/id_type.h"
#include "content/public/browser/child_process_host.h"

namespace performance_manager {

using RenderProcessHostIdBase =
    base::IdType<class RenderProcessHostIdTag,
                 int32_t,
                 content::ChildProcessHost::kInvalidUniqueID,
                 1>;

// A strongly typed wrapper for the id returned by RenderProcessHost::GetID().
//
// This uses ChildProcessHost::kInvalidUniqueId (-1) as the default invalid id,
// but also recognizes 0 as an invalid id because there is existing code that
// uses 0 as an invalid value. It starts generating id's at 1.
class RenderProcessHostId : public RenderProcessHostIdBase {
 public:
  using RenderProcessHostIdBase::RenderProcessHostIdBase;

  // 0 is also an invalid value.
  constexpr bool is_null() const {
    return RenderProcessHostIdBase::is_null() || this->value() == 0;
  }

  // Override operator bool() to call the overridden is_null().
  constexpr explicit operator bool() const { return !is_null(); }
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RENDER_PROCESS_HOST_ID_H_
