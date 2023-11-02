// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/freezing/freezing.h"

#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/test_support/performance_manager_test_harness.h"
#include "components/performance_manager/test_support/test_harness_helper.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace performance_manager {
namespace freezing {

namespace {

constexpr char kCanFreeze[] = "Can freeze";
constexpr char kCannotFreeze[] = "Cannot freeze";

// Get the aggregated freezing vote associated with |contents|.
absl::optional<FreezingVote> GetFreezingVote(content::WebContents* contents) {
  base::RunLoop run_loop;
  absl::optional<FreezingVote> ret;
  auto quit_closure = run_loop.QuitClosure();
  PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<PageNode> page_node, base::OnceClosure quit_closure,
             absl::optional<FreezingVote>* expected_vote) {
            EXPECT_TRUE(page_node);
            auto vote = page_node->GetFreezingVote();
            *expected_vote = vote;
            std::move(quit_closure).Run();
          },
          PerformanceManager::GetPrimaryPageNodeForWebContents(contents),
          std::move(quit_closure), &ret));
  run_loop.Run();
  return ret;
}

// Get the number of freezing votes associated with |contents|.
size_t GetVoteCount(content::WebContents* contents) {
  base::RunLoop run_loop;
  size_t ret = 0;
  auto quit_closure = run_loop.QuitClosure();
  PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<PageNode> page_node, base::OnceClosure quit_closure,
             size_t* vote_count) {
            EXPECT_TRUE(page_node);
            *vote_count =
                FreezingVoteCountForPageOnPMForTesting(page_node.get());
            std::move(quit_closure).Run();
          },
          PerformanceManager::GetPrimaryPageNodeForWebContents(contents),
          std::move(quit_closure), &ret));
  run_loop.Run();
  return ret;
}

// Get the total number of freezing votes.
size_t GetTotalVoteCount() {
  base::RunLoop run_loop;
  size_t ret = 0;
  auto quit_closure = run_loop.QuitClosure();
  PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindOnce(
          [](base::OnceClosure quit_closure, size_t* vote_count, Graph* graph) {
            *vote_count = TotalFreezingVoteCountOnPMForTesting(graph);
            std::move(quit_closure).Run();
          },
          std::move(quit_closure), &ret));
  run_loop.Run();
  return ret;
}

}  // namespace

class FreezingTest : public PerformanceManagerTestHarness {
 public:
  FreezingTest() = default;
  ~FreezingTest() override = default;
  FreezingTest(const FreezingTest& other) = delete;
  FreezingTest& operator=(const FreezingTest&) = delete;

  void SetUp() override {
    GetGraphFeatures().EnableFreezingVoteDecorator();
    PerformanceManagerTestHarness::SetUp();
    SetContents(CreateTestWebContents());
  }
};

