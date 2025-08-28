// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/performance_manager_registry_impl.h"

#include "base/memory/ptr_util.h"
#include "base/test/gtest_util.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/performance_manager_observer.h"
#include "components/performance_manager/test_support/performance_manager_test_harness.h"
#include "components/performance_manager/test_support/test_browser_child_process.h"
#include "content/public/common/process_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

using PerformanceManagerRegistryImplTest = PerformanceManagerTestHarness;
using PerformanceManagerRegistryImplDeathTest = PerformanceManagerTestHarness;

class LenientMockObserver : public PerformanceManagerObserver {
 public:
  LenientMockObserver() = default;
  ~LenientMockObserver() override = default;

  MOCK_METHOD(void,
              OnPageNodeCreatedForWebContents,
              (content::WebContents*),
              (override));
  MOCK_METHOD(void, OnBeforePerformanceManagerDestroyed, (), (override));
};

using MockObserver = ::testing::StrictMock<LenientMockObserver>;

}  // namespace

TEST_F(PerformanceManagerRegistryImplTest, ObserverWorks) {
  MockObserver observer;
  PerformanceManagerRegistryImpl* registry =
      PerformanceManagerRegistryImpl::GetInstance();
  registry->AddObserver(&observer);

  std::unique_ptr<content::WebContents> contents =
      content::RenderViewHostTestHarness::CreateTestWebContents();
  EXPECT_CALL(observer, OnPageNodeCreatedForWebContents(contents.get()));
  registry->CreatePageNodeForWebContents(contents.get());
  testing::Mock::VerifyAndClear(&observer);

  // Expect a tear down notification, and use it to unregister ourselves.
  EXPECT_CALL(observer, OnBeforePerformanceManagerDestroyed())
      .WillOnce(
          [&registry, &observer]() { registry->RemoveObserver(&observer); });
  TearDownNow();
}

// Tests that accessors for browser and utility ProcessNodes work. Renderer
// ProcessNodes are handled by RenderProcessUserData.

TEST_F(PerformanceManagerRegistryImplDeathTest, BrowserProcessNode) {
  PerformanceManagerRegistryImpl* registry =
      PerformanceManagerRegistryImpl::GetInstance();
  ASSERT_TRUE(registry);

  const ProcessNodeImpl* browser_node = registry->GetBrowserProcessNode();
  ASSERT_TRUE(browser_node);
  EXPECT_EQ(browser_node->GetProcessType(), content::PROCESS_TYPE_BROWSER);

  DeleteBrowserProcessNodeForTesting();
  EXPECT_FALSE(registry->GetBrowserProcessNode());

  // Can't delete twice.
  EXPECT_CHECK_DEATH(DeleteBrowserProcessNodeForTesting());
}

TEST_F(PerformanceManagerRegistryImplDeathTest, BrowserChildProcessNodes) {
  PerformanceManagerRegistryImpl* registry =
      PerformanceManagerRegistryImpl::GetInstance();
  ASSERT_TRUE(registry);

  TestBrowserChildProcess utility_process(content::PROCESS_TYPE_UTILITY);
  TestBrowserChildProcess gpu_process(content::PROCESS_TYPE_GPU);
  EXPECT_FALSE(registry->GetBrowserChildProcessNode(utility_process.GetId()));
  EXPECT_FALSE(registry->GetBrowserChildProcessNode(gpu_process.GetId()));

  utility_process.SimulateLaunch();
  const ProcessNodeImpl* utility_node =
      registry->GetBrowserChildProcessNode(utility_process.GetId());
  ASSERT_TRUE(utility_node);
  EXPECT_FALSE(registry->GetBrowserChildProcessNode(gpu_process.GetId()));

  gpu_process.SimulateLaunch();
  const ProcessNodeImpl* gpu_node =
      registry->GetBrowserChildProcessNode(gpu_process.GetId());
  ASSERT_TRUE(gpu_node);
  EXPECT_NE(utility_node, gpu_node);

  EXPECT_EQ(utility_node->GetProcessType(), content::PROCESS_TYPE_UTILITY);
  EXPECT_EQ(gpu_node->GetProcessType(), content::PROCESS_TYPE_GPU);

  utility_process.SimulateDisconnect();
  utility_node = nullptr;  // No longer safe.
  EXPECT_FALSE(registry->GetBrowserChildProcessNode(utility_process.GetId()));
  EXPECT_EQ(registry->GetBrowserChildProcessNode(gpu_process.GetId()),
            gpu_node);

  // Can't delete twice.
  EXPECT_CHECK_DEATH(utility_process.SimulateDisconnect());

  // Should be able to re-create `utility_node` after it's deleted, but not
  // create two simultaneous copies.
  utility_process.SimulateLaunch();
  EXPECT_TRUE(registry->GetBrowserChildProcessNode(utility_process.GetId()));
  EXPECT_CHECK_DEATH(utility_process.SimulateLaunch());

  // `gpu_node` still exists. It should be safely deleted during teardown.
}

}  // namespace performance_manager
