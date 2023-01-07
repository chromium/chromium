// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

int PriorityAndReason::Compare(const PriorityAndReason& other) const {
  if (priority_ > other.priority_)
    return 1;
  if (priority_ < other.priority_)
    return -1;
  return ReasonCompare(reason_, other.reason_);
}

bool PriorityAndReason::operator==(const PriorityAndReason& other) const {
  return Compare(other) == 0;
}

bool PriorityAndReason::operator!=(const PriorityAndReason& other) const {
  return Compare(other) != 0;
}

bool PriorityAndReason::operator<=(const PriorityAndReason& other) const {
  return Compare(other) <= 0;
}

bool PriorityAndReason::operator>=(const PriorityAndReason& other) const {
  return Compare(other) >= 0;
}

bool PriorityAndReason::operator<(const PriorityAndReason& other) const {
  return Compare(other) < 0;
}

bool PriorityAndReason::operator>(const PriorityAndReason& other) const {
  return Compare(other) > 0;
}

}  // namespace execution_context_priority
}  // namespace performance_manager