TEST_F(FreezingTest, FreezingToken) {
  {
    // Emit a positive freezing vote, this should make the page node freezable.
    auto token = EmitFreezingVoteForWebContents(
        web_contents(), FreezingVoteValue::kCanFreeze, kCanFreeze);
    EXPECT_EQ(1U, GetVoteCount(web_contents()));
    EXPECT_EQ(1U, GetTotalVoteCount());
    EXPECT_EQ(GetFreezingVote(web_contents()),
              FreezingVote(FreezingVoteValue::kCanFreeze, kCanFreeze));
  }
  // Once the freezing vote token is destroyed the vote should be invalidated.
  EXPECT_EQ(GetFreezingVote(web_contents()), absl::nullopt);
  EXPECT_EQ(0U, GetVoteCount(web_contents()));
  EXPECT_EQ(0U, GetTotalVoteCount());

  // Same test but for a negative freezing vote.
  {
    auto token = EmitFreezingVoteForWebContents(
        web_contents(), FreezingVoteValue::kCannotFreeze, kCannotFreeze);
    EXPECT_EQ(1U, GetVoteCount(web_contents()));
    EXPECT_EQ(1U, GetTotalVoteCount());
    EXPECT_EQ(GetFreezingVote(web_contents()),
              FreezingVote(FreezingVoteValue::kCannotFreeze, kCannotFreeze));
  }
  EXPECT_EQ(GetFreezingVote(web_contents()), absl::nullopt);
  EXPECT_EQ(0U, GetTotalVoteCount());

  // Emit multiple positive token for the same page.
  {
    auto token1 = EmitFreezingVoteForWebContents(
        web_contents(), FreezingVoteValue::kCanFreeze, kCanFreeze);
    EXPECT_EQ(1U, GetVoteCount(web_contents()));
    EXPECT_EQ(1U, GetTotalVoteCount());
    auto token2 = EmitFreezingVoteForWebContents(
        web_contents(), FreezingVoteValue::kCanFreeze, kCanFreeze);
    EXPECT_EQ(2U, GetVoteCount(web_contents()));
    EXPECT_EQ(2U, GetTotalVoteCount());
    auto token3 = EmitFreezingVoteForWebContents(
        web_contents(), FreezingVoteValue::kCanFreeze, kCanFreeze);
    EXPECT_EQ(GetFreezingVote(web_contents()),
              FreezingVote(FreezingVoteValue::kCanFreeze, kCanFreeze));
    EXPECT_EQ(3U, GetVoteCount(web_contents()));
    EXPECT_EQ(3U, GetTotalVoteCount());
    token3.reset();
    EXPECT_EQ(2U, GetVoteCount(web_contents()));
    EXPECT_EQ(2U, GetTotalVoteCount());
    EXPECT_EQ(GetFreezingVote(web_contents()),
              FreezingVote(FreezingVoteValue::kCanFreeze, kCanFreeze));
    token2.reset();
    EXPECT_EQ(1U, GetVoteCount(web_contents()));
    EXPECT_EQ(1U, GetTotalVoteCount());
    EXPECT_EQ(GetFreezingVote(web_contents()),
              FreezingVote(FreezingVoteValue::kCanFreeze, kCanFreeze));
    token1.reset();
    EXPECT_EQ(0U, GetVoteCount(web_contents()));
    EXPECT_EQ(0U, GetTotalVoteCount());
    EXPECT_EQ(GetFreezingVote(web_contents()), absl::nullopt);
  }
}

TEST_F(FreezingTest, WebContentsDestroyedBeforeToken) {
  // Emit a positive freezing vote, this should make the page node freezable.
  auto token = EmitFreezingVoteForWebContents(
      web_contents(), FreezingVoteValue::kCanFreeze, kCanFreeze);
  EXPECT_EQ(GetFreezingVote(web_contents()),
            FreezingVote(FreezingVoteValue::kCanFreeze, kCanFreeze));
  DeleteContents();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0U, GetTotalVoteCount());
  token.reset();
  EXPECT_EQ(0U, GetTotalVoteCount());
}

