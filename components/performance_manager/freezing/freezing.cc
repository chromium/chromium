// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/freezing/freezing.h"

#include <memory>

#include "base/bind.h"
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

  // PageNodeObserver:
  void OnBeforePageNodeRemoved(const PageNode* page_node) override;

 private:
  const PageNode* page_node_ = nullptr;
  Graph* graph_ = nullptr;

  // Voting channel wrapper. This objects should only be used on the PM
  // sequence.
  std::unique_ptr<FreezingVotingChannelWrapper> voter_;

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
  // Voting channel wrapper. This objects should only be used on the PM
  // sequence.
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
          [](base::WeakPtr<PageNode> page_node, FreezingVoteValue vote_value,
             const char* vote_reason, FreezingVoteTokenPMImpl* voter_pm_impl,
             Graph* graph) {
            voter_pm_impl->voter_ =
                std::make_unique<FreezingVotingChannelWrapper>();
            voter_pm_impl->graph_ = graph;
            graph->AddPageNodeObserver(voter_pm_impl);
            voter_pm_impl->voter_->SetVotingChannel(
                graph->GetRegisteredObjectAs<FreezingVoteAggregator>()
                    ->GetVotingChannel());
            if (page_node) {
              voter_pm_impl->voter_->SubmitVote(page_node.get(),
                                                {vote_value, vote_reason});
              voter_pm_impl->page_node_ = page_node.get();
            }
          },
          PerformanceManager::GetPageNodeForWebContents(content), vote_value,
          // It's safe to use Unretained because |vote_reason| is a static
          // string.
          base::Unretained(vote_reason),
          // It's safe to use Unretained because |this| can only be deleted
          // from a task running on the PM sequence after this callback.
          base::Unretained(this)));
}

FreezingVoteTokenPMImpl::~FreezingVoteTokenPMImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (graph_)
    graph_->RemovePageNodeObserver(this);
}

void FreezingVoteTokenPMImpl::OnBeforePageNodeRemoved(
    const PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (page_node == page_node_) {
    // Invalidate the vote if its associated page node is destroyed. This can
    // happen if a freezing vote token is released after the destruction of the
    // WebContents it's associated with.
    voter_->InvalidateVote(page_node);
    page_node_ = nullptr;
    graph_->RemovePageNodeObserver(this);
    graph_ = nullptr;
  }
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