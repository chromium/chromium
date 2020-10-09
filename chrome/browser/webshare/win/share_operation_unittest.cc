// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/win/share_operation.h"

#include "base/guid.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/win/core_winrt_util.h"
#include "base/win/post_async_results.h"
#include "chrome/browser/webshare/share_service_impl.h"
#include "chrome/browser/webshare/win/fake_data_transfer_manager.h"
#include "chrome/browser/webshare/win/fake_data_transfer_manager_interop.h"
#include "chrome/browser/webshare/win/scoped_fake_data_transfer_manager_interop.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/browser_task_traits.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "ui/views/win/hwnd_util.h"
#include "url/gurl.h"

#include <wrl/event.h>

using ABI::Windows::ApplicationModel::DataTransfer::IDataPackage;
using ABI::Windows::ApplicationModel::DataTransfer::IDataPackagePropertySet;
using ABI::Windows::ApplicationModel::DataTransfer::IDataRequest;
using ABI::Windows::ApplicationModel::DataTransfer::IDataRequestedEventArgs;
using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Foundation::IAsyncOperationCompletedHandler;
using ABI::Windows::Storage::FileAccessMode;
using ABI::Windows::Storage::IStorageFile;
using ABI::Windows::Storage::Streams::IDataReader;
using ABI::Windows::Storage::Streams::IDataReaderFactory;
using ABI::Windows::Storage::Streams::IInputStream;
using ABI::Windows::Storage::Streams::IRandomAccessStream;
using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

namespace webshare {
namespace {
constexpr base::TimeDelta kOperationWaitIncrement =
    base::TimeDelta::FromMilliseconds(100);
constexpr base::TimeDelta kMaxOperationWait = base::TimeDelta::FromSeconds(10);
constexpr uint64_t kMaxSharedFileBytesForTest = 1024 * 100;
}  // namespace

class ShareOperationUnitTest : public ChromeRenderViewHostTestHarness {
 public:
  ShareOperationUnitTest() {
    feature_list_.InitAndEnableFeature(features::kWebShare);
  }
  ~ShareOperationUnitTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    if (!IsSupportedEnvironment())
      return;

    scoped_interop_ = std::make_unique<ScopedFakeDataTransferManagerInterop>();
    ShareOperation::SetMaxFileBytesForTesting(kMaxSharedFileBytesForTest);
  }

  void TearDown() override {
    ChromeRenderViewHostTestHarness::TearDown();
    ShareOperation::SetMaxFileBytesForTesting(kMaxSharedFileBytes);
  }

 protected:
  // Waits/blocks for an operation to complete that involves code from the
  // system and our own internal code.
  template <typename T, typename TResult>
  void WaitForAsyncOperationWithSystemAndInternalCode(
      ComPtr<IAsyncOperation<T>>& operation,
      TResult& result) {
    base::WaitableEvent waitable_event;
    ASSERT_HRESULT_SUCCEEDED(base::win::PostAsyncResults(
        operation, base::BindLambdaForTesting(
                       [&result, &waitable_event](TResult returned_result) {
                         result = returned_result;
                         waitable_event.Signal();
                       })));

    base::TimeDelta time_waited;
    while (!waitable_event.IsSignaled() && time_waited < kMaxOperationWait) {
      task_environment()->RunUntilIdle();
      waitable_event.TimedWait(kOperationWaitIncrement);
      time_waited += kOperationWaitIncrement;
    }

    ASSERT_TRUE(waitable_event.IsSignaled());
  }

  void ReadFile(IStorageFile* file, std::string& result) {
    ComPtr<IAsyncOperation<IRandomAccessStream*>> open_operation;
    ASSERT_HRESULT_SUCCEEDED(
        file->OpenAsync(FileAccessMode::FileAccessMode_Read, &open_operation));

    ComPtr<IRandomAccessStream> stream;
    ASSERT_NO_FATAL_FAILURE(
        WaitForAsyncOperationWithSystemAndInternalCode(open_operation, stream));
    ASSERT_TRUE(stream);

    UINT64 size;
    ASSERT_HRESULT_SUCCEEDED(stream->get_Size(&size));

    ComPtr<IInputStream> input_stream;
    ASSERT_HRESULT_SUCCEEDED(stream->GetInputStreamAt(0, &input_stream));

    ComPtr<IDataReaderFactory> data_reader_factory;
    HRESULT hr = base::win::GetActivationFactory<
        IDataReaderFactory, RuntimeClass_Windows_Storage_Streams_DataReader>(
        &data_reader_factory);
    ASSERT_HRESULT_SUCCEEDED(hr);

    ComPtr<IDataReader> data_reader;
    ASSERT_HRESULT_SUCCEEDED(data_reader_factory->CreateDataReader(
        input_stream.Get(), &data_reader));

    ComPtr<IAsyncOperation<UINT32>> load_operation;
    ASSERT_HRESULT_SUCCEEDED(data_reader->LoadAsync(size, &load_operation));

    UINT32 bytes_loaded;
    ASSERT_NO_FATAL_FAILURE(WaitForAsyncOperationWithSystemAndInternalCode(
        load_operation, bytes_loaded));
    ASSERT_NE(bytes_loaded, 0u);

    std::vector<unsigned char> bytes(bytes_loaded);
    ASSERT_HRESULT_SUCCEEDED(
        data_reader->ReadBytes(bytes_loaded, bytes.data()));

    result = std::string(bytes.begin(), bytes.end());
  }

