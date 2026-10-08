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
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/graph/worker_node_impl.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager::execution_context_priority {

namespace {

constexpr char kFrameReason[] = "frame reason";
constexpr char kPageReason[] = "page reason";
constexpr char kProcessReason[] = "process reason";
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

// Tests that a page vote goes through the whole voting system and is applied to
// every frame of the page.
TEST_F(PriorityVotingSystemTest, PageVoteAppliedToFrames) {
  MockSinglePageWithMultipleProcessesGraph mock_graph(graph());
  auto* page = mock_graph.page.get();
  auto* frame = mock_graph.frame.get();
  auto* child_frame = mock_graph.child_frame.get();

  const PriorityAndReason kPagePriorityAndReason(
      base::Process::Priority::kUserBlocking, kPageReason);
  voter_->SetVote(page,
                  Vote(base::Process::Priority::kUserBlocking, kPageReason));
  EXPECT_EQ(frame->GetPriorityAndReason(), kPagePriorityAndReason);
  EXPECT_EQ(child_frame->GetPriorityAndReason(), kPagePriorityAndReason);

  // Removing the page vote resets the frames to the default priority.
  const PriorityAndReason kDefaultPriorityAndReason(
      base::Process::Priority::kMinValue,
      FrameNodeImpl::kDefaultPriorityReason);
  voter_->SetVote(page, std::nullopt);
  EXPECT_EQ(frame->GetPriorityAndReason(), kDefaultPriorityAndReason);
  EXPECT_EQ(child_frame->GetPriorityAndReason(), kDefaultPriorityAndReason);
}

// Tests that the priority of a frame is the highest of its own vote and the
// vote of its page.
TEST_F(PriorityVotingSystemTest, HighestOfFrameAndPageVote) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  auto* page = mock_graph.page.get();
  auto* frame = mock_graph.frame.get();

  const Vote kLowFrameVote(base::Process::Priority::kBestEffort, kFrameReason);
  const Vote kHighFrameVote(base::Process::Priority::kUserBlocking,
                            kFrameReason);
  const Vote kMediumPageVote(base::Process::Priority::kUserVisible,
                             kPageReason);

  // The page vote wins over a lower frame vote.
  voter_->SetVote(frame, kLowFrameVote);
  voter_->SetVote(page, kMediumPageVote);
  EXPECT_EQ(frame->GetPriorityAndReason(),
            PriorityAndReason(kMediumPageVote.value(), kPageReason));

  // The frame vote wins over a lower page vote.
  voter_->SetVote(frame, kHighFrameVote);
  EXPECT_EQ(frame->GetPriorityAndReason(),
            PriorityAndReason(kHighFrameVote.value(), kFrameReason));

  // Removing the frame vote falls back to the page vote.
  voter_->SetVote(frame, std::nullopt);
  EXPECT_EQ(frame->GetPriorityAndReason(),
            PriorityAndReason(kMediumPageVote.value(), kPageReason));

  // Removing the page vote falls back to the frame vote, even if it has the
  // same priority as the default priority.
  voter_->SetVote(frame, kLowFrameVote);
  voter_->SetVote(page, std::nullopt);
  EXPECT_EQ(frame->GetPriorityAndReason(),
            PriorityAndReason(kLowFrameVote.value(), kFrameReason));

  voter_->SetVote(frame, std::nullopt);
}

// Tests that a page vote only applies to the frames of that page, and not to
// workers, even those whose client is a frame of the page. Workers only inherit
// the priority of their client frames through the InheritClientPriorityVoter,
// which is not added in this test.
TEST_F(PriorityVotingSystemTest, PageVoteOnlyAppliesToItsFrames) {
  MockMultiplePagesAndWorkersWithMultipleProcessesGraph mock_graph(graph());

  const PriorityAndReason kDefaultPriorityAndReason(
      base::Process::Priority::kMinValue,
      FrameNodeImpl::kDefaultPriorityReason);

  voter_->SetVote(mock_graph.page.get(),
                  Vote(base::Process::Priority::kUserBlocking, kPageReason));
  EXPECT_EQ(
      mock_graph.frame->GetPriorityAndReason(),
      PriorityAndReason(base::Process::Priority::kUserBlocking, kPageReason));
  EXPECT_EQ(mock_graph.other_frame->GetPriorityAndReason(),
            kDefaultPriorityAndReason);
  EXPECT_EQ(mock_graph.worker->GetPriorityAndReason().priority(),
            base::Process::Priority::kMinValue);

  voter_->SetVote(mock_graph.page.get(), std::nullopt);
  EXPECT_EQ(mock_graph.frame->GetPriorityAndReason(),
            kDefaultPriorityAndReason);
}

// Tests that a frame added to a page that has a vote gets that vote, and that
// the vote is removed when the frame is removed.
TEST_F(PriorityVotingSystemTest, PageVoteAppliesToAddedFrame) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  auto* page = mock_graph.page.get();

  const PriorityAndReason kPagePriorityAndReason(
      base::Process::Priority::kUserVisible, kPageReason);
  voter_->SetVote(page,
                  Vote(base::Process::Priority::kUserVisible, kPageReason));

  auto child_frame = CreateFrameNodeAutoId(mock_graph.process.get(), page,
                                           mock_graph.frame.get());
  EXPECT_EQ(child_frame->GetPriorityAndReason(), kPagePriorityAndReason);

  // Removing the frame removes its page vote. Otherwise, this would hit the
  // CHECK that no votes remain on a removed frame.
  child_frame.reset();
  EXPECT_EQ(mock_graph.frame->GetPriorityAndReason(), kPagePriorityAndReason);

  voter_->SetVote(page, std::nullopt);
}

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

