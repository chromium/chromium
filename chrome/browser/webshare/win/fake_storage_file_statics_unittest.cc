// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/webshare/win/fake_storage_file_statics.h"

#include <wrl/event.h>
#include <wrl/implements.h>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/win/scoped_hstring.h"
#include "chrome/browser/webshare/win/fake_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Foundation::IAsyncOperationCompletedHandler;
using ABI::Windows::Foundation::IAsyncOperationWithProgress;
using ABI::Windows::Foundation::IAsyncOperationWithProgressCompletedHandler;
using ABI::Windows::Foundation::IClosable;
using ABI::Windows::Storage::FileAccessMode;
using ABI::Windows::Storage::IStorageFile;
using ABI::Windows::Storage::IStorageItem;
using ABI::Windows::Storage::IStreamedFileDataRequestedHandler;
using ABI::Windows::Storage::StorageFile;
using ABI::Windows::Storage::Streams::IBuffer;
using ABI::Windows::Storage::Streams::IInputStream;
using ABI::Windows::Storage::Streams::InputStreamOptions;
using ABI::Windows::Storage::Streams::IOutputStream;
using ABI::Windows::Storage::Streams::IRandomAccessStream;
using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Make;

namespace webshare {

TEST(FakeStorageFileStaticsTest, CreateStreamedFileAsync) {
  base::test::SingleThreadTaskEnvironment task_environment;
  auto file_statics = Make<FakeStorageFileStatics>();

  // Create a streamed file, populated on-demand by the provided callback.
  ComPtr<IStorageFile> storage_file;
  {
    base::RunLoop run_loop;
    auto file_name = base::win::ScopedHString::Create("MyTestFile.abc");
    ComPtr<IAsyncOperation<StorageFile*>> create_operation;
    file_statics->CreateStreamedFileAsync(
        file_name.get(),
        Callback<IStreamedFileDataRequestedHandler>([](IOutputStream* stream) {
          // Create a small buffer of bytes to write to the stream
          auto buffer = Make<FakeBuffer>(4);
          EXPECT_HRESULT_SUCCEEDED(buffer->put_Length(4));
          byte* raw_buffer;
          EXPECT_HRESULT_SUCCEEDED(buffer->Buffer(&raw_buffer));
          raw_buffer[0] = 'f';
          raw_buffer[1] = 'i';
          raw_buffer[2] = 's';
          raw_buffer[3] = 'h';

          // Write the bytes to the stream
          ComPtr<IAsyncOperationWithProgress<UINT32, UINT32>> write_operation;
          EXPECT_HRESULT_SUCCEEDED(
              stream->WriteAsync(buffer.Get(), &write_operation));
          ComPtr<IOutputStream> captured_output_stream = stream;
          write_operation->put_Completed(
              Callback<
                  IAsyncOperationWithProgressCompletedHandler<UINT32, UINT32>>(
                  [captured_output_stream](
                      IAsyncOperationWithProgress<UINT32, UINT32>*
                          async_operation,
                      AsyncStatus async_status) {
                    // Verify the write operation completed successfully
                    EXPECT_EQ(async_status, AsyncStatus::Completed);
                    UINT32 result;
                    EXPECT_HRESULT_SUCCEEDED(
                        async_operation->GetResults(&result));
                    EXPECT_EQ(result, 4u);

                    // Flush the stream
                    ComPtr<IAsyncOperation<bool>> flush_operation;
                    EXPECT_HRESULT_SUCCEEDED(
                        captured_output_stream->FlushAsync(&flush_operation));
                    flush_operation->put_Completed(
                        Callback<IAsyncOperationCompletedHandler<bool>>(
                            [captured_output_stream](
                                IAsyncOperation<bool>* async_operation,
                                AsyncStatus async_status) {
                              // Verify the flush operation completed
                              // successfully
                              EXPECT_EQ(async_status, AsyncStatus::Completed);
                              boolean result;
                              EXPECT_HRESULT_SUCCEEDED(
                                  async_operation->GetResults(&result));
                              EXPECT_EQ(result, TRUE);

                              // Close the stream
                              ComPtr<IClosable> closable;
                              EXPECT_HRESULT_SUCCEEDED(
                                  captured_output_stream.As(&closable));
                              EXPECT_HRESULT_SUCCEEDED(closable->Close());
                              return S_OK;
                            })
                            .Get());
                    return S_OK;
                  })
                  .Get());
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

  // Verify the created streamed file has the expected name
  ComPtr<IStorageItem> storage_item;
  ASSERT_HRESULT_SUCCEEDED(storage_file.As(&storage_item));
  HSTRING name;
  ASSERT_HRESULT_SUCCEEDED(storage_item->get_Name(&name));
  ASSERT_EQ(base::win::ScopedHString(name).GetAsUTF8(), "MyTestFile.abc");

  // Open the streamed file
  ComPtr<IRandomAccessStream> opened_stream;
  {
    base::RunLoop run_loop;
    ComPtr<IAsyncOperation<IRandomAccessStream*>> open_operation;
    ASSERT_HRESULT_SUCCEEDED(storage_file->OpenAsync(
        FileAccessMode::FileAccessMode_Read, &open_operation));
    ASSERT_HRESULT_SUCCEEDED(open_operation->put_Completed(
        Callback<IAsyncOperationCompletedHandler<IRandomAccessStream*>>(
            [&run_loop, &opened_stream](
                IAsyncOperation<IRandomAccessStream*>* async_operation,
                AsyncStatus async_status) {
              EXPECT_EQ(async_status, AsyncStatus::Completed);
              EXPECT_HRESULT_SUCCEEDED(
                  async_operation->GetResults(&opened_stream));
              run_loop.Quit();
              return S_OK;
            })
            .Get()));
    run_loop.Run();
  }

  ComPtr<IInputStream> input_stream;
  ASSERT_HRESULT_SUCCEEDED(opened_stream->GetInputStreamAt(0, &input_stream));
  UINT64 size;
  ASSERT_HRESULT_SUCCEEDED(opened_stream->get_Size(&size));
  ComPtr<IClosable> closable_opened_stream;
  ASSERT_HRESULT_SUCCEEDED(opened_stream.As(&closable_opened_stream));
  ASSERT_HRESULT_SUCCEEDED(closable_opened_stream->Close());

  // Read the opened stream
  auto buffer = Make<FakeBuffer>(size);
  {
    base::RunLoop run_loop;
    ComPtr<IAsyncOperationWithProgress<IBuffer*, UINT32>> read_operation;
    ASSERT_HRESULT_SUCCEEDED(input_stream->ReadAsync(
        buffer.Get(), size, InputStreamOptions::InputStreamOptions_None,
        &read_operation));
    ASSERT_HRESULT_SUCCEEDED(read_operation->put_Completed(
        Callback<IAsyncOperationWithProgressCompletedHandler<IBuffer*, UINT32>>(
            [&run_loop, &buffer](
                IAsyncOperationWithProgress<IBuffer*, UINT32>* async_operation,
                AsyncStatus async_status) {
              EXPECT_EQ(async_status, AsyncStatus::Completed);
              ComPtr<IBuffer> result;
              EXPECT_HRESULT_SUCCEEDED(async_operation->GetResults(&result));
              EXPECT_EQ(result.Get(), buffer.Get());
              run_loop.Quit();
              return S_OK;
            })
            .Get()));
    run_loop.Run();
  }

  // Verify the stream read from the file has content matching what our
  // DataRequested callback provided
  UINT32 length;
  ASSERT_HRESULT_SUCCEEDED(buffer->get_Length(&length));
  ASSERT_EQ(length, size);
  byte* raw_buffer;
  ASSERT_HRESULT_SUCCEEDED(buffer->Buffer(&raw_buffer));
  ASSERT_EQ(raw_buffer[0], 'f');
  ASSERT_EQ(raw_buffer[1], 'i');
  ASSERT_EQ(raw_buffer[2], 's');
  ASSERT_EQ(raw_buffer[3], 'h');

  // Cleanup
  ComPtr<IClosable> closable_input_stream;
  ASSERT_HRESULT_SUCCEEDED(input_stream.As(&closable_input_stream));
  ASSERT_HRESULT_SUCCEEDED(closable_input_stream->Close());
}

}  // namespace webshare
