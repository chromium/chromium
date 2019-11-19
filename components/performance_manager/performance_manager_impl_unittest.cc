// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/performance_manager_impl.h"

#include <utility>

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/render_process_host_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

class PerformanceManagerImplTest : public testing::Test {
 public:
  PerformanceManagerImplTest() {}

  ~PerformanceManagerImplTest() override {}

  void SetUp() override {
    EXPECT_EQ(nullptr, PerformanceManagerImpl::GetInstance());
    performance_manager_ = PerformanceManagerImpl::Create(base::DoNothing());
    // Make sure creation registers the created instance.
    EXPECT_EQ(performance_manager_.get(),
              PerformanceManagerImpl::GetInstance());
  }

  void TearDown() override {
    if (performance_manager_) {
      PerformanceManagerImpl::Destroy(std::move(performance_manager_));
      // Make sure destruction unregisters the instance.
      EXPECT_EQ(nullptr, PerformanceManagerImpl::GetInstance());
    }

    task_environment_.RunUntilIdle();
  }

 protected:
  PerformanceManagerImpl* performance_manager() {
    return performance_manager_.get();
  }

 private:
  std::unique_ptr<PerformanceManagerImpl> performance_manager_;
  base::test::TaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(PerformanceManagerImplTest);
};

TEST_F(PerformanceManagerImplTest, InstantiateNodes) {
  int next_render_frame_id = 0;

  std::unique_ptr<ProcessNodeImpl> process_node =
      performance_manager()->CreateProcessNode(RenderProcessHostProxy());
  EXPECT_NE(nullptr, process_node.get());
  std::unique_ptr<PageNodeImpl> page_node =
      performance_manager()->CreatePageNode(WebContentsProxy(), std::string(),
                                            GURL(), false, false);
  EXPECT_NE(nullptr, page_node.get());

  // Create a node of each type.
  std::unique_ptr<FrameNodeImpl> frame_node =
      performance_manager()->CreateFrameNode(
          process_node.get(), page_node.get(), nullptr, 0,
          ++next_render_frame_id, base::UnguessableToken::Create(), 0, 0);
  EXPECT_NE(nullptr, frame_node.get());

  performance_manager()->DeleteNode(std::move(frame_node));
  performance_manager()->DeleteNode(std::move(page_node));
  performance_manager()->DeleteNode(std::move(process_node));
}

TEST_F(PerformanceManagerImplTest, BatchDeleteNodes) {
  int next_render_frame_id = 0;
  // Create a page node and a small hierarchy of frames.
  std::unique_ptr<ProcessNodeImpl> process_node =
      performance_manager()->CreateProcessNode(RenderProcessHostProxy());
  std::unique_ptr<PageNodeImpl> page_node =
      performance_manager()->CreatePageNode(WebContentsProxy(), std::string(),
                                            GURL(), false, false);

  std::unique_ptr<FrameNodeImpl> parent1_frame =
      performance_manager()->CreateFrameNode(
          process_node.get(), page_node.get(), nullptr, 0,
          ++next_render_frame_id, base::UnguessableToken::Create(), 0, 0);
  std::unique_ptr<FrameNodeImpl> parent2_frame =
      performance_manager()->CreateFrameNode(
          process_node.get(), page_node.get(), nullptr, 1,
          ++next_render_frame_id, base::UnguessableToken::Create(), 0, 0);

  std::unique_ptr<FrameNodeImpl> child1_frame =
      performance_manager()->CreateFrameNode(
          process_node.get(), page_node.get(), parent1_frame.get(), 2,
          ++next_render_frame_id, base::UnguessableToken::Create(), 0, 0);
  std::unique_ptr<FrameNodeImpl> child2_frame =
      performance_manager()->CreateFrameNode(
          process_node.get(), page_node.get(), parent2_frame.get(), 3,
          ++next_render_frame_id, base::UnguessableToken::Create(), 0, 0);

  std::vector<std::unique_ptr<NodeBase>> nodes;
  for (size_t i = 0; i < 10; ++i) {
    nodes.push_back(performance_manager()->CreateFrameNode(
        process_node.get(), page_node.get(), child1_frame.get(), 0,
        ++next_render_frame_id, base::UnguessableToken::Create(), 0, 0));
    nodes.push_back(performance_manager()->CreateFrameNode(
        process_node.get(), page_node.get(), child1_frame.get(), 1,
        ++next_render_frame_id, base::UnguessableToken::Create(), 0, 0));
  }

  nodes.push_back(std::move(process_node));
  nodes.push_back(std::move(page_node));
  nodes.push_back(std::move(parent1_frame));
  nodes.push_back(std::move(parent2_frame));
  nodes.push_back(std::move(child1_frame));
  nodes.push_back(std::move(child2_frame));

  performance_manager()->BatchDeleteNodes(std::move(nodes));
}

TEST_F(PerformanceManagerImplTest, CallOnGraphImpl) {
  // Create a page node for something to target.
  std::unique_ptr<PageNodeImpl> page_node =
      performance_manager()->CreatePageNode(WebContentsProxy(), std::string(),
                                            GURL(), false, false);
  base::RunLoop run_loop;
  base::OnceClosure quit_closure = run_loop.QuitClosure();
  EXPECT_FALSE(performance_manager()->OnPMTaskRunnerForTesting());
  PerformanceManagerImpl::GraphImplCallback graph_callback =
      base::BindLambdaForTesting([&](GraphImpl* graph) {
        EXPECT_TRUE(
            PerformanceManagerImpl::GetInstance()->OnPMTaskRunnerForTesting());
        EXPECT_EQ(page_node.get()->graph(), graph);
        std::move(quit_closure).Run();
      });

  performance_manager()->CallOnGraphImpl(FROM_HERE, std::move(graph_callback));
  run_loop.Run();

  performance_manager()->DeleteNode(std::move(page_node));
}

TEST_F(PerformanceManagerImplTest, CallOnGraphAndReplyWithResult) {
  // Create a page node for something to target.
  std::unique_ptr<PageNodeImpl> page_node =
      performance_manager()->CreatePageNode(WebContentsProxy(), std::string(),
                                            GURL(), false, false);
  base::RunLoop run_loop;

  EXPECT_FALSE(performance_manager()->OnPMTaskRunnerForTesting());
  base::OnceCallback<int(GraphImpl*)> task =
      base::BindLambdaForTesting([&](GraphImpl* graph) {
        EXPECT_TRUE(
            PerformanceManagerImpl::GetInstance()->OnPMTaskRunnerForTesting());
        EXPECT_EQ(page_node.get()->graph(), graph);
        return 1;
      });

  bool reply_called = false;
  base::OnceCallback<void(int)> reply = base::BindLambdaForTesting([&](int i) {
    EXPECT_EQ(i, 1);
    reply_called = true;
    std::move(run_loop.QuitClosure()).Run();
  });

  performance_manager()->CallOnGraphAndReplyWithResult(
      FROM_HERE, std::move(task), std::move(reply));
  run_loop.Run();

  performance_manager()->DeleteNode(std::move(page_node));

  EXPECT_TRUE(reply_called);
}

}  // namespace performance_manager
