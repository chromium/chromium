// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRACING_COMMON_INCLUSION_POLICY_WIN_H_
#define COMPONENTS_TRACING_COMMON_INCLUSION_POLICY_WIN_H_

#include "base/memory/raw_ref.h"
#include "components/tracing/tracing_export.h"

namespace tracing {

class ActiveProcesses;

class TRACING_EXPORT InclusionPolicy {
 public:
  explicit InclusionPolicy(const ActiveProcesses& active_processes)
      : active_processes_(active_processes) {}

  // Returns true if produced Perfetto events should include the identifier of
  // the given thread.
  bool ShouldIncludeThreadId(uint32_t thread_id) const;

 private:
  const raw_ref<const ActiveProcesses> active_processes_;
};

}  // namespace tracing

#endif  // COMPONENTS_TRACING_COMMON_INCLUSION_POLICY_WIN_H_
