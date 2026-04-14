// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/execution_context_priority/force_foreground_voter_for_origins.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/graph/worker_node_impl.h"
#include "components/performance_manager/public/execution_context/execution_context.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/voting.h"
#include "components/url_matcher/url_matcher.h"
#include "components/url_matcher/url_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace performance_manager::execution_context_priority {

using DummyVoteObserver = voting::test::DummyVoteObserver<Vote>;

namespace {

const char kBrowserContextId[] = "browser_context_id";

class ForceForegroundVoterForOriginsTest : public GraphTestHarness {
 public:
  using Super = GraphTestHarness;

  ForceForegroundVoterForOriginsTest() = default;
  ~ForceForegroundVoterForOriginsTest() override = default;

  ForceForegroundVoterForOriginsTest(
      const ForceForegroundVoterForOriginsTest&) = delete;
  ForceForegroundVoterForOriginsTest& operator=(
      const ForceForegroundVoterForOriginsTest&) = delete;

  void SetUp() override {
    Super::SetUp();
    voter_ = std::make_unique<ForceForegroundVoterForOrigins>();
    voter_->InitializeOnGraph(graph(), observer_.BuildVotingChannel());
  }

  void SetPatterns(const std::string& browser_context_id,
                   const std::vector<std::string>& patterns_vector) {
    base::ListValue patterns;
    for (const auto& pattern : patterns_vector) {
      patterns.Append(pattern);
    }
    voter_->SetPatternsForProfile(browser_context_id, patterns);
  }

  void TearDown() override {
    if (voter_) {
      voter_->TearDownOnGraph(graph());
    }
    Super::TearDown();
  }

  VoterId voter_id() const { return voter_->voter_id(); }

  DummyVoteObserver observer_;
  std::unique_ptr<ForceForegroundVoterForOrigins> voter_;
};

}  // namespace

TEST_F(ForceForegroundVoterForOriginsTest, MatchingUrl) {
  SetPatterns(kBrowserContextId, {"example.com"});

  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>(nullptr, kBrowserContextId);
  auto frame = CreateFrameNodeAutoId(process.get(), page.get());

  // URL doesn't match yet.
  EXPECT_EQ(observer_.GetVoteCount(), 0u);

  // URL matches.
  frame->OnNavigationCommitted(GURL("https://example.com/index.html"),
                               url::Origin::Create(GURL("https://example.com")),
                               /*same_document=*/false,
                               /*is_served_from_back_forward_cache=*/false);
  EXPECT_EQ(observer_.GetVoteCount(), 1u);
  EXPECT_TRUE(observer_.HasVote(
      voter_id(), execution_context::ExecutionContext::From(frame.get()),
      base::Process::Priority::kUserBlocking,
      ForceForegroundVoterForOrigins::kForceForegroundReason));

  // URL no longer matches.
  frame->OnNavigationCommitted(GURL("https://other.com/"),
                               url::Origin::Create(GURL("https://other.com")),
                               /*same_document=*/false,
                               /*is_served_from_back_forward_cache=*/false);
  EXPECT_EQ(observer_.GetVoteCount(), 0u);
}

TEST_F(ForceForegroundVoterForOriginsTest, InitialUrlMatch) {
  SetPatterns(kBrowserContextId, {"example.com"});

  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>(nullptr, kBrowserContextId);
  auto frame = CreateFrameNodeAutoId(process.get(), page.get());
  frame->OnNavigationCommitted(GURL("https://example.com/"),
                               url::Origin::Create(GURL("https://example.com")),
                               /*same_document=*/false,
                               /*is_served_from_back_forward_cache=*/false);

  // The voter should have been notified during frame creation, but at that
  // point the URL was empty. When the URL is set, OnURLChanged should trigger
  // the vote.
  EXPECT_EQ(observer_.GetVoteCount(), 1u);
  EXPECT_TRUE(observer_.HasVote(
      voter_id(), execution_context::ExecutionContext::From(frame.get()),
      base::Process::Priority::kUserBlocking,
      ForceForegroundVoterForOrigins::kForceForegroundReason));
}

