// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/performance_manager_impl.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/render_process_host_id.h"
#include "components/performance_manager/public/render_process_host_proxy.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/process_type.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace performance_manager {

class PerformanceManagerImplTest : public testing::Test {
 public:
  PerformanceManagerImplTest() = default;

  PerformanceManagerImplTest(const PerformanceManagerImplTest&) = delete;
  PerformanceManagerImplTest& operator=(const PerformanceManagerImplTest&) =
      delete;

  ~PerformanceManagerImplTest() override = default;

  void SetUp() override {
    EXPECT_FALSE(PerformanceManagerImpl::IsAvailable());
    performance_manager_ = PerformanceManagerImpl::Create(base::DoNothing());
    // Make sure creation registers the created instance.
    EXPECT_TRUE(PerformanceManagerImpl::IsAvailable());
  }

  void TearDown() override {
    PerformanceManagerImpl::Destroy(std::move(performance_manager_));
    // Make sure destruction unregisters the instance.
    EXPECT_FALSE(PerformanceManagerImpl::IsAvailable());

    task_environment_.RunUntilIdle();
  }

 private:
  std::unique_ptr<PerformanceManagerImpl> performance_manager_;
  content::BrowserTaskEnvironment task_environment_;
};

using PerformanceManagerImplDeathTest = PerformanceManagerImplTest;

TEST_F(PerformanceManagerImplTest, InstantiateNodes) {
  const auto render_process_host_id = RenderProcessHostId(1);
  int next_render_frame_id = 0;

  std::unique_ptr<ProcessNodeImpl> process_node =
      PerformanceManagerImpl::CreateProcessNode(
          RenderProcessHostProxy::CreateForTesting(render_process_host_id),
          base::TaskPriority::HIGHEST);
  EXPECT_NE(nullptr, process_node.get());
  std::unique_ptr<PageNodeImpl> page_node =
      PerformanceManagerImpl::CreatePageNode(nullptr, std::string(), GURL(),
                                             PagePropertyFlags{},
                                             base::TimeTicks::Now());
  EXPECT_NE(nullptr, page_node.get());

  // Create a node of each type.
  std::unique_ptr<FrameNodeImpl> frame_node =
      PerformanceManagerImpl::CreateFrameNode(
          process_node.get(), page_node.get(), /*parent_frame_node=*/nullptr,
          /*outer_document_for_fenced_frame*/ nullptr, ++next_render_frame_id,
          blink::LocalFrameToken(), content::BrowsingInstanceId(0),
          content::SiteInstanceGroupId(0), /*is_current=*/true);
  EXPECT_NE(nullptr, frame_node.get());

  PerformanceManagerImpl::DeleteNode(std::move(frame_node));
  PerformanceManagerImpl::DeleteNode(std::move(page_node));
  PerformanceManagerImpl::DeleteNode(std::move(process_node));
}

TEST_F(PerformanceManagerImplDeathTest, InvalidProcessHostProxies) {
  const auto browser_child_process_host_id = BrowserChildProcessHostId(1);
  EXPECT_CHECK_DEATH(PerformanceManagerImpl::CreateProcessNode(
      RenderProcessHostProxy(), base::TaskPriority::HIGHEST));
  EXPECT_CHECK_DEATH(PerformanceManagerImpl::CreateProcessNode(
      content::PROCESS_TYPE_UTILITY, BrowserChildProcessHostProxy()));

  // Valid proxy, wrong process type.
  EXPECT_CHECK_DEATH(PerformanceManagerImpl::CreateProcessNode(
      content::PROCESS_TYPE_BROWSER,
      BrowserChildProcessHostProxy::CreateForTesting(
          browser_child_process_host_id)));
  EXPECT_CHECK_DEATH(PerformanceManagerImpl::CreateProcessNode(
      content::PROCESS_TYPE_RENDERER,
      BrowserChildProcessHostProxy::CreateForTesting(
          browser_child_process_host_id)));
}

