// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PRIORITY_PRIORITY_SETTER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PRIORITY_PRIORITY_SETTER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "components/performance_manager/public/priority/max_vote_aggregator.h"
#include "components/performance_manager/public/priority/priority.h"

namespace performance_manager::priority {

// Sets the priority of frames, workers and renderer processes from their top
// vote in the MaxVoteAggregator, or to their default priority when they have no
// vote.
class PrioritySetter : public MaxVoteAggregator::Observer {
 public:
  // `max_vote_aggregator` must outlive this.
  explicit PrioritySetter(MaxVoteAggregator* max_vote_aggregator);
  ~PrioritySetter() override;

  PrioritySetter(const PrioritySetter&) = delete;
  PrioritySetter& operator=(const PrioritySetter&) = delete;

  // MaxVoteAggregator::Observer:
  void OnFrameTopVoteChanged(const FrameNode* frame_node,
                             const std::optional<Vote>& vote) override;
  void OnWorkerTopVoteChanged(const WorkerNode* worker_node,
                              const std::optional<Vote>& vote) override;
  void OnProcessTopVoteChanged(const ProcessNode* process_node,
                               const std::optional<Vote>& vote) override;

 private:
  const raw_ptr<MaxVoteAggregator> max_vote_aggregator_;
};

}  // namespace performance_manager::priority

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PRIORITY_PRIORITY_SETTER_H_
