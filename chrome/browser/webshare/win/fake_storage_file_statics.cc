// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/win/fake_storage_file_statics.h"

#include <windows.foundation.h>
#include <windows.storage.streams.h>
#include <wrl/module.h>

#include <memory>
#include <string>
#include <tuple>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/fake_iasync_operation_win.h"
#include "base/win/scoped_hstring.h"
#include "chrome/browser/webshare/win/fake_random_access_stream.h"
#include "testing/gtest/include/gtest/gtest.h"

using ABI::Windows::Foundation::DateTime;
using ABI::Windows::Foundation::IAsyncAction;
using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Foundation::IUriRuntimeClass;
using ABI::Windows::Storage::FileAccessMode;
using ABI::Windows::Storage::FileAttributes;
using ABI::Windows::Storage::IStorageFile;
using ABI::Windows::Storage::IStorageFolder;
using ABI::Windows::Storage::IStorageItem;
using ABI::Windows::Storage::IStreamedFileDataRequestedHandler;
using ABI::Windows::Storage::NameCollisionOption;
using ABI::Windows::Storage::StorageDeleteOption;
using ABI::Windows::Storage::StorageFile;
using ABI::Windows::Storage::StorageItemTypes;
using ABI::Windows::Storage::StorageStreamTransaction;
using ABI::Windows::Storage::FileProperties::BasicProperties;
using ABI::Windows::Storage::Streams::IOutputStream;
using ABI::Windows::Storage::Streams::IRandomAccessStream;
using ABI::Windows::Storage::Streams::IRandomAccessStreamReference;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Make;
using Microsoft::WRL::RuntimeClass;
using Microsoft::WRL::RuntimeClassFlags;
using Microsoft::WRL::WinRtClassicComMix;

namespace webshare {
namespace {

class FakeStorageFile final
    : public RuntimeClass<RuntimeClassFlags<WinRtClassicComMix>,
                          IStorageFile,
                          IStorageItem> {
 public:
  FakeStorageFile(HSTRING display_name_with_extension,
                  IStreamedFileDataRequestedHandler* data_requested,
                  IRandomAccessStreamReference* thumbnail)
      : streamed_file_data_requested_handler_(data_requested) {
    // ScopedHString takes ownership of the HSTRING provided to it, but taking
    // ownership is not an expected behavior when passing an HSTRING to a system
    // API, so we use a temporary ScopedHString to make a copy we can safely own
    // and release ownership of the original 'back' to the caller.
    base::win::ScopedHString holder(display_name_with_extension);
    display_name_with_extension_ = holder.GetAsUTF8();
    std::ignore = holder.release();
  }
  FakeStorageFile(const FakeStorageFile&) = delete;
  FakeStorageFile& operator=(const FakeStorageFile&) = delete;
  ~FakeStorageFile() final {
    EXPECT_FALSE(open_async_in_progress_)
        << "FakeStorageFile destroyed while open operation is in progress.";
  }

  // ABI::Windows::Storage::IStorageFile
  IFACEMETHODIMP get_FileType(HSTRING* value) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_ContentType(HSTRING* value) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP OpenAsync(
      FileAccessMode access_mode,
      IAsyncOperation<IRandomAccessStream*>** operation) final {
    if (open_async_in_progress_) {
      ADD_FAILURE()
          << "OpenAsync called while an open operation is in progress.";
      return E_ILLEGAL_METHOD_CALL;
    }

    auto fake_iasync_operation =
        Make<base::win::FakeIAsyncOperation<IRandomAccessStream*>>();
    HRESULT hr = fake_iasync_operation->QueryInterface(IID_PPV_ARGS(operation));
    if (FAILED(hr)) {
      EXPECT_HRESULT_SUCCEEDED(hr);
      return hr;
    }

    bool success = base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&FakeStorageFile::OnOpenAsync,
                       weak_factory_.GetWeakPtr(), fake_iasync_operation));
    if (!success) {
      EXPECT_TRUE(success);
      return E_ASYNC_OPERATION_NOT_STARTED;
    }