  blink::mojom::SharedFilePtr CreateSharedFile(const std::string& name,
                                               const std::string& content_type,
                                               const std::string& contents) {
    auto blob = blink::mojom::SerializedBlob::New();
    const std::string uuid = base::GenerateGUID();
    blob->uuid = uuid;
    blob->content_type = content_type;
    blob->size = contents.size();

    base::RunLoop run_loop;
    auto blob_context_getter =
        content::BrowserContext::GetBlobStorageContext(browser_context());
    content::GetIOThreadTaskRunner({})->PostTaskAndReply(
        FROM_HERE,
        base::BindLambdaForTesting([&blob_context_getter, &blob, &uuid,
                                    &content_type, &contents]() {
          auto builder = std::make_unique<storage::BlobDataBuilder>(uuid);
          builder->set_content_type(content_type);
          builder->AppendData(contents);
          storage::BlobImpl::Create(
              blob_context_getter.Run()->AddFinishedBlob(std::move(builder)),
              blob->blob.InitWithNewPipeAndPassReceiver());
        }),
        base::BindLambdaForTesting([&run_loop]() { run_loop.Quit(); }));
    run_loop.Run();
    return blink::mojom::SharedFile::New(name, std::move(blob));
  }

  bool IsSupportedEnvironment() {
    return ScopedFakeDataTransferManagerInterop::IsSupportedEnvironment();
  }

  // Fetches the FakeDataTransferManager associated with the current context.
  // Returns a non-ref-counted pointer, as the lifetime is already maintained by
  // the scoped_interop_.
  FakeDataTransferManager* fake_data_transfer_manager() {
    if (!fake_data_transfer_manager_) {
      HWND hwnd =
          views::HWNDForNativeWindow(web_contents()->GetTopLevelNativeWindow());
      ComPtr<FakeDataTransferManager> fake_data_transfer_manager;
      EXPECT_HRESULT_SUCCEEDED(scoped_interop_->instance().GetForWindow(
          hwnd, IID_PPV_ARGS(&fake_data_transfer_manager)));
      fake_data_transfer_manager_ = fake_data_transfer_manager.Get();
    }
    return fake_data_transfer_manager_;
  }

 private:
  FakeDataTransferManager* fake_data_transfer_manager_ = nullptr;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<ScopedFakeDataTransferManagerInterop> scoped_interop_;
};

TEST_F(ShareOperationUnitTest, WithoutTitle) {
  if (!IsSupportedEnvironment())
    return;

  bool post_data_requested_callback_invoked = false;
  fake_data_transfer_manager()->SetPostDataRequestedCallback(
      base::BindLambdaForTesting(
          [&post_data_requested_callback_invoked](
              const FakeDataTransferManager::DataRequestedContent&
                  data_requested_content) {
            ASSERT_FALSE(post_data_requested_callback_invoked);
            post_data_requested_callback_invoked = true;
            ASSERT_EQ(data_requested_content.title, " ");
            ASSERT_EQ(data_requested_content.text, "shared Text");
          }));

  base::RunLoop run_loop;
  std::vector<blink::mojom::SharedFilePtr> files;
  ShareOperation operation{"", "shared Text", GURL::EmptyGURL(),
                           std::move(files), web_contents()};
  operation.Run(
      base::BindLambdaForTesting([&run_loop](blink::mojom::ShareError error) {
        ASSERT_EQ(error, blink::mojom::ShareError::OK);
        run_loop.Quit();
      }));
  run_loop.Run();

  ASSERT_TRUE(post_data_requested_callback_invoked);
}