// Tests that process votes go through the whole voting system.
TEST_F(PriorityVotingSystemTest, ProcessVote) {
  auto process = CreateRendererProcessNode();

  voter_->SetVote(process.get(),
                  Vote(base::Process::Priority::kUserVisible, kProcessReason));
  EXPECT_EQ(process->GetPriority(), base::Process::Priority::kUserVisible);

  voter_->SetVote(process.get(),
                  Vote(base::Process::Priority::kUserBlocking, kProcessReason));
  EXPECT_EQ(process->GetPriority(), base::Process::Priority::kUserBlocking);

  // Removing the vote resets the process to the lowest priority.
  voter_->SetVote(process.get(), std::nullopt);
  EXPECT_EQ(process->GetPriority(), base::Process::Priority::kMinValue);
}

// Tests that the priority of a process is the highest priority of the frames
// and workers it hosts.
TEST_F(PriorityVotingSystemTest, ProcessPriorityFromFramesAndWorkers) {
  MockMultiplePagesAndWorkersWithMultipleProcessesGraph mock_graph(graph());

  auto* proc1 = mock_graph.process.get();
  auto* proc2 = mock_graph.other_process.get();
  auto* frame1_1 = mock_graph.frame.get();
  auto* frame1_2 = mock_graph.other_frame.get();
  auto* frame2_1 = mock_graph.child_frame.get();
  auto* worker1 = mock_graph.worker.get();
  auto* worker2 = mock_graph.other_worker.get();

  const Vote kUserVisibleVote(base::Process::Priority::kUserVisible,
                              kFrameReason);
  const Vote kUserBlockingVote(base::Process::Priority::kUserBlocking,
                               kFrameReason);

  // Processes start with a high priority, and are lowered to the default
  // priority of the frames and workers they host, which have no votes.
  EXPECT_EQ(proc1->GetPriority(), base::Process::Priority::kMinValue);
  EXPECT_EQ(proc2->GetPriority(), base::Process::Priority::kMinValue);

  voter_->SetVote(frame1_1, kUserVisibleVote);
  EXPECT_EQ(proc1->GetPriority(), base::Process::Priority::kUserVisible);
  EXPECT_EQ(proc2->GetPriority(), base::Process::Priority::kMinValue);

  voter_->SetVote(frame2_1, kUserVisibleVote);
  EXPECT_EQ(proc1->GetPriority(), base::Process::Priority::kUserVisible);
  EXPECT_EQ(proc2->GetPriority(), base::Process::Priority::kUserVisible);

  // Another frame in process 1 with a higher priority wins.
  voter_->SetVote(frame1_2, kUserBlockingVote);
  EXPECT_EQ(proc1->GetPriority(), base::Process::Priority::kUserBlocking);

  // A worker in process 2 with a higher priority wins.
  voter_->SetVote(worker2, kUserBlockingVote);
  EXPECT_EQ(proc2->GetPriority(), base::Process::Priority::kUserBlocking);

  // Lowering the highest frame falls back to the other frame.
  voter_->SetVote(frame1_2, kUserVisibleVote);
  EXPECT_EQ(proc1->GetPriority(), base::Process::Priority::kUserVisible);

  // Removing the worker vote falls back to the frame.
  voter_->SetVote(worker2, std::nullopt);
  EXPECT_EQ(proc2->GetPriority(), base::Process::Priority::kUserVisible);

  // Removing all votes in process 2 resets it to the lowest priority.
  voter_->SetVote(frame2_1, std::nullopt);
  EXPECT_EQ(proc2->GetPriority(), base::Process::Priority::kMinValue);

  voter_->SetVote(frame1_1, std::nullopt);
  EXPECT_EQ(proc1->GetPriority(), base::Process::Priority::kUserVisible);
  voter_->SetVote(frame1_2, std::nullopt);
  EXPECT_EQ(proc1->GetPriority(), base::Process::Priority::kMinValue);

  // A worker alone dictates the priority of its process.
  voter_->SetVote(worker1, kUserVisibleVote);
  EXPECT_EQ(proc1->GetPriority(), base::Process::Priority::kUserVisible);
  voter_->SetVote(worker1, std::nullopt);
  EXPECT_EQ(proc1->GetPriority(), base::Process::Priority::kMinValue);
}