    open_async_in_progress_ = true;
    return S_OK;
  }
  IFACEMETHODIMP OpenTransactedWriteAsync(
      IAsyncOperation<StorageStreamTransaction*>** operation) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP CopyOverloadDefaultNameAndOptions(
      IStorageFolder* destination_folder,
      IAsyncOperation<StorageFile*>** operation) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP CopyOverloadDefaultOptions(
      IStorageFolder* destination_folder,
      HSTRING desired_new_name,
      IAsyncOperation<StorageFile*>** operation) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP CopyOverload(IStorageFolder* destination_folder,
                              HSTRING desired_new_name,
                              NameCollisionOption option,
                              IAsyncOperation<StorageFile*>** operation) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP
  CopyAndReplaceAsync(IStorageFile* file_to_replace,
                      IAsyncAction** operation) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP MoveOverloadDefaultNameAndOptions(
      IStorageFolder* destination_folder,
      IAsyncAction** operation) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP MoveOverloadDefaultOptions(IStorageFolder* destination_folder,
                                            HSTRING desired_new_name,
                                            IAsyncAction** operation) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP
  MoveOverload(IStorageFolder* destination_folder,
               HSTRING desired_new_name,
               NameCollisionOption option,
               IAsyncAction** operation) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP
  MoveAndReplaceAsync(IStorageFile* file_to_replace,
                      IAsyncAction** operation) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }

  // ABI::Windows::Storage::IStorageItem
  IFACEMETHODIMP RenameAsyncOverloadDefaultOptions(
      HSTRING desired_name,
      IAsyncAction** operation) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP
  RenameAsync(HSTRING desired_name,
              NameCollisionOption option,
              IAsyncAction** operation) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP DeleteAsyncOverloadDefaultOptions(
      IAsyncAction** operation) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP
  DeleteAsync(StorageDeleteOption option, IAsyncAction** operation) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP GetBasicPropertiesAsync(
      IAsyncOperation<BasicProperties*>** operation) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_Name(HSTRING* value) final {
    auto copy = base::win::ScopedHString::Create(display_name_with_extension_);
    *value = copy.release();
    return S_OK;
  }
  IFACEMETHODIMP get_Path(HSTRING* value) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP
  get_Attributes(FileAttributes* value) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP
  get_DateCreated(DateTime* value) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP
  IsOfType(StorageItemTypes type, boolean* value) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }

 private:
  void OnOpenAsync(ComPtr<base::win::FakeIAsyncOperation<IRandomAccessStream*>>
                       fake_iasync_operation) {
    ASSERT_TRUE(open_async_in_progress_);
    open_async_in_progress_ = false;

    auto fake_stream = Make<FakeRandomAccessStream>();

    ComPtr<IOutputStream> output_stream;
    ASSERT_HRESULT_SUCCEEDED(fake_stream->GetOutputStreamAt(0, &output_stream));
    ComPtr<FakeRandomAccessStream> fake_output_stream =
        static_cast<FakeRandomAccessStream*>(output_stream.Get());
    ASSERT_TRUE(fake_output_stream);

    fake_output_stream->OnClose(
        base::BindLambdaForTesting([fake_stream, fake_iasync_operation]() {
          ComPtr<IRandomAccessStream> random_access_stream;
          ASSERT_HRESULT_SUCCEEDED(fake_stream.As(&random_access_stream));
          fake_iasync_operation->CompleteWithResults(random_access_stream);
        }));

    ASSERT_HRESULT_SUCCEEDED(
        streamed_file_data_requested_handler_->Invoke(output_stream.Get()));
  }

  std::string display_name_with_extension_;
  bool open_async_in_progress_ = false;
  ComPtr<IStreamedFileDataRequestedHandler>
      streamed_file_data_requested_handler_;
  base::WeakPtrFactory<FakeStorageFile> weak_factory_{this};
};

}  // namespace

FakeStorageFileStatics::FakeStorageFileStatics() = default;
FakeStorageFileStatics::~FakeStorageFileStatics() = default;

IFACEMETHODIMP FakeStorageFileStatics::GetFileFromPathAsync(
    HSTRING path,
    IAsyncOperation<StorageFile*>** operation) {
  NOTREACHED_IN_MIGRATION();
  return E_NOTIMPL;
}

IFACEMETHODIMP FakeStorageFileStatics::GetFileFromApplicationUriAsync(
    IUriRuntimeClass* uri,
    IAsyncOperation<StorageFile*>** operation) {
  NOTREACHED_IN_MIGRATION();
  return E_NOTIMPL;
}

IFACEMETHODIMP FakeStorageFileStatics::CreateStreamedFileAsync(
    HSTRING display_name_with_extension,
    IStreamedFileDataRequestedHandler* data_requested,
    IRandomAccessStreamReference* thumbnail,
    IAsyncOperation<StorageFile*>** operation) {
  auto fake_iasync_operation =
      Make<base::win::FakeIAsyncOperation<StorageFile*>>();
  HRESULT hr = fake_iasync_operation->QueryInterface(IID_PPV_ARGS(operation));
  if (FAILED(hr)) {
    EXPECT_HRESULT_SUCCEEDED(hr);
    return hr;
  }

  auto fake_storage_file = Make<FakeStorageFile>(display_name_with_extension,
                                                 data_requested, thumbnail);
  bool success = base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindLambdaForTesting([fake_iasync_operation, fake_storage_file]() {
        fake_iasync_operation->CompleteWithResults(fake_storage_file);
      }));
  if (!success) {
    EXPECT_TRUE(success);
    return E_ASYNC_OPERATION_NOT_STARTED;
  }

  return S_OK;
}

IFACEMETHODIMP FakeStorageFileStatics::ReplaceWithStreamedFileAsync(
    IStorageFile* file_to_replace,
    IStreamedFileDataRequestedHandler* data_requested,
    IRandomAccessStreamReference* thumbnail,
    IAsyncOperation<StorageFile*>** operation) {
  NOTREACHED_IN_MIGRATION();
  return E_NOTIMPL;
}

IFACEMETHODIMP FakeStorageFileStatics::CreateStreamedFileFromUriAsync(
    HSTRING display_name_with_extension,
    IUriRuntimeClass* uri,
    IRandomAccessStreamReference* thumbnail,
    IAsyncOperation<StorageFile*>** operation) {
  NOTREACHED_IN_MIGRATION();
  return E_NOTIMPL;
}

IFACEMETHODIMP FakeStorageFileStatics::ReplaceWithStreamedFileFromUriAsync(
    IStorageFile* file_to_replace,
    IUriRuntimeClass* uri,
    IRandomAccessStreamReference* thumbnail,
    IAsyncOperation<StorageFile*>** operation) {
  NOTREACHED_IN_MIGRATION();
  return E_NOTIMPL;
}

}  // namespace webshare