TEST_F(ShareOperationUnitTest, BasicFields) {
  if (!IsSupportedEnvironment())
    return;

  bool post_data_requested_callback_invoked = false;
  fake_data_transfer_manager()->SetPostDataRequestedCallback(
      base::BindLambdaForTesting(
          [&post_data_requested_callback_invoked](
              const FakeDataTransferManager::DataRequestedContent&
                  data_requested_content) {
            ASSERT_FALSE(post_data_requested_callback_invoked);
            post_data_requested_callback_invoked = true;
            ASSERT_EQ(data_requested_content.title, "shared title");
            ASSERT_EQ(data_requested_content.text, "shared text");
            ASSERT_EQ(GURL(data_requested_content.uri),
                      GURL("https://www.contoso.com"));
          }));

  base::RunLoop run_loop;
  std::vector<blink::mojom::SharedFilePtr> files;
  ShareOperation operation{"shared title", "shared text",
                           GURL("https://www.contoso.com"), std::move(files),
                           web_contents()};
  operation.Run(
      base::BindLambdaForTesting([&run_loop](blink::mojom::ShareError error) {
        ASSERT_EQ(error, blink::mojom::ShareError::OK);
        run_loop.Quit();
      }));
  run_loop.Run();

  ASSERT_TRUE(post_data_requested_callback_invoked);
}

TEST_F(ShareOperationUnitTest, BasicFile) {
  if (!IsSupportedEnvironment())
    return;

  bool post_data_requested_callback_invoked = false;
  fake_data_transfer_manager()->SetPostDataRequestedCallback(
      base::BindLambdaForTesting(
          [&](const FakeDataTransferManager::DataRequestedContent&
                  data_requested_content) {
            ASSERT_FALSE(post_data_requested_callback_invoked);
            post_data_requested_callback_invoked = true;
            ASSERT_EQ(data_requested_content.title, "shared title");
            ASSERT_EQ(data_requested_content.files.size(), 1ull);
            ASSERT_EQ(data_requested_content.files[0].name, "MyFile.txt");
            std::string file_contents;
            ASSERT_NO_FATAL_FAILURE(ReadFile(
                data_requested_content.files[0].file.Get(), file_contents));
            ASSERT_EQ(file_contents, "Contents of the file");
          }));

  base::RunLoop run_loop;
  std::vector<blink::mojom::SharedFilePtr> files;
  files.push_back(
      CreateSharedFile("MyFile.txt", "text/plain", "Contents of the file"));
  ShareOperation operation{"shared title", "", GURL::EmptyGURL(),
                           std::move(files), web_contents()};
  operation.Run(
      base::BindLambdaForTesting([&run_loop](blink::mojom::ShareError error) {
        ASSERT_EQ(error, blink::mojom::ShareError::OK);
        run_loop.Quit();
      }));
  run_loop.Run();

  ASSERT_TRUE(post_data_requested_callback_invoked);
}

TEST_F(ShareOperationUnitTest, SingleFileAtSizeLimit) {
  if (!IsSupportedEnvironment())
    return;

  bool post_data_requested_callback_invoked = false;
  fake_data_transfer_manager()->SetPostDataRequestedCallback(
      base::BindLambdaForTesting(
          [&](const FakeDataTransferManager::DataRequestedContent&
                  data_requested_content) {
            ASSERT_FALSE(post_data_requested_callback_invoked);
            post_data_requested_callback_invoked = true;
            ASSERT_EQ(data_requested_content.files.size(), 1ull);
            std::string file_contents;
            ASSERT_NO_FATAL_FAILURE(ReadFile(
                data_requested_content.files[0].file.Get(), file_contents));
            ASSERT_EQ(file_contents.length(), kMaxSharedFileBytesForTest);
          }));

  base::RunLoop run_loop;
  std::vector<blink::mojom::SharedFilePtr> files;
  files.push_back(
      CreateSharedFile("MyFile.txt", "text/plain",
                       std::string(kMaxSharedFileBytesForTest, '*')));
  ShareOperation operation{"", "", GURL::EmptyGURL(), std::move(files),
                           web_contents()};
  operation.Run(
      base::BindLambdaForTesting([&run_loop](blink::mojom::ShareError error) {
        ASSERT_EQ(error, blink::mojom::ShareError::OK);
        run_loop.Quit();
      }));
  run_loop.Run();

  ASSERT_TRUE(post_data_requested_callback_invoked);
}

