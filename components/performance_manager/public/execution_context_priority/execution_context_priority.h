// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_EXECUTION_CONTEXT_PRIORITY_EXECUTION_CONTEXT_PRIORITY_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_EXECUTION_CONTEXT_PRIORITY_EXECUTION_CONTEXT_PRIORITY_H_

#include <variant>

#include "base/process/process.h"
#include "base/task/task_traits.h"
#include "components/performance_manager/public/voting/voting.h"

// Specialization of a voting system used to get votes related to the
// ProcessPriority of graph nodes.

namespace content {
class WebContents;
}

namespace performance_manager {

class FrameNode;
class PageNode;
class ProcessNode;
class WorkerNode;

namespace execution_context_priority {

// The graph nodes that a priority vote can target. Any node type not listed
// here (e.g. SystemNode) is intentionally not votable.
//
// A vote on a PageNode applies to every frame of that page (see
// PageToFrameVoteExpander).
//
// A vote on a ProcessNode is aggregated with the priority of the frames and
// workers it hosts (see ProcessPriorityAggregator). Only renderer processes can
// be voted on.
using VoteContext = std::variant<const FrameNode*,
                                 const WorkerNode*,
                                 const PageNode*,
                                 const ProcessNode*>;

// Helper function equivalent to strcmp, but that is safe to use with nullptr.
int ReasonCompare(const char* reason1, const char* reason2);

// Helper class for storing a priority and a reason.
class PriorityAndReason {
 public:
  PriorityAndReason() = default;
  constexpr PriorityAndReason(base::Process::Priority priority,
                              const char* reason)
      : priority_(priority), reason_(reason) {}
  PriorityAndReason(const PriorityAndReason&) = default;
  PriorityAndReason& operator=(const PriorityAndReason&) = default;
  ~PriorityAndReason() = default;

  base::Process::Priority priority() const { return priority_; }
  const char* reason() const { return reason_; }

  friend bool operator==(const PriorityAndReason& lhs,
                         const PriorityAndReason& rhs);
  friend auto operator<=>(const PriorityAndReason& lhs,
                          const PriorityAndReason& rhs) {
    if (lhs.priority_ != rhs.priority_) {
      return lhs.priority_ <=> rhs.priority_;
    }
    return ReasonCompare(lhs.reason_, rhs.reason_) <=> 0;
  }

 private:
  base::Process::Priority priority_ = base::Process::Priority::kMinValue;
  const char* reason_ = nullptr;
};

using Vote = voting::Vote<VoteContext,
                          base::Process::Priority,
                          base::Process::Priority::kMinValue>;
using VoterId = voting::VoterId<Vote>;
using VoteObserver = voting::VoteObserver<Vote>;
using VotingChannel = voting::VotingChannel<Vote>;
using VotingChannelFactory = voting::VotingChannelFactory<Vote>;

// Sets whether the given `contents` is closing.
// Must be called from the PM sequence.
void SetPageIsClosing(content::WebContents* contents, bool is_closing);

}  // namespace execution_context_priority
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_EXECUTION_CONTEXT_PRIORITY_EXECUTION_CONTEXT_PRIORITY_H_
