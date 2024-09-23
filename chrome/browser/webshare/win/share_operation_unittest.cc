// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/webshare/win/share_operation.h"

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "base/win/core_winrt_util.h"
#include "base/win/post_async_results.h"
#include "chrome/browser/webshare/share_service_impl.h"
#include "chrome/browser/webshare/win/fake_buffer.h"
#include "chrome/browser/webshare/win/fake_data_transfer_manager.h"
#include "chrome/browser/webshare/win/fake_data_transfer_manager_interop.h"
#include "chrome/browser/webshare/win/scoped_share_operation_fake_components.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
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
using ABI::Windows::Foundation::IAsyncOperationWithProgress;
using ABI::Windows::Foundation::IAsyncOperationWithProgressCompletedHandler;
using ABI::Windows::Foundation::IClosable;
using ABI::Windows::Storage::FileAccessMode;
using ABI::Windows::Storage::IStorageFile;
using ABI::Windows::Storage::Streams::IBuffer;
using ABI::Windows::Storage::Streams::IDataReader;
using ABI::Windows::Storage::Streams::IDataReaderFactory;
using ABI::Windows::Storage::Streams::IInputStream;
using ABI::Windows::Storage::Streams::InputStreamOptions;
using ABI::Windows::Storage::Streams::IRandomAccessStream;
using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Make;

namespace webshare {
namespace {
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

    ASSERT_NO_FATAL_FAILURE(scoped_fake_components_.SetUp());
    ShareOperation::SetMaxFileBytesForTesting(kMaxSharedFileBytesForTest);
  }

  void TearDown() override {
    ChromeRenderViewHostTestHarness::TearDown();
    ShareOperation::SetMaxFileBytesForTesting(kMaxSharedFileBytes);
  }

 protected:
  void ReadFile(IStorageFile* file, std::string& result) {
    ComPtr<IRandomAccessStream> stream;
    {
      base::RunLoop run_loop;
      ComPtr<IAsyncOperation<IRandomAccessStream*>> open_operation;
      ASSERT_HRESULT_SUCCEEDED(file->OpenAsync(
          FileAccessMode::FileAccessMode_Read, &open_operation));
      ASSERT_HRESULT_SUCCEEDED(open_operation->put_Completed(
          Callback<IAsyncOperationCompletedHandler<IRandomAccessStream*>>(
              [&run_loop, &stream](
                  IAsyncOperation<IRandomAccessStream*>* async_operation,
                  AsyncStatus async_status) {
                EXPECT_EQ(async_status, AsyncStatus::Completed);
                EXPECT_HRESULT_SUCCEEDED(async_operation->GetResults(&stream));
                run_loop.Quit();
                return S_OK;
              })
              .Get()));
      run_loop.Run();
    }

    UINT64 size;
    ASSERT_HRESULT_SUCCEEDED(stream->get_Size(&size));

    ComPtr<IInputStream> input_stream;
    ASSERT_HRESULT_SUCCEEDED(stream->GetInputStreamAt(0, &input_stream));
    ComPtr<IClosable> closable_stream;
    ASSERT_HRESULT_SUCCEEDED(stream.As(&closable_stream));
    ASSERT_HRESULT_SUCCEEDED(closable_stream->Close());

    auto buffer = Make<FakeBuffer>(size);
    {
      base::RunLoop run_loop;
      ComPtr<IAsyncOperationWithProgress<IBuffer*, UINT32>> read_operation;
      ASSERT_HRESULT_SUCCEEDED(input_stream->ReadAsync(
          buffer.Get(), size, InputStreamOptions::InputStreamOptions_None,
          &read_operation));
      ASSERT_HRESULT_SUCCEEDED(read_operation->put_Completed(
          Callback<
              IAsyncOperationWithProgressCompletedHandler<IBuffer*, UINT32>>(
              [&run_loop](IAsyncOperationWithProgress<IBuffer*, UINT32>*
                              async_operation,
                          AsyncStatus async_status) {
                EXPECT_EQ(async_status, AsyncStatus::Completed);
                run_loop.Quit();
                return S_OK;
              })
              .Get()));
      run_loop.Run();
    }

    UINT32 bytes_loaded;
    ASSERT_HRESULT_SUCCEEDED(buffer->get_Length(&bytes_loaded));
    ASSERT_EQ(size, bytes_loaded);

    BYTE* raw_buffer;
    ASSERT_HRESULT_SUCCEEDED(buffer->Buffer(&raw_buffer));

    std::vector<unsigned char> bytes(bytes_loaded);
    for (UINT32 i = 0; i < bytes_loaded; i++) {
      bytes[i] = raw_buffer[i];
    }
    result = std::string(bytes.begin(), bytes.end());

    // Cleanup
    ComPtr<IClosable> closable_input_stream;
    ASSERT_HRESULT_SUCCEEDED(input_stream.As(&closable_input_stream));
    ASSERT_HRESULT_SUCCEEDED(closable_input_stream->Close());
  }

