// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/components/component_manager.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/chrome_cleaner/components/component_api.h"
#include "chrome/chrome_cleaner/test/test_component.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

namespace {

const UwSId kFakePupId = 42;

// This delegate validates that calls were made, and also interrupt the
// currently running message loop, waiting for the delegate methods to be
// called.
class TestComponentManagerDelegate : public ComponentManagerDelegate {
 public:
  struct Calls {
    bool pre_scan = false;
    bool post_scan = false;
    bool pre_cleanup = false;
    bool post_cleanup = false;
  };
  explicit TestComponentManagerDelegate(Calls* calls) : calls_(calls) {}

  void set_quit_closure(base::OnceClosure quit_closure) {
    quit_closure_ = std::move(quit_closure);
  }

  // ComponentManagerDelegate
  void PreScanDone() override {
    calls_->pre_scan = true;
    if (quit_closure_)
      base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                    std::move(quit_closure_));
  }
  void PostScanDone() override {
    calls_->post_scan = true;
    if (quit_closure_)
      base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                    std::move(quit_closure_));
  }
  void PreCleanupDone() override {
    calls_->pre_cleanup = true;
    if (quit_closure_)
      base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                    std::move(quit_closure_));
  }
  void PostCleanupDone() override {
    calls_->post_cleanup = true;
    if (quit_closure_)
      base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                    std::move(quit_closure_));
  }

 private:
  Calls* calls_;
  base::OnceClosure quit_closure_;
};

}  // namespace

TEST(ComponentManagerTest, Empty) {
  base::test::TaskEnvironment task_environment;

  TestComponentManagerDelegate::Calls calls;
  TestComponentManagerDelegate delegate(&calls);
  ComponentManager component_manager(&delegate);

  base::RunLoop run_loop1;
  delegate.set_quit_closure(run_loop1.QuitWhenIdleClosure());
  component_manager.PreScan();
  run_loop1.Run();
  EXPECT_TRUE(calls.pre_scan);
  calls.pre_scan = false;
  EXPECT_FALSE(calls.post_scan);
  EXPECT_FALSE(calls.pre_cleanup);
  EXPECT_FALSE(calls.post_cleanup);

  base::RunLoop run_loop2;
  delegate.set_quit_closure(run_loop2.QuitWhenIdleClosure());
  component_manager.PostScan(std::vector<UwSId>());
  run_loop2.Run();
  EXPECT_FALSE(calls.pre_scan);
  EXPECT_TRUE(calls.post_scan);
  calls.post_scan = false;
  EXPECT_FALSE(calls.pre_cleanup);
  EXPECT_FALSE(calls.post_cleanup);

  base::RunLoop run_loop3;
  delegate.set_quit_closure(run_loop3.QuitWhenIdleClosure());
  component_manager.PreCleanup();
  run_loop3.Run();
  EXPECT_FALSE(calls.pre_scan);
  EXPECT_FALSE(calls.post_scan);
  EXPECT_TRUE(calls.pre_cleanup);
  calls.pre_cleanup = false;
  EXPECT_FALSE(calls.post_cleanup);

  base::RunLoop run_loop4;
  delegate.set_quit_closure(run_loop4.QuitWhenIdleClosure());
  component_manager.PostCleanup(RESULT_CODE_SUCCESS, nullptr);
  run_loop4.Run();
  EXPECT_FALSE(calls.pre_scan);
  EXPECT_FALSE(calls.post_scan);
  EXPECT_FALSE(calls.pre_cleanup);
  EXPECT_TRUE(calls.post_cleanup);

  // Nothing to validate, just make sure it can be called.
  component_manager.CloseAllComponents(RESULT_CODE_FAILED);
}

TEST(ComponentManagerTest, All) {
  base::test::TaskEnvironment task_environment;

  TestComponentManagerDelegate::Calls delegate_calls;
  TestComponentManagerDelegate delegate(&delegate_calls);
  ComponentManager component_manager(&delegate);

  TestComponent::Calls component_calls;
  component_manager.AddComponent(
      std::make_unique<TestComponent>(&component_calls));

  base::RunLoop run_loop1;
  delegate.set_quit_closure(run_loop1.QuitWhenIdleClosure());
  component_manager.PreScan();
  run_loop1.Run();
  EXPECT_TRUE(delegate_calls.pre_scan);
  EXPECT_TRUE(component_calls.pre_scan);

  base::RunLoop run_loop2;
  delegate.set_quit_closure(run_loop2.QuitWhenIdleClosure());
  std::vector<UwSId> pup_ids;
  pup_ids.push_back(kFakePupId);
  component_manager.PostScan(pup_ids);
  run_loop2.Run();
  EXPECT_TRUE(delegate_calls.post_scan);
  EXPECT_TRUE(component_calls.post_scan);
  ASSERT_EQ(1UL, component_calls.post_scan_found_pups.size());
  EXPECT_EQ(kFakePupId, component_calls.post_scan_found_pups[0]);

  base::RunLoop run_loop3;
  delegate.set_quit_closure(run_loop3.QuitWhenIdleClosure());
  component_manager.PreCleanup();
  run_loop3.Run();
  EXPECT_TRUE(delegate_calls.pre_cleanup);
  EXPECT_TRUE(component_calls.pre_cleanup);

  base::RunLoop run_loop4;
  delegate.set_quit_closure(run_loop4.QuitWhenIdleClosure());
  component_manager.PostCleanup(RESULT_CODE_SUCCESS, nullptr);
  run_loop4.Run();
  EXPECT_TRUE(delegate_calls.post_cleanup);
  EXPECT_TRUE(component_calls.post_cleanup);
  EXPECT_EQ(RESULT_CODE_SUCCESS, component_calls.result_code);
  component_calls.result_code = RESULT_CODE_INVALID;

  component_manager.PostValidation(RESULT_CODE_FAILED);
  EXPECT_TRUE(component_calls.post_validation);
  EXPECT_EQ(RESULT_CODE_FAILED, component_calls.result_code);

  component_manager.CloseAllComponents(RESULT_CODE_CANCELED);
  EXPECT_TRUE(component_calls.on_close);
  EXPECT_TRUE(component_calls.destroyed);
  EXPECT_EQ(RESULT_CODE_CANCELED, component_calls.result_code);
}

TEST(ComponentManagerTest, Interrupt) {
  base::test::TaskEnvironment task_environment;

  TestComponentManagerDelegate::Calls delegate_calls;
  TestComponentManagerDelegate delegate(&delegate_calls);
  ComponentManager component_manager(&delegate);

  TestComponent::Calls component_calls;
  for (size_t i = 0; i < 1000; ++i) {
    component_manager.AddComponent(
        std::make_unique<TestComponent>(&component_calls));
  }

  component_manager.PreScan();
  EXPECT_FALSE(delegate_calls.pre_scan);
  component_manager.CloseAllComponents(RESULT_CODE_INVALID);
  EXPECT_FALSE(delegate_calls.pre_scan);
  // We rely on test harness leak detector to make sure TestComponents were
  // properly destroyed.
  EXPECT_GT(component_manager.num_tasks_pending(), 0UL);
}

}  // namespace chrome_cleaner
