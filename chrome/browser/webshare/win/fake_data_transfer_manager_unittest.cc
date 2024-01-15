// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/win/fake_data_transfer_manager.h"

#include <windows.storage.h>
#include <wrl/event.h>

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/win/scoped_hstring.h"
#include "base/win/scoped_winrt_initializer.h"
#include "base/win/vector.h"
#include "chrome/browser/webshare/win/fake_storage_file_statics.h"
#include "chrome/browser/webshare/win/fake_uri_runtime_class_factory.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"

using ABI::Windows::ApplicationModel::DataTransfer::DataRequestedEventArgs;
using ABI::Windows::ApplicationModel::DataTransfer::DataTransferManager;
using ABI::Windows::ApplicationModel::DataTransfer::IDataPackage;
using ABI::Windows::ApplicationModel::DataTransfer::IDataPackage2;
using ABI::Windows::ApplicationModel::DataTransfer::IDataPackagePropertySet;
using ABI::Windows::ApplicationModel::DataTransfer::IDataRequest;
using ABI::Windows::ApplicationModel::DataTransfer::IDataRequestDeferral;
using ABI::Windows::ApplicationModel::DataTransfer::IDataRequestedEventArgs;
using ABI::Windows::ApplicationModel::DataTransfer::IDataTransferManager;
using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Foundation::IAsyncOperationCompletedHandler;
using ABI::Windows::Foundation::ITypedEventHandler;
using ABI::Windows::Foundation::IUriRuntimeClass;
using ABI::Windows::Storage::IStorageFile;
using ABI::Windows::Storage::IStorageItem;
using ABI::Windows::Storage::IStreamedFileDataRequestedHandler;
using ABI::Windows::Storage::StorageFile;
using ABI::Windows::Storage::Streams::IOutputStream;
using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Make;

namespace ABI {
namespace Windows {
namespace Foundation {
namespace Collections {

// Define template specializations for the types used. These uuids were randomly
// generated.
template <>
struct __declspec(uuid("CBE31E85-DEC8-4227-987F-9C63D6AA1A2E"))
    IObservableVector<IStorageItem*> : IObservableVector_impl<IStorageItem*> {};

template <>
struct __declspec(uuid("30BE4864-5EE5-4111-916E-15126649F3C9"))
    VectorChangedEventHandler<IStorageItem*>
    : VectorChangedEventHandler_impl<IStorageItem*> {};

}  // namespace Collections
}  // namespace Foundation
}  // namespace Windows
}  // namespace ABI

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
  void SetUp() override {
    winrt_initializer_.emplace();
    ASSERT_TRUE(winrt_initializer_->Succeeded());
    fake_data_transfer_manager_ =
        Microsoft::WRL::Make<FakeDataTransferManager>();
  }

  std::optional<base::win::ScopedWinrtInitializer> winrt_initializer_;
  ComPtr<FakeDataTransferManager> fake_data_transfer_manager_;
};