  blink::mojom::SharedFilePtr CreateSharedFile(
      base::FilePath::StringPieceType name,
      const std::string& content_type,
      const std::string& contents) {
    auto blob = blink::mojom::SerializedBlob::New();
    const std::string uuid = base::Uuid::GenerateRandomV4().AsLowercaseString();
    blob->uuid = uuid;
    blob->content_type = content_type;
    blob->size = contents.size();

    base::RunLoop run_loop;
    auto blob_context_getter = browser_context()->GetBlobStorageContext();
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
    return blink::mojom::SharedFile::New(*base::SafeBaseName::Create(name),
                                         std::move(blob));
  }

  // Fetches the FakeDataTransferManager associated with the current context.
  // Returns a non-ref-counted pointer, as the lifetime is already maintained by
  // the scoped_fake_components_.
  FakeDataTransferManager* fake_data_transfer_manager() {
    if (!fake_data_transfer_manager_) {
      HWND hwnd =
          views::HWNDForNativeWindow(web_contents()->GetTopLevelNativeWindow());
      ComPtr<FakeDataTransferManager> fake_data_transfer_manager;
      EXPECT_HRESULT_SUCCEEDED(
          scoped_fake_components_.fake_data_transfer_manager_interop()
              .GetForWindow(hwnd, IID_PPV_ARGS(&fake_data_transfer_manager)));
      fake_data_transfer_manager_ = fake_data_transfer_manager.Get();
    }
    return fake_data_transfer_manager_;
  }

  FakeDataTransferManagerInterop& fake_data_transfer_manager_interop() {
    return scoped_fake_components_.fake_data_transfer_manager_interop();
  }

 private:
  raw_ptr<FakeDataTransferManager, DanglingUntriaged>
      fake_data_transfer_manager_ = nullptr;
  base::test::ScopedFeatureList feature_list_;
  ScopedShareOperationFakeComponents scoped_fake_components_;
};

TEST_F(ShareOperationUnitTest, WithoutTitle) {
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
  ShareOperation operation{"", "shared Text", GURL(), std::move(files),
                           web_contents()};
  operation.Run(
      base::BindLambdaForTesting([&run_loop](blink::mojom::ShareError error) {
        ASSERT_EQ(error, blink::mojom::ShareError::OK);
        run_loop.Quit();
      }));
  run_loop.Run();

  ASSERT_TRUE(post_data_requested_callback_invoked);
}