TEST_F(FreezingTest, FreezingTokenMultiplePages) {
  auto contents2 = CreateTestWebContents();
  auto contents3 = CreateTestWebContents();

  auto contents1_token1 = EmitFreezingVoteForWebContents(
      web_contents(), FreezingVoteValue::kCanFreeze, kCanFreeze);
  EXPECT_EQ(GetFreezingVote(web_contents()),
            FreezingVote(FreezingVoteValue::kCanFreeze, kCanFreeze));
  EXPECT_EQ(GetFreezingVote(contents2.get()), absl::nullopt);
  EXPECT_EQ(GetFreezingVote(contents3.get()), absl::nullopt);
  EXPECT_EQ(1U, GetVoteCount(web_contents()));
  EXPECT_EQ(0U, GetVoteCount(contents2.get()));
  EXPECT_EQ(0U, GetVoteCount(contents3.get()));
  EXPECT_EQ(1U, GetTotalVoteCount());

  auto contents1_token2 = EmitFreezingVoteForWebContents(
      web_contents(), FreezingVoteValue::kCanFreeze, kCanFreeze);
  EXPECT_EQ(GetFreezingVote(web_contents()),
            FreezingVote(FreezingVoteValue::kCanFreeze, kCanFreeze));
  EXPECT_EQ(GetFreezingVote(contents2.get()), absl::nullopt);
  EXPECT_EQ(GetFreezingVote(contents3.get()), absl::nullopt);
  EXPECT_EQ(2U, GetVoteCount(web_contents()));
  EXPECT_EQ(0U, GetVoteCount(contents2.get()));
  EXPECT_EQ(0U, GetVoteCount(contents3.get()));
  EXPECT_EQ(2U, GetTotalVoteCount());

  auto contents2_token = EmitFreezingVoteForWebContents(
      contents2.get(), FreezingVoteValue::kCanFreeze, kCanFreeze);
  EXPECT_EQ(GetFreezingVote(web_contents()),
            FreezingVote(FreezingVoteValue::kCanFreeze, kCanFreeze));
  EXPECT_EQ(GetFreezingVote(contents2.get()),
            FreezingVote(FreezingVoteValue::kCanFreeze, kCanFreeze));
  EXPECT_EQ(GetFreezingVote(contents3.get()), absl::nullopt);
  EXPECT_EQ(2U, GetVoteCount(web_contents()));
  EXPECT_EQ(1U, GetVoteCount(contents2.get()));
  EXPECT_EQ(0U, GetVoteCount(contents3.get()));
  EXPECT_EQ(3U, GetTotalVoteCount());

  auto contents3_token = EmitFreezingVoteForWebContents(
      contents3.get(), FreezingVoteValue::kCanFreeze, kCanFreeze);
  EXPECT_EQ(GetFreezingVote(web_contents()),
            FreezingVote(FreezingVoteValue::kCanFreeze, kCanFreeze));
  EXPECT_EQ(GetFreezingVote(contents2.get()),
            FreezingVote(FreezingVoteValue::kCanFreeze, kCanFreeze));
  EXPECT_EQ(GetFreezingVote(contents3.get()),
            FreezingVote(FreezingVoteValue::kCanFreeze, kCanFreeze));
  EXPECT_EQ(2U, GetVoteCount(web_contents()));
  EXPECT_EQ(1U, GetVoteCount(contents2.get()));
  EXPECT_EQ(1U, GetVoteCount(contents3.get()));
  EXPECT_EQ(4U, GetTotalVoteCount());

  contents1_token1.reset();
  EXPECT_EQ(GetFreezingVote(web_contents()),
            FreezingVote(FreezingVoteValue::kCanFreeze, kCanFreeze));
  EXPECT_EQ(GetFreezingVote(contents2.get()),
            FreezingVote(FreezingVoteValue::kCanFreeze, kCanFreeze));
  EXPECT_EQ(GetFreezingVote(contents3.get()),
            FreezingVote(FreezingVoteValue::kCanFreeze, kCanFreeze));
  EXPECT_EQ(1U, GetVoteCount(web_contents()));
  EXPECT_EQ(1U, GetVoteCount(contents2.get()));
  EXPECT_EQ(1U, GetVoteCount(contents3.get()));
  EXPECT_EQ(3U, GetTotalVoteCount());

  contents1_token2.reset();
  EXPECT_EQ(GetFreezingVote(web_contents()), absl::nullopt);
  EXPECT_EQ(GetFreezingVote(contents2.get()),
            FreezingVote(FreezingVoteValue::kCanFreeze, kCanFreeze));
  EXPECT_EQ(GetFreezingVote(contents3.get()),
            FreezingVote(FreezingVoteValue::kCanFreeze, kCanFreeze));
  EXPECT_EQ(0U, GetVoteCount(web_contents()));
  EXPECT_EQ(1U, GetVoteCount(contents2.get()));
  EXPECT_EQ(1U, GetVoteCount(contents3.get()));
  EXPECT_EQ(2U, GetTotalVoteCount());

  contents2_token.reset();
  EXPECT_EQ(GetFreezingVote(web_contents()), absl::nullopt);
  EXPECT_EQ(GetFreezingVote(contents2.get()), absl::nullopt);
  EXPECT_EQ(GetFreezingVote(contents3.get()),
            FreezingVote(FreezingVoteValue::kCanFreeze, kCanFreeze));
  EXPECT_EQ(0U, GetVoteCount(web_contents()));
  EXPECT_EQ(0U, GetVoteCount(contents2.get()));
  EXPECT_EQ(1U, GetVoteCount(contents3.get()));
  EXPECT_EQ(1U, GetTotalVoteCount());

  contents3_token.reset();
  EXPECT_EQ(GetFreezingVote(web_contents()), absl::nullopt);
  EXPECT_EQ(GetFreezingVote(contents2.get()), absl::nullopt);
  EXPECT_EQ(GetFreezingVote(contents3.get()), absl::nullopt);
  EXPECT_EQ(0U, GetVoteCount(web_contents()));
  EXPECT_EQ(0U, GetVoteCount(contents2.get()));
  EXPECT_EQ(0U, GetVoteCount(contents3.get()));
  EXPECT_EQ(0U, GetTotalVoteCount());
}

}  // namespace freezing
}  // namespace performance_manager