TEST_F(FakeDataTransferManagerTest, RemovingHandlerForInvalidToken) {
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

TEST_F(FakeDataTransferManagerTest, PostDataRequestedCallback) {
  base::test::SingleThreadTaskEnvironment task_environment;

  // Create a StorageFile/Item to provide to the DataRequested event
  ComPtr<IStorageFile> storage_file;
  {
    base::RunLoop run_loop;
    auto file_name = base::win::ScopedHString::Create("MyTestFile.abc");
    ComPtr<IAsyncOperation<StorageFile*>> create_operation;
    auto storage_file_statics = Make<FakeStorageFileStatics>();
    storage_file_statics->CreateStreamedFileAsync(
        file_name.get(),
        Callback<IStreamedFileDataRequestedHandler>([](IOutputStream* stream) {
          ADD_FAILURE() << "DataRequestedHandler called for streamed file that "
                           "should never have been opened";
          return S_OK;
        }).Get(),
        /*thumbnail*/ nullptr, &create_operation);
    ASSERT_HRESULT_SUCCEEDED(create_operation->put_Completed(
        Callback<IAsyncOperationCompletedHandler<StorageFile*>>(
            [&run_loop, &storage_file](
                IAsyncOperation<StorageFile*>* async_operation,
                AsyncStatus async_status) {
              EXPECT_EQ(async_status, AsyncStatus::Completed);
              EXPECT_HRESULT_SUCCEEDED(
                  async_operation->GetResults(&storage_file));
              run_loop.Quit();
              return S_OK;
            })
            .Get()));
    run_loop.Run();
  }
  ComPtr<IStorageItem> storage_item;
  ASSERT_HRESULT_SUCCEEDED(storage_file.As(&storage_item));

  // Set up a handler for the DataRequested event that provides a collection of
  // content
  auto callback = Callback<
      ITypedEventHandler<DataTransferManager*, DataRequestedEventArgs*>>(
      [&storage_item](IDataTransferManager* data_transfer_manager,
                      IDataRequestedEventArgs* event_args) -> HRESULT {
        ComPtr<IDataRequest> data_request;
        EXPECT_HRESULT_SUCCEEDED(event_args->get_Request(&data_request));

        ComPtr<IDataPackage> data_package;
        EXPECT_HRESULT_SUCCEEDED(data_request->get_Data(&data_package));

        ComPtr<IDataPackagePropertySet> data_prop_sets;
        EXPECT_HRESULT_SUCCEEDED(data_package->get_Properties(&data_prop_sets));

        auto title_h = base::win::ScopedHString::Create("my title");
        EXPECT_HRESULT_SUCCEEDED(data_prop_sets->put_Title(title_h.get()));

        auto text_h = base::win::ScopedHString::Create("my text");
        EXPECT_HRESULT_SUCCEEDED(data_package->SetText(text_h.get()));

        auto uri_factory = Make<FakeUriRuntimeClassFactory>();
        auto url_h = base::win::ScopedHString::Create("https://my.url.com");
        ComPtr<IUriRuntimeClass> uri;
        EXPECT_HRESULT_SUCCEEDED(uri_factory->CreateUri(url_h.get(), &uri));
        ComPtr<IDataPackage2> data_package_2;
        EXPECT_HRESULT_SUCCEEDED(data_package.As(&data_package_2));
        EXPECT_HRESULT_SUCCEEDED(data_package_2->SetWebLink(uri.Get()));

        auto storage_items = Make<base::win::Vector<IStorageItem*>>();
        storage_items->Append(storage_item.Get());
        EXPECT_HRESULT_SUCCEEDED(data_package->SetStorageItems(
            storage_items.Get(), true /*readonly*/));

        return S_OK;
      });
  EventRegistrationToken token;
  ASSERT_HRESULT_SUCCEEDED(
      fake_data_transfer_manager_->add_DataRequested(callback.Get(), &token));

  // Set up a handler for the PostDataRequested event that validates all the
  // expected data
  bool post_data_requested_callback_invoked = false;
  fake_data_transfer_manager_->SetPostDataRequestedCallback(
      base::BindLambdaForTesting(
          [&post_data_requested_callback_invoked,
           &storage_file](const FakeDataTransferManager::DataRequestedContent&
                              data_requested_content) {
            ASSERT_FALSE(post_data_requested_callback_invoked);
            post_data_requested_callback_invoked = true;
            ASSERT_EQ(data_requested_content.title, "my title");
            ASSERT_EQ(data_requested_content.text, "my text");
            ASSERT_EQ(data_requested_content.uri, "https://my.url.com");
            ASSERT_EQ(data_requested_content.files.size(), 1u);
            ASSERT_EQ(data_requested_content.files[0].name, "MyTestFile.abc");
            ASSERT_EQ(data_requested_content.files[0].file, storage_file);
          }));

  // Run the flow
  auto callback_invoker =
      fake_data_transfer_manager_->GetDataRequestedInvoker();
  ASSERT_FALSE(post_data_requested_callback_invoked);
  std::move(callback_invoker).Run();
  ASSERT_TRUE(post_data_requested_callback_invoked);

  // Cleanup
  ASSERT_HRESULT_SUCCEEDED(
      fake_data_transfer_manager_->remove_DataRequested(token));
}

TEST_F(FakeDataTransferManagerTest, PostDataRequestedCallback_Deferral) {
  // Set up a handler for the DataRequested event that requests a deferral
  ComPtr<IDataRequestDeferral> data_request_deferral;
  auto callback = Callback<
      ITypedEventHandler<DataTransferManager*, DataRequestedEventArgs*>>(
      [&data_request_deferral](IDataTransferManager* data_transfer_manager,
                               IDataRequestedEventArgs* event_args) -> HRESULT {
        ComPtr<IDataRequest> data_request;
        EXPECT_HRESULT_SUCCEEDED(event_args->get_Request(&data_request));

        EXPECT_HRESULT_SUCCEEDED(
            data_request->GetDeferral(&data_request_deferral));

        return S_OK;
      });
  EventRegistrationToken token;
  ASSERT_HRESULT_SUCCEEDED(
      fake_data_transfer_manager_->add_DataRequested(callback.Get(), &token));

  // Set up a handler for the PostDataRequested event that records the event
  bool post_data_requested_callback_invoked = false;
  fake_data_transfer_manager_->SetPostDataRequestedCallback(
      base::BindLambdaForTesting(
          [&post_data_requested_callback_invoked](
              const FakeDataTransferManager::DataRequestedContent&
                  data_requested_content) {
            ASSERT_FALSE(post_data_requested_callback_invoked);
            post_data_requested_callback_invoked = true;
          }));

  // Run the flow
  auto callback_invoker =
      fake_data_transfer_manager_->GetDataRequestedInvoker();
  ASSERT_FALSE(post_data_requested_callback_invoked);
  ASSERT_FALSE(data_request_deferral);
  std::move(callback_invoker).Run();
  ASSERT_FALSE(post_data_requested_callback_invoked);
  ASSERT_TRUE(data_request_deferral);
  ASSERT_HRESULT_SUCCEEDED(data_request_deferral->Complete());
  ASSERT_TRUE(post_data_requested_callback_invoked);

  // Cleanup
  ASSERT_HRESULT_SUCCEEDED(
      fake_data_transfer_manager_->remove_DataRequested(token));
}

}  // namespace webshare