TEST_F(ShareOperationUnitTest, BasicFields) {
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

TEST_F(ShareOperationUnitTest, ShowShareUIForWindowFailure) {
  fake_data_transfer_manager_interop().SetShowShareUIForWindowBehavior(
      FakeDataTransferManagerInterop::ShowShareUIForWindowBehavior::
          FailImmediately);

  base::RunLoop run_loop;
  std::vector<blink::mojom::SharedFilePtr> files;
  ShareOperation operation{"shared title", "shared text",
                           GURL("https://www.contoso.com"), std::move(files),
                           web_contents()};
  operation.Run(
      base::BindLambdaForTesting([&run_loop](blink::mojom::ShareError error) {
        ASSERT_EQ(error, blink::mojom::ShareError::INTERNAL_ERROR);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ShareOperationUnitTest, BasicFile) {
  bool post_data_requested_callback_invoked = false;
  ComPtr<IStorageFile> shared_file;
  fake_data_transfer_manager()->SetPostDataRequestedCallback(
      base::BindLambdaForTesting(
          [&](const FakeDataTransferManager::DataRequestedContent&
                  data_requested_content) {
            ASSERT_FALSE(post_data_requested_callback_invoked);
            post_data_requested_callback_invoked = true;
            ASSERT_EQ(data_requested_content.title, "shared title");
            ASSERT_EQ(data_requested_content.files.size(), 1ull);
            ASSERT_EQ(data_requested_content.files[0].name, "MyFile.txt");
            shared_file = data_requested_content.files[0].file;
          }));

  base::RunLoop run_loop;
  std::vector<blink::mojom::SharedFilePtr> files;
  files.push_back(CreateSharedFile(FILE_PATH_LITERAL("MyFile.txt"),
                                   "text/plain", "Contents of the file"));
  ShareOperation operation{"shared title", "", GURL(), std::move(files),
                           web_contents()};
  operation.Run(
      base::BindLambdaForTesting([&run_loop](blink::mojom::ShareError error) {
        ASSERT_EQ(error, blink::mojom::ShareError::OK);
        run_loop.Quit();
      }));
  run_loop.Run();

  ASSERT_TRUE(post_data_requested_callback_invoked);
  ASSERT_TRUE(shared_file);
  std::string file_contents;
  ASSERT_NO_FATAL_FAILURE(ReadFile(shared_file.Get(), file_contents));
  ASSERT_EQ(file_contents, "Contents of the file");
}

TEST_F(ShareOperationUnitTest, SingleFileAtSizeLimit) {
  bool post_data_requested_callback_invoked = false;
  ComPtr<IStorageFile> shared_file;
  fake_data_transfer_manager()->SetPostDataRequestedCallback(
      base::BindLambdaForTesting(
          [&](const FakeDataTransferManager::DataRequestedContent&
                  data_requested_content) {
            ASSERT_FALSE(post_data_requested_callback_invoked);
            post_data_requested_callback_invoked = true;
            ASSERT_EQ(data_requested_content.files.size(), 1ull);
            shared_file = data_requested_content.files[0].file;
          }));

  base::RunLoop run_loop;
  std::vector<blink::mojom::SharedFilePtr> files;
  files.push_back(
      CreateSharedFile(FILE_PATH_LITERAL("MyFile.txt"), "text/plain",
                       std::string(kMaxSharedFileBytesForTest, '*')));
  ShareOperation operation{"", "", GURL(), std::move(files), web_contents()};
  operation.Run(
      base::BindLambdaForTesting([&run_loop](blink::mojom::ShareError error) {
        ASSERT_EQ(error, blink::mojom::ShareError::OK);
        run_loop.Quit();
      }));
  run_loop.Run();

  ASSERT_TRUE(post_data_requested_callback_invoked);
  ASSERT_TRUE(shared_file);
  std::string file_contents;
  ASSERT_NO_FATAL_FAILURE(ReadFile(shared_file.Get(), file_contents));
  ASSERT_EQ(file_contents.length(), kMaxSharedFileBytesForTest);
}

TEST_F(ShareOperationUnitTest, SingleFileLargerThanSizeLimit) {
  bool post_data_requested_callback_invoked = false;
  ComPtr<IStorageFile> shared_file;
  fake_data_transfer_manager()->SetPostDataRequestedCallback(
      base::BindLambdaForTesting(
          [&](const FakeDataTransferManager::DataRequestedContent&
                  data_requested_content) {
            ASSERT_FALSE(post_data_requested_callback_invoked);
            post_data_requested_callback_invoked = true;
            ASSERT_EQ(data_requested_content.files.size(), 1ull);
            shared_file = data_requested_content.files[0].file;
          }));

  base::RunLoop run_loop;
  std::vector<blink::mojom::SharedFilePtr> files;
  files.push_back(
      CreateSharedFile(FILE_PATH_LITERAL("MyFile.txt"), "text/plain",
                       std::string(kMaxSharedFileBytesForTest + 1, '*')));
  ShareOperation operation{"", "", GURL(), std::move(files), web_contents()};
  operation.Run(
      base::BindLambdaForTesting([&run_loop](blink::mojom::ShareError error) {
        ASSERT_EQ(error, blink::mojom::ShareError::OK);
        run_loop.Quit();
      }));
  run_loop.Run();

  ASSERT_TRUE(post_data_requested_callback_invoked);
  ASSERT_TRUE(shared_file);
  std::string file_contents;
  ASSERT_NO_FATAL_FAILURE(ReadFile(shared_file.Get(), file_contents));
  ASSERT_LT(file_contents.length(), kMaxSharedFileBytesForTest + 1);
}

TEST_F(ShareOperationUnitTest, FilesTotallingSizeLimit) {
  bool post_data_requested_callback_invoked = false;
  ComPtr<IStorageFile> shared_file_1;
  ComPtr<IStorageFile> shared_file_2;
  fake_data_transfer_manager()->SetPostDataRequestedCallback(
      base::BindLambdaForTesting(
          [&](const FakeDataTransferManager::DataRequestedContent&
                  data_requested_content) {
            ASSERT_FALSE(post_data_requested_callback_invoked);
            post_data_requested_callback_invoked = true;
            ASSERT_EQ(data_requested_content.files.size(), 2ull);
            shared_file_1 = data_requested_content.files[0].file;
            shared_file_2 = data_requested_content.files[1].file;
          }));

  base::RunLoop run_loop;
  std::vector<blink::mojom::SharedFilePtr> files;
  files.push_back(
      CreateSharedFile(FILE_PATH_LITERAL("File1.txt"), "text/plain",
                       std::string(kMaxSharedFileBytesForTest / 2, '*')));
  files.push_back(
      CreateSharedFile(FILE_PATH_LITERAL("File2.txt"), "text/plain",
                       std::string(kMaxSharedFileBytesForTest / 2, '*')));
  ShareOperation operation{"", "", GURL(), std::move(files), web_contents()};
  operation.Run(
      base::BindLambdaForTesting([&run_loop](blink::mojom::ShareError error) {
        ASSERT_EQ(error, blink::mojom::ShareError::OK);
        run_loop.Quit();
      }));
  run_loop.Run();

  ASSERT_TRUE(post_data_requested_callback_invoked);
  ASSERT_TRUE(shared_file_1);
  ASSERT_TRUE(shared_file_2);
  std::string file1_contents;
  ASSERT_NO_FATAL_FAILURE(ReadFile(shared_file_1.Get(), file1_contents));
  std::string file2_contents;
  ASSERT_NO_FATAL_FAILURE(ReadFile(shared_file_2.Get(), file2_contents));
  ASSERT_EQ(file1_contents.length() + file2_contents.length(),
            kMaxSharedFileBytesForTest);
}

TEST_F(ShareOperationUnitTest, FilesTotallingLargerThanSizeLimit) {
  bool post_data_requested_callback_invoked = false;
  ComPtr<IStorageFile> shared_file_1;
  ComPtr<IStorageFile> shared_file_2;
  fake_data_transfer_manager()->SetPostDataRequestedCallback(
      base::BindLambdaForTesting(
          [&](const FakeDataTransferManager::DataRequestedContent&
                  data_requested_content) {
            ASSERT_FALSE(post_data_requested_callback_invoked);
            post_data_requested_callback_invoked = true;
            ASSERT_EQ(data_requested_content.files.size(), 2ull);
            shared_file_1 = data_requested_content.files[0].file;
            shared_file_2 = data_requested_content.files[1].file;
          }));

  base::RunLoop run_loop;
  std::vector<blink::mojom::SharedFilePtr> files;
  files.push_back(
      CreateSharedFile(FILE_PATH_LITERAL("File1.txt"), "text/plain",
                       std::string(kMaxSharedFileBytesForTest / 2, '*')));
  files.push_back(
      CreateSharedFile(FILE_PATH_LITERAL("File2.txt"), "text/plain",
                       std::string((kMaxSharedFileBytesForTest / 2) + 1, '*')));
  ShareOperation operation{"", "", GURL(), std::move(files), web_contents()};
  operation.Run(
      base::BindLambdaForTesting([&run_loop](blink::mojom::ShareError error) {
        ASSERT_EQ(error, blink::mojom::ShareError::OK);
        run_loop.Quit();
      }));
  run_loop.Run();

  ASSERT_TRUE(post_data_requested_callback_invoked);
  ASSERT_TRUE(shared_file_1);
  ASSERT_TRUE(shared_file_2);
  std::string file1_contents;
  ASSERT_NO_FATAL_FAILURE(ReadFile(shared_file_1.Get(), file1_contents));
  std::string file2_contents;
  ASSERT_NO_FATAL_FAILURE(ReadFile(shared_file_2.Get(), file2_contents));
  ASSERT_LT(file1_contents.length() + file2_contents.length(),
            kMaxSharedFileBytesForTest + 1);
}

}  // namespace webshare
