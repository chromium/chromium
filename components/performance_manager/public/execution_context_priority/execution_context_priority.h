// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_EXECUTION_CONTEXT_PRIORITY_EXECUTION_CONTEXT_PRIORITY_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_EXECUTION_CONTEXT_PRIORITY_EXECUTION_CONTEXT_PRIORITY_H_

#include "base/task/task_traits.h"
#include "components/performance_manager/public/voting/optional_voting_channel.h"
#include "components/performance_manager/public/voting/voting.h"

// Specialization of a voting system used to get votes related to the
// TaskPriority of ExecutionContexts.

namespace performance_manager {

namespace execution_context {
class ExecutionContext;
}

namespace execution_context_priority {

using execution_context::ExecutionContext;

// Helper function equivalent to strcmp, but that is safe to use with nullptr.
int ReasonCompare(const char* reason1, const char* reason2);

// Helper class for storing a priority and a reason.
class PriorityAndReason {
 public:
  PriorityAndReason() = default;
  constexpr PriorityAndReason(base::TaskPriority priority, const char* reason)
      : priority_(priority), reason_(reason) {}
  PriorityAndReason(const PriorityAndReason&) = default;
  PriorityAndReason& operator=(const PriorityAndReason&) = default;
  ~PriorityAndReason() = default;

  base::TaskPriority priority() const { return priority_; }
  const char* reason() const { return reason_; }

  // Returns -1, 0 or 1 indicating the outcome of a comparison of this value
  // and |other|.
  int Compare(const PriorityAndReason& other) const;

  bool operator==(const PriorityAndReason& other) const;
  bool operator!=(const PriorityAndReason& other) const;
  bool operator<=(const PriorityAndReason& other) const;
  bool operator>=(const PriorityAndReason& other) const;
  bool operator<(const PriorityAndReason& other) const;
  bool operator>(const PriorityAndReason& other) const;

 private:
  base::TaskPriority priority_ = base::TaskPriority::LOWEST;
  const char* reason_ = nullptr;
};

using Vote = voting::
    Vote<ExecutionContext, base::TaskPriority, base::TaskPriority::LOWEST>;
using VoterId = voting::VoterId<Vote>;
using VoteObserver = voting::VoteObserver<Vote>;
using VotingChannel = voting::VotingChannel<Vote>;
using VotingChannelFactory = voting::VotingChannelFactory<Vote>;
using OptionalVotingChannel = voting::OptionalVotingChannel<Vote>;

}  // namespace execution_context_priority
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_EXECUTION_CONTEXT_PRIORITY_EXECUTION_CONTEXT_PRIORITY_H_