TEST_F(PerformanceManagerImplTest, BatchDeleteNodes) {
  const auto render_process_host_id = RenderProcessHostId(1);
  int next_render_frame_id = 0;
  // Create a page node and a small hierarchy of frames.
  std::unique_ptr<ProcessNodeImpl> process_node =
      PerformanceManagerImpl::CreateProcessNode(
          RenderProcessHostProxy::CreateForTesting(render_process_host_id),
          base::TaskPriority::HIGHEST);
  std::unique_ptr<PageNodeImpl> page_node =
      PerformanceManagerImpl::CreatePageNode(nullptr, std::string(), GURL(),
                                             PagePropertyFlags{},
                                             base::TimeTicks::Now());

  std::unique_ptr<FrameNodeImpl> parent1_frame =
      PerformanceManagerImpl::CreateFrameNode(
          process_node.get(), page_node.get(), /*parent_frame_node=*/nullptr,
          /*outer_document_for_fenced_frame*/ nullptr, ++next_render_frame_id,
          blink::LocalFrameToken(), content::BrowsingInstanceId(0),
          content::SiteInstanceGroupId(0), /*is_current*/ true);
  std::unique_ptr<FrameNodeImpl> parent2_frame =
      PerformanceManagerImpl::CreateFrameNode(
          process_node.get(), page_node.get(), /*parent_frame_node=*/nullptr,
          /*outer_document_for_fenced_frame*/ nullptr, ++next_render_frame_id,
          blink::LocalFrameToken(), content::BrowsingInstanceId(0),
          content::SiteInstanceGroupId(0), /*is_current*/ true);

  std::unique_ptr<FrameNodeImpl> child1_frame =
      PerformanceManagerImpl::CreateFrameNode(
          process_node.get(), page_node.get(), parent1_frame.get(),
          /*outer_document_for_fenced_frame*/ nullptr, ++next_render_frame_id,
          blink::LocalFrameToken(), content::BrowsingInstanceId(0),
          content::SiteInstanceGroupId(0), /*is_current*/ true);
  std::unique_ptr<FrameNodeImpl> child2_frame =
      PerformanceManagerImpl::CreateFrameNode(
          process_node.get(), page_node.get(), parent2_frame.get(),
          /*outer_document_for_fenced_frame*/ nullptr, ++next_render_frame_id,
          blink::LocalFrameToken(), content::BrowsingInstanceId(0),
          content::SiteInstanceGroupId(0), /*is_current*/ true);

  std::vector<std::unique_ptr<NodeBase>> nodes;
  for (size_t i = 0; i < 10; ++i) {
    nodes.push_back(PerformanceManagerImpl::CreateFrameNode(
        process_node.get(), page_node.get(), child1_frame.get(),
        /*outer_document_for_fenced_frame*/ nullptr, ++next_render_frame_id,
        blink::LocalFrameToken(), content::BrowsingInstanceId(0),
        content::SiteInstanceGroupId(0), /*is_current*/ true));
    nodes.push_back(PerformanceManagerImpl::CreateFrameNode(
        process_node.get(), page_node.get(), child1_frame.get(),
        /*outer_document_for_fenced_frame*/ nullptr, ++next_render_frame_id,
        blink::LocalFrameToken(), content::BrowsingInstanceId(0),
        content::SiteInstanceGroupId(0), /*is_current*/ true));
  }

  nodes.push_back(std::move(process_node));
  nodes.push_back(std::move(page_node));
  nodes.push_back(std::move(parent1_frame));
  nodes.push_back(std::move(parent2_frame));
  nodes.push_back(std::move(child1_frame));
  nodes.push_back(std::move(child2_frame));

  PerformanceManagerImpl::BatchDeleteNodes(std::move(nodes));
}

TEST_F(PerformanceManagerImplTest, CallOnGraphImpl) {
  // Create a page node for something to target.
  std::unique_ptr<PageNodeImpl> page_node =
      PerformanceManagerImpl::CreatePageNode(nullptr, std::string(), GURL(),
                                             PagePropertyFlags{},
                                             base::TimeTicks::Now());
  base::RunLoop run_loop;
  base::OnceClosure quit_closure = run_loop.QuitClosure();
  EXPECT_TRUE(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  PerformanceManagerImpl::GraphImplCallback graph_callback =
      base::BindLambdaForTesting([&](GraphImpl* graph) {
        EXPECT_TRUE(PerformanceManagerImpl::OnPMTaskRunnerForTesting());
        EXPECT_EQ(page_node.get()->graph(), graph);
        std::move(quit_closure).Run();
      });

  PerformanceManagerImpl::CallOnGraphImpl(FROM_HERE, std::move(graph_callback));
  run_loop.Run();

  PerformanceManagerImpl::DeleteNode(std::move(page_node));
}

TEST_F(PerformanceManagerImplTest, CallOnGraphAndReplyWithResult) {
  // Create a page node for something to target.
  std::unique_ptr<PageNodeImpl> page_node =
      PerformanceManagerImpl::CreatePageNode(nullptr, std::string(), GURL(),
                                             PagePropertyFlags{},
                                             base::TimeTicks::Now());
  base::RunLoop run_loop;

  EXPECT_TRUE(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  base::OnceCallback<int(GraphImpl*)> task =
      base::BindLambdaForTesting([&](GraphImpl* graph) {
        EXPECT_TRUE(PerformanceManagerImpl::OnPMTaskRunnerForTesting());
        EXPECT_EQ(page_node.get()->graph(), graph);
        return 1;
      });

  bool reply_called = false;
  base::OnceCallback<void(int)> reply = base::BindLambdaForTesting([&](int i) {
    EXPECT_EQ(i, 1);
    reply_called = true;
    std::move(run_loop.QuitClosure()).Run();
  });

  PerformanceManagerImpl::CallOnGraphAndReplyWithResult(
      FROM_HERE, std::move(task), std::move(reply));
  run_loop.Run();

  PerformanceManagerImpl::DeleteNode(std::move(page_node));

  EXPECT_TRUE(reply_called);
}

}  // namespace performance_manager
