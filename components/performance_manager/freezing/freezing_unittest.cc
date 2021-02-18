// Copyright 2020 The Chromium Authors. All rights reserved.
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

namespace performance_manager {
namespace freezing {

namespace {

constexpr char kCanFreeze[] = "Can freeze";
constexpr char kCannotFreeze[] = "Cannot freeze";

// Check that the freezing vote attached to the page node associated with
// |content| has the expected value.
void ExpectFreezingVote(content::WebContents* content,
                        base::Optional<FreezingVote> expected_vote) {
  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<PageNode> page_node, base::OnceClosure quit_closure,
             base::Optional<FreezingVote> expected_vote) {
            EXPECT_TRUE(page_node);
            auto vote = page_node->GetFreezingVote();
            EXPECT_EQ(expected_vote, vote);
            std::move(quit_closure).Run();
          },
          PerformanceManager::GetPageNodeForWebContents(content),
          std::move(quit_closure), expected_vote));
  run_loop.Run();
}

}  // namespace

class FreezingTest : public PerformanceManagerTestHarness {
 public:
  FreezingTest() = default;
  ~FreezingTest() override = default;
  FreezingTest(const FreezingTest& other) = delete;
  FreezingTest& operator=(const FreezingTest&) = delete;

  void SetUp() override {
    GetGraphFeaturesHelper().EnableFreezingVoteDecorator();
    PerformanceManagerTestHarness::SetUp();
    SetContents(CreateTestWebContents());
  }
};

TEST_F(FreezingTest, FreezingToken) {
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  EXPECT_TRUE(web_contents_tester);
  web_contents_tester->NavigateAndCommit(GURL("https:/foo.com"));

  {
    // Emit a positive freezing vote, this should make the page node freezable.
    auto token = EmitFreezingVoteForWebContents(
        web_contents(), FreezingVoteValue::kCanFreeze, kCanFreeze);
    ExpectFreezingVote(web_contents(),
                       FreezingVote(FreezingVoteValue::kCanFreeze, kCanFreeze));
  }
  // Once the freezing vote token is destroyed the vote should be invalidated.
  ExpectFreezingVote(web_contents(), base::nullopt);

  // Same test but for a negative freezing vote.
  {
    auto token = EmitFreezingVoteForWebContents(
        web_contents(), FreezingVoteValue::kCannotFreeze, kCannotFreeze);
    ExpectFreezingVote(
        web_contents(),
        FreezingVote(FreezingVoteValue::kCannotFreeze, kCannotFreeze));
  }
  ExpectFreezingVote(web_contents(), base::nullopt);
}

TEST_F(FreezingTest, WebContentsDestroyedBeforeToken) {
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  EXPECT_TRUE(web_contents_tester);
  web_contents_tester->NavigateAndCommit(GURL("https:/foo.com"));

  // Emit a positive freezing vote, this should make the page node freezable.
  auto token = EmitFreezingVoteForWebContents(
      web_contents(), FreezingVoteValue::kCanFreeze, kCanFreeze);
  ExpectFreezingVote(web_contents(),
                     FreezingVote(FreezingVoteValue::kCanFreeze, kCanFreeze));
  DeleteContents();
  base::RunLoop().RunUntilIdle();
}

}  // namespace freezing
}  // namespace performance_manager
