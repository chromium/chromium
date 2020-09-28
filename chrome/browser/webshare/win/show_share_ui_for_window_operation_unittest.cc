// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/win/show_share_ui_for_window_operation.h"

#include <shlobj.h>
#include <windows.applicationmodel.datatransfer.h>
#include <wrl/implements.h>
#include <wrl/module.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/win/com_init_util.h"
#include "base/win/core_winrt_util.h"
#include "chrome/browser/webshare/win/fake_data_transfer_manager_interop.h"
#include "chrome/browser/webshare/win/scoped_fake_data_transfer_manager_interop.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using ABI::Windows::ApplicationModel::DataTransfer::IDataRequest;
using ABI::Windows::ApplicationModel::DataTransfer::IDataRequestedEventArgs;
using ABI::Windows::ApplicationModel::DataTransfer::IDataTransferManager;
using Microsoft::WRL::ActivationFactory;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::RuntimeClass;
using Microsoft::WRL::RuntimeClassFlags;

namespace webshare {

using DataRequestedCallback =
    ShowShareUIForWindowOperation::DataRequestedCallback;
using ShowShareUIForWindowBehavior =
    FakeDataTransferManagerInterop::ShowShareUIForWindowBehavior;

class ShowShareUIForWindowOperationTest : public ::testing::Test {
 protected:
  enum TestCallbackState { NotRun = 0, RunWithoutValue, RunWithValue };

  bool IsSupportedEnvironment() {
    return base::win::ResolveCoreWinRTDelayload() &&
           base::win::ScopedHString::ResolveCoreWinRTStringDelayload();
  }

  void SetUp() override {
    if (!IsSupportedEnvironment())
      return;
    scoped_interop_ = std::make_unique<ScopedFakeDataTransferManagerInterop>();
    auto weak_ptr = weak_factory_.GetWeakPtr();
    test_callback_ = base::BindOnce(
        [](base::WeakPtr<ShowShareUIForWindowOperationTest> weak_ptr,
           IDataRequestedEventArgs* event_args) {
          if (weak_ptr) {
            EXPECT_EQ(weak_ptr->test_callback_state_,
                      TestCallbackState::NotRun);
            weak_ptr->test_callback_state_ =
                event_args ? TestCallbackState::RunWithValue
                           : TestCallbackState::RunWithoutValue;
          }
        },
        weak_ptr);
  }

  void TearDown() override {
    if (!IsSupportedEnvironment())
      return;
    base::win::RoUninitialize();
    ASSERT_FALSE(fake_interop().HasDataRequestedListener(hwnd_));
  }

  FakeDataTransferManagerInterop& fake_interop() {
    return scoped_interop_->instance();
  }

  const HWND hwnd_ = reinterpret_cast<HWND>(1);
  std::unique_ptr<ScopedFakeDataTransferManagerInterop> scoped_interop_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  DataRequestedCallback test_callback_;
  TestCallbackState test_callback_state_ = TestCallbackState::NotRun;
  base::WeakPtrFactory<ShowShareUIForWindowOperationTest> weak_factory_{this};
};

TEST_F(ShowShareUIForWindowOperationTest, AsyncSuccess) {
  if (!IsSupportedEnvironment())
    return;

  fake_interop().SetShowShareUIForWindowBehavior(
      ShowShareUIForWindowBehavior::SucceedWithoutAction);

  ShowShareUIForWindowOperation operation{hwnd_};
  operation.Run(std::move(test_callback_));
  ASSERT_EQ(test_callback_state_, TestCallbackState::NotRun);
  auto data_requested_invoker = fake_interop().GetDataRequestedInvoker(hwnd_);

  std::move(data_requested_invoker).Run();
  ASSERT_EQ(test_callback_state_, TestCallbackState::RunWithValue);
}

TEST_F(ShowShareUIForWindowOperationTest, AsyncFailure) {
  if (!IsSupportedEnvironment())
    return;

  fake_interop().SetShowShareUIForWindowBehavior(
      ShowShareUIForWindowBehavior::SucceedWithoutAction);

  ShowShareUIForWindowOperation operation{hwnd_};
  operation.Run(std::move(test_callback_));
  ASSERT_EQ(test_callback_state_, TestCallbackState::NotRun);

  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  ASSERT_EQ(test_callback_state_, TestCallbackState::NotRun);

  task_environment_.FastForwardBy(
      ShowShareUIForWindowOperation::max_execution_time_for_testing());
  ASSERT_EQ(test_callback_state_, TestCallbackState::RunWithoutValue);
}

TEST_F(ShowShareUIForWindowOperationTest, AsyncEarlyDestruction) {
  if (!IsSupportedEnvironment())
    return;

  fake_interop().SetShowShareUIForWindowBehavior(
      ShowShareUIForWindowBehavior::SucceedWithoutAction);

  auto operation = std::make_unique<ShowShareUIForWindowOperation>(hwnd_);
  operation->Run(std::move(test_callback_));
  ASSERT_EQ(test_callback_state_, TestCallbackState::NotRun);

  auto data_requested_invoker = fake_interop().GetDataRequestedInvoker(hwnd_);
  ASSERT_NO_FATAL_FAILURE(operation.reset());
  ASSERT_EQ(test_callback_state_, TestCallbackState::RunWithoutValue);
  ASSERT_NO_FATAL_FAILURE(std::move(data_requested_invoker).Run());
}

TEST_F(ShowShareUIForWindowOperationTest, SyncSuccess) {
  if (!IsSupportedEnvironment())
    return;

  fake_interop().SetShowShareUIForWindowBehavior(
      ShowShareUIForWindowBehavior::InvokeEventSynchronously);

  ShowShareUIForWindowOperation operation{hwnd_};
  operation.Run(std::move(test_callback_));
  ASSERT_EQ(test_callback_state_, TestCallbackState::RunWithValue);
}

TEST_F(ShowShareUIForWindowOperationTest, SyncEarlyFailure) {
  if (!IsSupportedEnvironment())
    return;

  fake_interop().SetShowShareUIForWindowBehavior(
      ShowShareUIForWindowBehavior::FailImmediately);

  ShowShareUIForWindowOperation operation{hwnd_};
  operation.Run(std::move(test_callback_));
  ASSERT_EQ(test_callback_state_, TestCallbackState::NotRun);
}

TEST_F(ShowShareUIForWindowOperationTest, SyncLateFailure) {
  if (!IsSupportedEnvironment())
    return;

  fake_interop().SetShowShareUIForWindowBehavior(
      ShowShareUIForWindowBehavior::InvokeEventSynchronouslyAndReturnFailure);

  ShowShareUIForWindowOperation operation{hwnd_};
  operation.Run(std::move(test_callback_));
  ASSERT_EQ(test_callback_state_, TestCallbackState::RunWithValue);
}

TEST_F(ShowShareUIForWindowOperationTest, DestructionWithoutRun) {
  if (!IsSupportedEnvironment())
    return;

  auto operation = std::make_unique<ShowShareUIForWindowOperation>(hwnd_);
  ASSERT_NO_FATAL_FAILURE(operation.reset());
}

}  // namespace webshare
