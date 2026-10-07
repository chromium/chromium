// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/execution_context_priority/priority_voting_system.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/process/process.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/worker_node_impl.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager::execution_context_priority {

namespace {

constexpr char kFrameReason[] = "frame reason";
constexpr char kWorkerReason[] = "worker reason";

// A voter that lets the test cast votes through its voting channel.
class TestPriorityVoter : public PriorityVoter {
 public:
  // `voting_channel` is set to point to the voting channel of this voter.
  explicit TestPriorityVoter(raw_ptr<VotingChannel>* voting_channel) {
    *voting_channel = &voting_channel_;
  }
  ~TestPriorityVoter() override = default;

  // PriorityVoter:
  void InitializeOnGraph(Graph* graph, VotingChannel voting_channel) override {
    voting_channel_ = std::move(voting_channel);
  }
  void TearDownOnGraph(Graph* graph) override { voting_channel_.Reset(); }

 private:
  VotingChannel voting_channel_;
};

class PriorityVotingSystemTest : public GraphTestHarness {
 public:
  using Super = GraphTestHarness;

  void SetUp() override {
    Super::SetUp();
    auto* priority_voting_system =
        graph()->PassToGraph(std::make_unique<PriorityVotingSystem>());
    priority_voting_system->AddPriorityVoter<TestPriorityVoter>(&voter_);
  }

  void TearDown() override {
    // `voter_` points into the TestPriorityVoter, which is destroyed with the
    // graph.
    voter_ = nullptr;
    Super::TearDown();
  }

  // The voting channel of the TestPriorityVoter, owned by the graph.
  raw_ptr<VotingChannel> voter_ = nullptr;
};

}  // namespace

// Tests that worker votes go through the whole voting system.
TEST_F(PriorityVotingSystemTest, WorkerVote) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  auto worker = CreateNode<WorkerNodeImpl>(WorkerNode::WorkerType::kDedicated,
                                           mock_graph.process.get());

  const Vote kWorkerVote(base::Process::Priority::kUserBlocking, kWorkerReason);
  voter_->SetVote(worker.get(), kWorkerVote);
  EXPECT_EQ(worker->GetPriorityAndReason(),
            PriorityAndReason(kWorkerVote.value(), kWorkerReason));

  voter_->SetVote(worker.get(), std::nullopt);
  EXPECT_EQ(worker->GetPriorityAndReason(),
            PriorityAndReason(base::Process::Priority::kMinValue,
                              WorkerNodeImpl::kDefaultPriorityReason));
}

// Tests that a voter observing the graph after the PriorityVotingSystem can
// remove its vote on a frame in OnBeforeFrameNodeRemoved(). The leak check is
// done in OnFrameNodeRemoved(), so this doesn't depend on the observer order.
TEST_F(PriorityVotingSystemTest, LeakCheckIsIndependentOfObserverOrder) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  auto child_frame = CreateFrameNodeAutoId(
      mock_graph.process.get(), mock_graph.page.get(), mock_graph.frame.get());

  voter_->SetVote(child_frame.get(),
                  Vote(base::Process::Priority::kUserBlocking, kFrameReason));

  class VoteRemover : public FrameNodeObserver {
   public:
    explicit VoteRemover(VotingChannel* voting_channel)
        : voting_channel_(voting_channel) {}

    void OnBeforeFrameNodeRemoved(const FrameNode* frame_node) override {
      voting_channel_->SetVote(frame_node, std::nullopt);
    }

   private:
    raw_ptr<VotingChannel> voting_channel_;
  };
  VoteRemover vote_remover(voter_);
  graph()->AddFrameNodeObserver(&vote_remover);

  // This would hit the CHECK that no votes remain on a removed frame if the
  // vote wasn't removed.
  child_frame.reset();

  graph()->RemoveFrameNodeObserver(&vote_remover);
}

namespace {

// Casts `vote` on `target` when the priority of `frame_node` is raised above
// the lowest priority, and removes it when it goes back to the lowest priority.
template <typename TargetNodeType>
class VoteOnFramePriorityChange : public FrameNodeObserver {
 public:
  VoteOnFramePriorityChange(const FrameNode* frame_node,
                            const TargetNodeType* target,
                            Vote vote,
                            VotingChannel* voting_channel)
      : frame_node_(frame_node),
        target_(target),
        vote_(vote),
        voting_channel_(voting_channel) {}

  void OnPriorityAndReasonChanged(
      const FrameNode* frame_node,
      const PriorityAndReason& previous_value) override {
    if (frame_node != frame_node_) {
      return;
    }
    voting_channel_->SetVote(target_.get(),
                             frame_node->GetPriorityAndReason().priority() ==
                                     base::Process::Priority::kMinValue
                                 ? std::nullopt
                                 : std::make_optional(vote_));
  }

 private:
  raw_ptr<const FrameNode> frame_node_;
  raw_ptr<const TargetNodeType> target_;
  Vote vote_;
  raw_ptr<VotingChannel> voting_channel_;
};

}  // namespace

// Tests that a voter can change the vote of a frame while the priority of
// another frame is being applied, like InheritParentPriorityVoter does.
TEST_F(PriorityVotingSystemTest, FrameVoteChangedWhileApplyingFrameVote) {
  MockSinglePageWithMultipleProcessesGraph mock_graph(graph());
  auto* frame = mock_graph.frame.get();
  auto* child_frame = mock_graph.child_frame.get();

  const Vote kVote(base::Process::Priority::kUserBlocking, kFrameReason);
  VoteOnFramePriorityChange<FrameNode> vote_on_child_frame(frame, child_frame,
                                                           kVote, voter_);
  graph()->AddFrameNodeObserver(&vote_on_child_frame);

  voter_->SetVote(frame, kVote);
  EXPECT_EQ(child_frame->GetPriorityAndReason(),
            PriorityAndReason(kVote.value(), kFrameReason));

  voter_->SetVote(frame, std::nullopt);
  EXPECT_EQ(child_frame->GetPriorityAndReason(),
            PriorityAndReason(base::Process::Priority::kMinValue,
                              FrameNodeImpl::kDefaultPriorityReason));

  graph()->RemoveFrameNodeObserver(&vote_on_child_frame);
}

}  // namespace performance_manager::execution_context_priority
