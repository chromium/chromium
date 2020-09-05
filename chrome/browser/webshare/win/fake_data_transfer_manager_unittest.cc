// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/win/fake_data_transfer_manager.h"

#include <wrl/event.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/win/core_winrt_util.h"
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

class FakeDataTransferManagerTest : public ::testing::Test {
 protected:
  bool IsSupportedEnvironment() {
    return base::win::ResolveCoreWinRTDelayload() &&
           base::win::ScopedHString::ResolveCoreWinRTStringDelayload();
  }

  void SetUp() override {
    if (!IsSupportedEnvironment())
      return;
    ASSERT_HRESULT_SUCCEEDED(
        base::win::RoInitialize(RO_INIT_TYPE::RO_INIT_MULTITHREADED));
    fake_data_transfer_manager_ =
        Microsoft::WRL::Make<FakeDataTransferManager>();
  }

  void TearDown() override {
    if (!IsSupportedEnvironment())
      return;
    base::win::RoUninitialize();
  }

  ComPtr<FakeDataTransferManager> fake_data_transfer_manager_;
};

TEST_F(FakeDataTransferManagerTest, RemovingHandlerForInvalidToken) {
  if (!IsSupportedEnvironment())
    return;

  // Validate removing an invalid token both fails and creates a test failure
  // when there is no listener
  EventRegistrationToken invalid_token;
  EXPECT_NONFATAL_FAILURE(
      ASSERT_HRESULT_FAILED(
          fake_data_transfer_manager_->remove_DataRequested(invalid_token)),
      "remove_DataRequested");
  invalid_token.value = 123;
  EXPECT_NONFATAL_FAILURE(
      ASSERT_HRESULT_FAILED(
          fake_data_transfer_manager_->remove_DataRequested(invalid_token)),
      "remove_DataRequested");

  // Validate removing an invalid token both fails and creates a test failure
  // when there is a listener
  EventRegistrationToken valid_token;
  DataRequestedTestCallback test_callback;
  ASSERT_HRESULT_SUCCEEDED(fake_data_transfer_manager_->add_DataRequested(
      test_callback.callback_.Get(), &valid_token));
  EXPECT_NONFATAL_FAILURE(
      ASSERT_HRESULT_FAILED(
          fake_data_transfer_manager_->remove_DataRequested(invalid_token)),
      "remove_DataRequested");

  // Validate removing a valid token is successful only once, failing and
  // creating a test failure on repeated uses
  ASSERT_HRESULT_SUCCEEDED(
      fake_data_transfer_manager_->remove_DataRequested(valid_token));
  EXPECT_NONFATAL_FAILURE(
      ASSERT_HRESULT_FAILED(
          fake_data_transfer_manager_->remove_DataRequested(valid_token)),
      "remove_DataRequested");
}

