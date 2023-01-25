// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/win/fake_data_transfer_manager_interop.h"

#include <windows.applicationmodel.datatransfer.h>
#include <wrl/event.h>
#include <wrl/implements.h>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/win/core_winrt_util.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"

using ABI::Windows::ApplicationModel::DataTransfer::DataRequestedEventArgs;
using ABI::Windows::ApplicationModel::DataTransfer::DataTransferManager;
using ABI::Windows::ApplicationModel::DataTransfer::IDataRequestedEventArgs;
using ABI::Windows::ApplicationModel::DataTransfer::IDataTransferManager;
using ABI::Windows::Foundation::ITypedEventHandler;
using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

namespace webshare {

using ShowShareUIForWindowBehavior =
    FakeDataTransferManagerInterop::ShowShareUIForWindowBehavior;

// Provides a DataRequested callback and records the number of times it is
// invoked
class DataRequestedTestCallback {
 public:
  DataRequestedTestCallback() {
    auto weak_ptr = weak_factory_.GetWeakPtr();
    callback_ = Callback<
        ITypedEventHandler<DataTransferManager*, DataRequestedEventArgs*>>(
        [weak_ptr](IDataTransferManager* data_transfer_manager,
                   IDataRequestedEventArgs* event_args) -> HRESULT {
          if (weak_ptr.get())
            weak_ptr->invocation_count_++;
          return S_OK;
        });
  }
  int invocation_count_ = 0;
  ComPtr<ITypedEventHandler<DataTransferManager*, DataRequestedEventArgs*>>
      callback_;

 private:
  base::WeakPtrFactory<DataRequestedTestCallback> weak_factory_{this};
};

class FakeDataTransferManagerInteropTest : public ::testing::Test {
 protected:
  void SetUp() override {
    fake_data_transfer_manager_interop_ =
        Microsoft::WRL::Make<FakeDataTransferManagerInterop>();
  }

