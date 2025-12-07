// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/common/inclusion_policy_win.h"

#include "components/tracing/common/active_processes_win.h"

namespace tracing {

bool InclusionPolicy::ShouldIncludeThreadId(uint32_t thread_id) const {
  return active_processes_->GetThreadCategory(thread_id) !=
         ActiveProcesses::Category::kOther;
}

bool InclusionPolicy::ShouldRecordFileIoEvents(uint32_t thread_id) const {
  return active_processes_->GetThreadCategory(thread_id) ==
         ActiveProcesses::Category::kClient;
}

}  // namespace tracing