TEST_F(ForceForegroundVoterForOriginsTest, WorkerMatchingUrl) {
  SetPatterns(kBrowserContextId, {"example.com"});

  auto process = CreateNode<ProcessNodeImpl>();
  auto worker = CreateNode<WorkerNodeImpl>(
      WorkerNode::WorkerType::kDedicated, process.get(), kBrowserContextId,
      blink::WorkerToken(), url::Origin::Create(GURL("https://example.com")));

  // Dedicated workers get their URL when the script is fetched.
  EXPECT_EQ(observer_.GetVoteCount(), 0u);

  worker->OnFinalResponseURLDetermined(GURL("https://example.com/worker.js"));
  EXPECT_EQ(observer_.GetVoteCount(), 1u);
  EXPECT_TRUE(observer_.HasVote(
      voter_id(), execution_context::ExecutionContext::From(worker.get()),
      base::Process::Priority::kUserBlocking,
      ForceForegroundVoterForOrigins::kForceForegroundReason));
}

TEST_F(ForceForegroundVoterForOriginsTest, MultipleProfiles) {
  const char kProfile1[] = "profile1";
  const char kProfile2[] = "profile2";

  SetPatterns(kProfile1, {"example.com"});
  SetPatterns(kProfile2, {"other.com"});

  auto process = CreateNode<ProcessNodeImpl>();
  auto page1 = CreateNode<PageNodeImpl>(nullptr, kProfile1);
  auto frame1 = CreateFrameNodeAutoId(process.get(), page1.get());
  frame1->OnNavigationCommitted(
      GURL("https://example.com/"),
      url::Origin::Create(GURL("https://example.com")),
      /*same_document=*/false,
      /*is_served_from_back_forward_cache=*/false);

  auto page2 = CreateNode<PageNodeImpl>(nullptr, kProfile2);
  auto frame2 = CreateFrameNodeAutoId(process.get(), page2.get());
  frame2->OnNavigationCommitted(GURL("https://other.com/"),
                                url::Origin::Create(GURL("https://other.com")),
                                /*same_document=*/false,
                                /*is_served_from_back_forward_cache=*/false);

  EXPECT_EQ(observer_.GetVoteCount(), 2u);

  // frame1 matches its profile patterns.
  EXPECT_TRUE(observer_.HasVote(
      voter_id(), execution_context::ExecutionContext::From(frame1.get()),
      base::Process::Priority::kUserBlocking,
      ForceForegroundVoterForOrigins::kForceForegroundReason));

  // frame2 matches its profile patterns.
  EXPECT_TRUE(observer_.HasVote(
      voter_id(), execution_context::ExecutionContext::From(frame2.get()),
      base::Process::Priority::kUserBlocking,
      ForceForegroundVoterForOrigins::kForceForegroundReason));

  // Swap URLs: frame1 should no longer match (it's in profile1, but URL matches
  // profile2 patterns).
  frame1->OnNavigationCommitted(GURL("https://other.com/"),
                                url::Origin::Create(GURL("https://other.com")),
                                /*same_document=*/false,
                                /*is_served_from_back_forward_cache=*/false);
  EXPECT_EQ(observer_.GetVoteCount(), 1u);
  EXPECT_FALSE(observer_.HasVote(
      voter_id(), execution_context::ExecutionContext::From(frame1.get()),
      base::Process::Priority::kUserBlocking,
      ForceForegroundVoterForOrigins::kForceForegroundReason));
}

TEST_F(ForceForegroundVoterForOriginsTest, OriginOnlyMatching) {
  // Pattern with a path.
  SetPatterns(kBrowserContextId, {"example.com/path"});

  auto process = CreateNode<ProcessNodeImpl>();
  auto page = CreateNode<PageNodeImpl>(nullptr, kBrowserContextId);
  auto frame = CreateFrameNodeAutoId(process.get(), page.get());

  // Should match even with a different path.
  frame->OnNavigationCommitted(GURL("https://example.com/other"),
                               url::Origin::Create(GURL("https://example.com")),
                               /*same_document=*/false,
                               /*is_served_from_back_forward_cache=*/false);

  EXPECT_EQ(observer_.GetVoteCount(), 1u);
}

}  // namespace performance_manager::execution_context_priority