TEST_F(ShareOperationUnitTest, SingleFileLargerThanSizeLimit) {
  if (!IsSupportedEnvironment())
    return;

  bool post_data_requested_callback_invoked = false;
  fake_data_transfer_manager()->SetPostDataRequestedCallback(
      base::BindLambdaForTesting(
          [&](const FakeDataTransferManager::DataRequestedContent&
                  data_requested_content) {
            ASSERT_FALSE(post_data_requested_callback_invoked);
            post_data_requested_callback_invoked = true;
            ASSERT_EQ(data_requested_content.files.size(), 1ull);
            std::string file_contents;
            ASSERT_NO_FATAL_FAILURE(ReadFile(
                data_requested_content.files[0].file.Get(), file_contents));
            ASSERT_LT(file_contents.length(), kMaxSharedFileBytesForTest + 1);
          }));

  base::RunLoop run_loop;
  std::vector<blink::mojom::SharedFilePtr> files;
  files.push_back(
      CreateSharedFile("MyFile.txt", "text/plain",
                       std::string(kMaxSharedFileBytesForTest + 1, '*')));
  ShareOperation operation{"", "", GURL::EmptyGURL(), std::move(files),
                           web_contents()};
  operation.Run(
      base::BindLambdaForTesting([&run_loop](blink::mojom::ShareError error) {
        ASSERT_EQ(error, blink::mojom::ShareError::OK);
        run_loop.Quit();
      }));
  run_loop.Run();

  ASSERT_TRUE(post_data_requested_callback_invoked);
}

TEST_F(ShareOperationUnitTest, FilesTotallingSizeLimit) {
  if (!IsSupportedEnvironment())
    return;

  bool post_data_requested_callback_invoked = false;
  fake_data_transfer_manager()->SetPostDataRequestedCallback(
      base::BindLambdaForTesting(
          [&](const FakeDataTransferManager::DataRequestedContent&
                  data_requested_content) {
            ASSERT_FALSE(post_data_requested_callback_invoked);
            post_data_requested_callback_invoked = true;
            ASSERT_EQ(data_requested_content.files.size(), 2ull);
            std::string file1_contents;
            ASSERT_NO_FATAL_FAILURE(ReadFile(
                data_requested_content.files[0].file.Get(), file1_contents));
            std::string file2_contents;
            ASSERT_NO_FATAL_FAILURE(ReadFile(
                data_requested_content.files[0].file.Get(), file2_contents));
            ASSERT_EQ(file1_contents.length() + file2_contents.length(),
                      kMaxSharedFileBytesForTest);
          }));

  base::RunLoop run_loop;
  std::vector<blink::mojom::SharedFilePtr> files;
  files.push_back(
      CreateSharedFile("File1.txt", "text/plain",
                       std::string(kMaxSharedFileBytesForTest / 2, '*')));
  files.push_back(
      CreateSharedFile("File2.txt", "text/plain",
                       std::string(kMaxSharedFileBytesForTest / 2, '*')));
  ShareOperation operation{"", "", GURL::EmptyGURL(), std::move(files),
                           web_contents()};
  operation.Run(
      base::BindLambdaForTesting([&run_loop](blink::mojom::ShareError error) {
        ASSERT_EQ(error, blink::mojom::ShareError::OK);
        run_loop.Quit();
      }));
  run_loop.Run();

  ASSERT_TRUE(post_data_requested_callback_invoked);
}

TEST_F(ShareOperationUnitTest, FilesTotallingLargerThanSizeLimit) {
  if (!IsSupportedEnvironment())
    return;

  bool post_data_requested_callback_invoked = false;
  fake_data_transfer_manager()->SetPostDataRequestedCallback(
      base::BindLambdaForTesting(
          [&](const FakeDataTransferManager::DataRequestedContent&
                  data_requested_content) {
            ASSERT_FALSE(post_data_requested_callback_invoked);
            post_data_requested_callback_invoked = true;
            ASSERT_EQ(data_requested_content.files.size(), 2ull);
            std::string file1_contents;
            ASSERT_NO_FATAL_FAILURE(ReadFile(
                data_requested_content.files[0].file.Get(), file1_contents));
            std::string file2_contents;
            ASSERT_NO_FATAL_FAILURE(ReadFile(
                data_requested_content.files[1].file.Get(), file2_contents));
            ASSERT_LT(file1_contents.length() + file2_contents.length(),
                      kMaxSharedFileBytesForTest + 1);
          }));

  base::RunLoop run_loop;
  std::vector<blink::mojom::SharedFilePtr> files;
  files.push_back(
      CreateSharedFile("File1.txt", "text/plain",
                       std::string(kMaxSharedFileBytesForTest / 2, '*')));
  files.push_back(
      CreateSharedFile("File2.txt", "text/plain",
                       std::string((kMaxSharedFileBytesForTest / 2) + 1, '*')));
  ShareOperation operation{"", "", GURL::EmptyGURL(), std::move(files),
                           web_contents()};
  operation.Run(
      base::BindLambdaForTesting([&run_loop](blink::mojom::ShareError error) {
        ASSERT_EQ(error, blink::mojom::ShareError::OK);
        run_loop.Quit();
      }));
  run_loop.Run();

  ASSERT_TRUE(post_data_requested_callback_invoked);
}

}  // namespace webshare
