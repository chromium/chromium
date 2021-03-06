// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/freezing/freezing.h"

#include "base/bind.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "components/performance_manager/freezing/freezing_vote_aggregator.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/browser/web_contents.h"

namespace performance_manager {

namespace freezing {

namespace {

// The counterpart of a FreezingVoteToken that lives on the PM sequence.
class FreezingVoteTokenPMImpl : public PageNode::ObserverDefaultImpl {
 public:
  FreezingVoteTokenPMImpl(content::WebContents* content,
                          FreezingVoteValue vote_value,
                          const char* vote_reason);
  ~FreezingVoteTokenPMImpl() override;
  FreezingVoteTokenPMImpl(const FreezingVoteTokenPMImpl& other) = delete;
  FreezingVoteTokenPMImpl& operator=(const FreezingVoteTokenPMImpl&) = delete;

  void InitializeOnGraph(base::WeakPtr<PageNode> page_node,
                         FreezingVote vote,
                         Graph* graph);

  // PageNodeObserver:
  void OnBeforePageNodeRemoved(const PageNode* page_node) override;

 private:
  // Resets this instance so it is no longer casting a vote for |page_node_|.
  void Reset();

  const PageNode* page_node_ = nullptr;

  base::ScopedObservation<Graph,
                          PageNodeObserver,
                          &Graph::AddPageNodeObserver,
                          &Graph::RemovePageNodeObserver>
      page_node_observation_{this};

  // Voting channel.
  FreezingVotingChannel voting_channel_;

  SEQUENCE_CHECKER(sequence_checker_);
};

// Concrete implementation of a FreezingVoteToken.
class FreezingVoteTokenImpl : public FreezingVoteToken {
 public:
  FreezingVoteTokenImpl(content::WebContents* content,
                        FreezingVoteValue vote_value,
                        const char* vote_reason);
  ~FreezingVoteTokenImpl() override;
  FreezingVoteTokenImpl(const FreezingVoteTokenImpl& other) = delete;
  FreezingVoteTokenImpl& operator=(const FreezingVoteTokenImpl&) = delete;

 private:
  // Implementation that lives on the PM sequence.
  std::unique_ptr<FreezingVoteTokenPMImpl, base::OnTaskRunnerDeleter> pm_impl_;
};

}  // namespace

FreezingVoteToken::FreezingVoteToken() = default;
FreezingVoteToken::~FreezingVoteToken() = default;

FreezingVoteTokenPMImpl::FreezingVoteTokenPMImpl(content::WebContents* content,
                                                 FreezingVoteValue vote_value,
                                                 const char* vote_reason) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  // Register the vote on the PM sequence.
  PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindOnce(
          &FreezingVoteTokenPMImpl::InitializeOnGraph,
          // It's safe to use Unretained because |this| can only be deleted
          // from a task running on the PM sequence after this callback.
          base::Unretained(this),
          PerformanceManager::GetPageNodeForWebContents(content),
          FreezingVote(vote_value, vote_reason)));
}

FreezingVoteTokenPMImpl::~FreezingVoteTokenPMImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Reset();
}

void FreezingVoteTokenPMImpl::InitializeOnGraph(
    base::WeakPtr<PageNode> page_node,
    FreezingVote vote,
    Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // No need to initialize if the page node doesn't exist anymore.
  if (!page_node)
    return;

  page_node_ = page_node.get();
  page_node_observation_.Observe(graph);

  voting_channel_ = graph->GetRegisteredObjectAs<FreezingVoteAggregator>()
                        ->GetVotingChannel();
  voting_channel_.SubmitVote(page_node_, vote);
}

void FreezingVoteTokenPMImpl::OnBeforePageNodeRemoved(
    const PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (page_node != page_node_)
    return;

  // Invalidate the vote if its associated page node is destroyed. This can
  // happen if a freezing vote token is released after the destruction of the
  // WebContents it's associated with.
  Reset();
}

void FreezingVoteTokenPMImpl::Reset() {
  if (!page_node_)
    return;

  voting_channel_.InvalidateVote(page_node_);
  voting_channel_.Reset();
  page_node_observation_.Reset();
  page_node_ = nullptr;
}

FreezingVoteTokenImpl::FreezingVoteTokenImpl(content::WebContents* content,
                                             FreezingVoteValue vote_value,
                                             const char* vote_reason)
    : pm_impl_(new FreezingVoteTokenPMImpl(content, vote_value, vote_reason),
               base::OnTaskRunnerDeleter(PerformanceManager::GetTaskRunner())) {
}

FreezingVoteTokenImpl::~FreezingVoteTokenImpl() = default;

std::unique_ptr<FreezingVoteToken> EmitFreezingVoteForWebContents(
    content::WebContents* content,
    FreezingVoteValue vote_value,
    const char* vote_reason) {
  return std::make_unique<FreezingVoteTokenImpl>(content, vote_value,
                                                 vote_reason);
}

const char* FreezingVoteValueToString(FreezingVoteValue freezing_vote_value) {
  if (freezing_vote_value == freezing::FreezingVoteValue::kCanFreeze) {
    return "kCanFreeze";
  } else {
    return "kCannotFreeze";
  }
}

}  // namespace freezing
}  // namespace performance_manager