TEST_F(FakeDataTransferManagerTest, OutOfOrderEventUnsubscribing) {
  if (!IsSupportedEnvironment())
    return;

  ASSERT_FALSE(fake_data_transfer_manager_->HasDataRequestedListener());

  DataRequestedTestCallback callback_1;
  EventRegistrationToken token_1;
  ASSERT_HRESULT_SUCCEEDED(fake_data_transfer_manager_->add_DataRequested(
      callback_1.callback_.Get(), &token_1));
  ASSERT_TRUE(fake_data_transfer_manager_->HasDataRequestedListener());

  DataRequestedTestCallback callback_2;
  EventRegistrationToken token_2;
  ASSERT_HRESULT_SUCCEEDED(fake_data_transfer_manager_->add_DataRequested(
      callback_2.callback_.Get(), &token_2));
  ASSERT_TRUE(fake_data_transfer_manager_->HasDataRequestedListener());

  DataRequestedTestCallback callback_3;
  EventRegistrationToken token_3;
  ASSERT_HRESULT_SUCCEEDED(fake_data_transfer_manager_->add_DataRequested(
      callback_3.callback_.Get(), &token_3));
  ASSERT_TRUE(fake_data_transfer_manager_->HasDataRequestedListener());

  ASSERT_EQ(callback_1.invocation_count_, 0);
  ASSERT_EQ(callback_2.invocation_count_, 0);
  ASSERT_EQ(callback_3.invocation_count_, 0);

  ASSERT_HRESULT_SUCCEEDED(
      fake_data_transfer_manager_->remove_DataRequested(token_2));
  ASSERT_TRUE(fake_data_transfer_manager_->HasDataRequestedListener());

  std::move(fake_data_transfer_manager_->GetDataRequestedInvoker()).Run();
  ASSERT_EQ(callback_1.invocation_count_, 0);
  ASSERT_EQ(callback_2.invocation_count_, 0);
  ASSERT_EQ(callback_3.invocation_count_, 1);

  ASSERT_HRESULT_SUCCEEDED(
      fake_data_transfer_manager_->remove_DataRequested(token_3));
  ASSERT_TRUE(fake_data_transfer_manager_->HasDataRequestedListener());

  std::move(fake_data_transfer_manager_->GetDataRequestedInvoker()).Run();
  ASSERT_EQ(callback_1.invocation_count_, 1);
  ASSERT_EQ(callback_2.invocation_count_, 0);
  ASSERT_EQ(callback_3.invocation_count_, 1);

  ASSERT_HRESULT_SUCCEEDED(
      fake_data_transfer_manager_->remove_DataRequested(token_1));
  ASSERT_FALSE(fake_data_transfer_manager_->HasDataRequestedListener());

  EXPECT_NONFATAL_FAILURE(
      fake_data_transfer_manager_->GetDataRequestedInvoker(),
      "GetDataRequestedInvoker");
}

TEST_F(FakeDataTransferManagerTest, OutOfOrderEventInvocation) {
  if (!IsSupportedEnvironment())
    return;

  DataRequestedTestCallback callback_1;
  EventRegistrationToken token_1;
  ASSERT_HRESULT_SUCCEEDED(fake_data_transfer_manager_->add_DataRequested(
      callback_1.callback_.Get(), &token_1));
  auto callback_1_invoker =
      fake_data_transfer_manager_->GetDataRequestedInvoker();

  DataRequestedTestCallback callback_2;
  EventRegistrationToken token_2;
  ASSERT_HRESULT_SUCCEEDED(fake_data_transfer_manager_->add_DataRequested(
      callback_2.callback_.Get(), &token_2));
  auto callback_2_invoker =
      fake_data_transfer_manager_->GetDataRequestedInvoker();

  DataRequestedTestCallback callback_3;
  EventRegistrationToken token_3;
  ASSERT_HRESULT_SUCCEEDED(fake_data_transfer_manager_->add_DataRequested(
      callback_3.callback_.Get(), &token_3));
  auto callback_3_invoker =
      fake_data_transfer_manager_->GetDataRequestedInvoker();

  ASSERT_EQ(callback_1.invocation_count_, 0);
  ASSERT_EQ(callback_2.invocation_count_, 0);
  ASSERT_EQ(callback_3.invocation_count_, 0);

  std::move(callback_2_invoker).Run();
  ASSERT_EQ(callback_1.invocation_count_, 0);
  ASSERT_EQ(callback_2.invocation_count_, 1);
  ASSERT_EQ(callback_3.invocation_count_, 0);

  std::move(callback_3_invoker).Run();
  ASSERT_EQ(callback_1.invocation_count_, 0);
  ASSERT_EQ(callback_2.invocation_count_, 1);
  ASSERT_EQ(callback_3.invocation_count_, 1);

  ASSERT_HRESULT_SUCCEEDED(
      fake_data_transfer_manager_->remove_DataRequested(token_1));
  ASSERT_HRESULT_SUCCEEDED(
      fake_data_transfer_manager_->remove_DataRequested(token_2));
  ASSERT_HRESULT_SUCCEEDED(
      fake_data_transfer_manager_->remove_DataRequested(token_3));

  std::move(callback_1_invoker).Run();
  ASSERT_EQ(callback_1.invocation_count_, 1);
  ASSERT_EQ(callback_2.invocation_count_, 1);
  ASSERT_EQ(callback_3.invocation_count_, 1);
}

}  // namespace webshare
