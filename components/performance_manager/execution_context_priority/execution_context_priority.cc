// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/390223051): Remove C-library calls to fix the errors.
#pragma allow_unsafe_libc_calls
#endif

#include "components/performance_manager/public/execution_context_priority/execution_context_priority.h"

#include <cstring>

namespace performance_manager {
namespace execution_context_priority {

int ReasonCompare(const char* reason1, const char* reason2) {
  if (reason1 == reason2)
    return 0;
  if (reason1 == nullptr)
    return -1;
  if (reason2 == nullptr)
    return 1;
  return ::strcmp(reason1, reason2);
}

/////////////////////////////////////////////////////////////////////
// PriorityAndReason

bool operator==(const PriorityAndReason& lhs, const PriorityAndReason& rhs) {
  return lhs.priority_ == rhs.priority_ &&
         ReasonCompare(lhs.reason_, rhs.reason_) == 0;
}

}  // namespace execution_context_priority
}  // namespace performance_manager