// Tests that adding and removing a frame updates the priority of its process.
TEST_F(PriorityVotingSystemTest, ProcessPriorityFollowsAddedAndRemovedFrame) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  auto* page = mock_graph.page.get();
  auto process = CreateRendererProcessNode();

  voter_->SetVote(page,
                  Vote(base::Process::Priority::kUserVisible, kPageReason));

  // The added frame inherits the page vote, which is cast on its process.
  auto child_frame =
      CreateFrameNodeAutoId(process.get(), page, mock_graph.frame.get());
  EXPECT_EQ(process->GetPriority(), base::Process::Priority::kUserVisible);

  // Removing the frame removes its vote on the process.
  child_frame.reset();
  EXPECT_EQ(process->GetPriority(), base::Process::Priority::kMinValue);

  voter_->SetVote(page, std::nullopt);
}

// Tests that a vote cast directly on a process is aggregated with the
// priorities of the frames it hosts.
TEST_F(PriorityVotingSystemTest, ProcessVoteAndFrameVote) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  auto* process = mock_graph.process.get();
  auto* frame = mock_graph.frame.get();

  voter_->SetVote(frame,
                  Vote(base::Process::Priority::kUserVisible, kFrameReason));
  voter_->SetVote(process,
                  Vote(base::Process::Priority::kUserBlocking, kProcessReason));
  EXPECT_EQ(process->GetPriority(), base::Process::Priority::kUserBlocking);

  // Removing the process vote falls back to the frame priority.
  voter_->SetVote(process, std::nullopt);
  EXPECT_EQ(process->GetPriority(), base::Process::Priority::kUserVisible);

  voter_->SetVote(frame, std::nullopt);
  EXPECT_EQ(process->GetPriority(), base::Process::Priority::kMinValue);
}

// Tests that adding and removing a worker updates the priority of its process.
TEST_F(PriorityVotingSystemTest, ProcessPriorityFollowsAddedAndRemovedWorker) {
  auto process = CreateRendererProcessNode();

  // A worker without votes lowers the process to the default priority.
  auto worker = CreateNode<WorkerNodeImpl>(WorkerNode::WorkerType::kDedicated,
                                           process.get());
  EXPECT_EQ(process->GetPriority(), base::Process::Priority::kMinValue);

  voter_->SetVote(worker.get(),
                  Vote(base::Process::Priority::kUserBlocking, kWorkerReason));
  EXPECT_EQ(process->GetPriority(), base::Process::Priority::kUserBlocking);

  // Removing the worker removes its vote on the process.
  voter_->SetVote(worker.get(), std::nullopt);
  worker.reset();
  EXPECT_EQ(process->GetPriority(), base::Process::Priority::kMinValue);

  // The ProcessPriorityAggregator removes its own vote on the process before
  // it is removed, so this doesn't hit the CHECK that no votes remain on a
  // removed process.
  process.reset();
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
  EXPECT_EQ(mock_graph.process->GetPriority(),
            base::Process::Priority::kUserBlocking);

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
  EXPECT_EQ(mock_graph.process->GetPriority(),
            base::Process::Priority::kMinValue);

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