  ComPtr<FakeDataTransferManagerInterop> fake_data_transfer_manager_interop_;
  const HWND hwnd_1_ = reinterpret_cast<HWND>(1);
  const HWND hwnd_2_ = reinterpret_cast<HWND>(2);
  content::BrowserTaskEnvironment task_environment;
};

TEST_F(FakeDataTransferManagerInteropTest, GetDataRequestedInvoker) {
  // Verify failure when called without a listener in place
  base::OnceClosure invoker;
  EXPECT_NONFATAL_FAILURE(
      invoker =
          fake_data_transfer_manager_interop_->GetDataRequestedInvoker(hwnd_1_),
      "GetDataRequestedInvoker");

  // Set up a listener for |hwnd_1_|
  ComPtr<IDataTransferManager> data_transfer_manager;
  ASSERT_HRESULT_SUCCEEDED(fake_data_transfer_manager_interop_->GetForWindow(
      hwnd_1_, IID_PPV_ARGS(&data_transfer_manager)));
  EventRegistrationToken token;
  DataRequestedTestCallback test_callback;
  ASSERT_HRESULT_SUCCEEDED(data_transfer_manager->add_DataRequested(
      test_callback.callback_.Get(), &token));

  // Verify failure when called with a different HWND
  EXPECT_NONFATAL_FAILURE(
      invoker =
          fake_data_transfer_manager_interop_->GetDataRequestedInvoker(hwnd_2_),
      "GetDataRequestedInvoker");

  // Verify success when called with a matching HWND
  EXPECT_NO_FATAL_FAILURE(
      invoker = fake_data_transfer_manager_interop_->GetDataRequestedInvoker(
          hwnd_1_));

  ASSERT_HRESULT_SUCCEEDED(data_transfer_manager->remove_DataRequested(token));
}

TEST_F(FakeDataTransferManagerInteropTest, HasDataRequestedListener) {
  // Verify values before any listeners are attached
  ASSERT_FALSE(
      fake_data_transfer_manager_interop_->HasDataRequestedListener(hwnd_1_));
  ASSERT_FALSE(
      fake_data_transfer_manager_interop_->HasDataRequestedListener(hwnd_2_));

  // Add a listener for |hwnd_1_| and verify values
  ComPtr<IDataTransferManager> data_transfer_manager_1;
  ASSERT_HRESULT_SUCCEEDED(fake_data_transfer_manager_interop_->GetForWindow(
      hwnd_1_, IID_PPV_ARGS(&data_transfer_manager_1)));
  EventRegistrationToken token_1;
  DataRequestedTestCallback test_callback;
  ASSERT_HRESULT_SUCCEEDED(data_transfer_manager_1->add_DataRequested(
      test_callback.callback_.Get(), &token_1));
  ASSERT_TRUE(
      fake_data_transfer_manager_interop_->HasDataRequestedListener(hwnd_1_));
  ASSERT_FALSE(
      fake_data_transfer_manager_interop_->HasDataRequestedListener(hwnd_2_));

  // Add a listener for |hwnd_2_| and verify values
  ComPtr<IDataTransferManager> data_transfer_manager_2;
  ASSERT_HRESULT_SUCCEEDED(fake_data_transfer_manager_interop_->GetForWindow(
      hwnd_2_, IID_PPV_ARGS(&data_transfer_manager_2)));
  EventRegistrationToken token_2;
  ASSERT_HRESULT_SUCCEEDED(data_transfer_manager_2->add_DataRequested(
      test_callback.callback_.Get(), &token_2));
  ASSERT_TRUE(
      fake_data_transfer_manager_interop_->HasDataRequestedListener(hwnd_1_));
  ASSERT_TRUE(
      fake_data_transfer_manager_interop_->HasDataRequestedListener(hwnd_2_));

  // Add an additional listener for |hwnd_2_| and verify values
  EventRegistrationToken token_3;
  ASSERT_HRESULT_SUCCEEDED(data_transfer_manager_2->add_DataRequested(
      test_callback.callback_.Get(), &token_3));
  ASSERT_TRUE(
      fake_data_transfer_manager_interop_->HasDataRequestedListener(hwnd_1_));
  ASSERT_TRUE(
      fake_data_transfer_manager_interop_->HasDataRequestedListener(hwnd_2_));

  // Remove the original listener for |hwnd_1_| and verify values
  ASSERT_HRESULT_SUCCEEDED(
      data_transfer_manager_1->remove_DataRequested(token_1));
  ASSERT_FALSE(
      fake_data_transfer_manager_interop_->HasDataRequestedListener(hwnd_1_));
  ASSERT_TRUE(
      fake_data_transfer_manager_interop_->HasDataRequestedListener(hwnd_2_));

  // Remove the original listener for |hwnd_2_| and verify values
  ASSERT_HRESULT_SUCCEEDED(
      data_transfer_manager_2->remove_DataRequested(token_2));
  ASSERT_FALSE(
      fake_data_transfer_manager_interop_->HasDataRequestedListener(hwnd_1_));
  ASSERT_TRUE(
      fake_data_transfer_manager_interop_->HasDataRequestedListener(hwnd_2_));

  // Remove the second listener for |hwnd_2_| and verify values
  ASSERT_HRESULT_SUCCEEDED(
      data_transfer_manager_2->remove_DataRequested(token_3));
  ASSERT_FALSE(
      fake_data_transfer_manager_interop_->HasDataRequestedListener(hwnd_1_));
  ASSERT_FALSE(
      fake_data_transfer_manager_interop_->HasDataRequestedListener(hwnd_2_));
}

TEST_F(FakeDataTransferManagerInteropTest,
       ShowShareUIForWindow_FailImmediately) {
  // Set up a listener for |hwnd_1_|
  ComPtr<IDataTransferManager> data_transfer_manager;
  ASSERT_HRESULT_SUCCEEDED(fake_data_transfer_manager_interop_->GetForWindow(
      hwnd_1_, IID_PPV_ARGS(&data_transfer_manager)));
  EventRegistrationToken token;
  DataRequestedTestCallback test_callback;
  ASSERT_HRESULT_SUCCEEDED(data_transfer_manager->add_DataRequested(
      test_callback.callback_.Get(), &token));

  // Validate that ShowShareUIForWindow fails without invoking the DataRequested
  // event
  ASSERT_NO_FATAL_FAILURE(
      fake_data_transfer_manager_interop_->SetShowShareUIForWindowBehavior(
          ShowShareUIForWindowBehavior::FailImmediately));
  base::RunLoop run_loop;
  ASSERT_HRESULT_FAILED(
      fake_data_transfer_manager_interop_->ShowShareUIForWindow(hwnd_1_));
  run_loop.RunUntilIdle();
  ASSERT_EQ(test_callback.invocation_count_, 0);

  ASSERT_HRESULT_SUCCEEDED(data_transfer_manager->remove_DataRequested(token));
}

TEST_F(FakeDataTransferManagerInteropTest,
       ShowShareUIForWindow_InvokeEventSynchronously) {
  // Set up a listener for |hwnd_1_|
  ComPtr<IDataTransferManager> data_transfer_manager;
  ASSERT_HRESULT_SUCCEEDED(fake_data_transfer_manager_interop_->GetForWindow(
      hwnd_1_, IID_PPV_ARGS(&data_transfer_manager)));
  EventRegistrationToken token;
  DataRequestedTestCallback test_callback;
  ASSERT_HRESULT_SUCCEEDED(data_transfer_manager->add_DataRequested(
      test_callback.callback_.Get(), &token));

  // Validate that ShowShareUIForWindow succeeds and invokes the DataRequested
  // event in a synchronous fashion
  ASSERT_NO_FATAL_FAILURE(
      fake_data_transfer_manager_interop_->SetShowShareUIForWindowBehavior(
          ShowShareUIForWindowBehavior::InvokeEventSynchronously));
  ASSERT_HRESULT_SUCCEEDED(
      fake_data_transfer_manager_interop_->ShowShareUIForWindow(hwnd_1_));
  ASSERT_EQ(test_callback.invocation_count_, 1);

  ASSERT_HRESULT_SUCCEEDED(data_transfer_manager->remove_DataRequested(token));
}

TEST_F(FakeDataTransferManagerInteropTest,
       ShowShareUIForWindow_InvokeEventSynchronouslyAndReturnFailure) {
  // Set up a listener for |hwnd_1_|
  ComPtr<IDataTransferManager> data_transfer_manager;
  ASSERT_HRESULT_SUCCEEDED(fake_data_transfer_manager_interop_->GetForWindow(
      hwnd_1_, IID_PPV_ARGS(&data_transfer_manager)));
  EventRegistrationToken token;
  DataRequestedTestCallback test_callback;
  ASSERT_HRESULT_SUCCEEDED(data_transfer_manager->add_DataRequested(
      test_callback.callback_.Get(), &token));

  // Validate that ShowShareUIForWindow invokes the DataRequested
  // event in a synchronous fashion, but still fails
  ASSERT_NO_FATAL_FAILURE(
      fake_data_transfer_manager_interop_->SetShowShareUIForWindowBehavior(
          ShowShareUIForWindowBehavior::
              InvokeEventSynchronouslyAndReturnFailure));
  ASSERT_HRESULT_FAILED(
      fake_data_transfer_manager_interop_->ShowShareUIForWindow(hwnd_1_));
  ASSERT_EQ(test_callback.invocation_count_, 1);

  ASSERT_HRESULT_SUCCEEDED(data_transfer_manager->remove_DataRequested(token));
}

TEST_F(FakeDataTransferManagerInteropTest, ShowShareUIForWindow_ScheduleEvent) {
  // Set up a listener for |hwnd_1_|
  ComPtr<IDataTransferManager> data_transfer_manager;
  ASSERT_HRESULT_SUCCEEDED(fake_data_transfer_manager_interop_->GetForWindow(
      hwnd_1_, IID_PPV_ARGS(&data_transfer_manager)));
  EventRegistrationToken token;
  DataRequestedTestCallback test_callback;
  ASSERT_HRESULT_SUCCEEDED(data_transfer_manager->add_DataRequested(
      test_callback.callback_.Get(), &token));

  // Validate that ShowShareUIForWindow succeeds and invokes the DataRequested
  // event in an async fashion
  ASSERT_NO_FATAL_FAILURE(
      fake_data_transfer_manager_interop_->SetShowShareUIForWindowBehavior(
          ShowShareUIForWindowBehavior::ScheduleEvent));
  base::RunLoop run_loop;
  ASSERT_HRESULT_SUCCEEDED(
      fake_data_transfer_manager_interop_->ShowShareUIForWindow(hwnd_1_));
  ASSERT_EQ(test_callback.invocation_count_, 0);
  run_loop.RunUntilIdle();
  ASSERT_EQ(test_callback.invocation_count_, 1);

  ASSERT_HRESULT_SUCCEEDED(data_transfer_manager->remove_DataRequested(token));
}

TEST_F(FakeDataTransferManagerInteropTest,
       ShowShareUIForWindow_SucceedWithoutAction) {
  // Set up a listener for |hwnd_1_|
  ComPtr<IDataTransferManager> data_transfer_manager;
  ASSERT_HRESULT_SUCCEEDED(fake_data_transfer_manager_interop_->GetForWindow(
      hwnd_1_, IID_PPV_ARGS(&data_transfer_manager)));
  EventRegistrationToken token;
  DataRequestedTestCallback test_callback;
  ASSERT_HRESULT_SUCCEEDED(data_transfer_manager->add_DataRequested(
      test_callback.callback_.Get(), &token));

  // Validate that ShowShareUIForWindow succeeds, but does not invoke the
  // DataRequested event
  ASSERT_NO_FATAL_FAILURE(
      fake_data_transfer_manager_interop_->SetShowShareUIForWindowBehavior(
          ShowShareUIForWindowBehavior::SucceedWithoutAction));
  base::RunLoop run_loop;
  ASSERT_HRESULT_SUCCEEDED(
      fake_data_transfer_manager_interop_->ShowShareUIForWindow(hwnd_1_));
  run_loop.RunUntilIdle();
  ASSERT_EQ(test_callback.invocation_count_, 0);

  ASSERT_HRESULT_SUCCEEDED(data_transfer_manager->remove_DataRequested(token));
}

TEST_F(FakeDataTransferManagerInteropTest,
       ShowShareUIForWindow_WithoutListener) {
  // Validate that ShowShareUIForWindow fails and causes a test failure when
  // called without a listener
  ASSERT_NO_FATAL_FAILURE(
      fake_data_transfer_manager_interop_->SetShowShareUIForWindowBehavior(
          ShowShareUIForWindowBehavior::SucceedWithoutAction));
  HRESULT hr;
  EXPECT_NONFATAL_FAILURE(
      hr = fake_data_transfer_manager_interop_->ShowShareUIForWindow(hwnd_1_),
      "ShowShareUIForWindow");
  ASSERT_HRESULT_FAILED(hr);
}

}  // namespace webshare
