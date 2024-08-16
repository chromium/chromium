// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/webshare/win/fake_random_access_stream.h"

#include <wrl/event.h>
#include <wrl/implements.h>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/browser/webshare/win/fake_buffer.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"

using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Foundation::IAsyncOperationCompletedHandler;
using ABI::Windows::Foundation::IAsyncOperationWithProgress;
using ABI::Windows::Foundation::IAsyncOperationWithProgressCompletedHandler;
using ABI::Windows::Foundation::IClosable;
using ABI::Windows::Storage::Streams::IBuffer;
using ABI::Windows::Storage::Streams::IInputStream;
using ABI::Windows::Storage::Streams::InputStreamOptions;
using ABI::Windows::Storage::Streams::IOutputStream;
using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Make;

namespace webshare {

TEST(FakeRandomAccessStreamTest, InvalidSeek) {
  auto stream = Make<FakeRandomAccessStream>();
  ASSERT_HRESULT_SUCCEEDED(stream->Seek(0));
  EXPECT_NONFATAL_FAILURE(ASSERT_HRESULT_FAILED(stream->Seek(1)), "Seek");
  ASSERT_HRESULT_SUCCEEDED(stream->Close());
}

TEST(FakeRandomAccessStreamTest, UsageAfterClose) {
  base::test::SingleThreadTaskEnvironment task_environment;

  auto stream = Make<FakeRandomAccessStream>();
  ASSERT_HRESULT_SUCCEEDED(stream->Close());

  // IRandomAccessStream APIs
  UINT64 size;
  EXPECT_NONFATAL_FAILURE(ASSERT_HRESULT_FAILED(stream->get_Size(&size)),
                          "closed");
  EXPECT_NONFATAL_FAILURE(ASSERT_HRESULT_FAILED(stream->put_Size(7)), "closed");
  ComPtr<IInputStream> input_stream;
  EXPECT_NONFATAL_FAILURE(
      ASSERT_HRESULT_FAILED(stream->GetInputStreamAt(0, &input_stream)),
      "closed");
  ComPtr<IOutputStream> output_stream;
  EXPECT_NONFATAL_FAILURE(
      ASSERT_HRESULT_FAILED(stream->GetOutputStreamAt(0, &output_stream)),
      "closed");
  UINT64 position;
  EXPECT_NONFATAL_FAILURE(
      ASSERT_HRESULT_FAILED(stream->get_Position(&position)), "closed");
  EXPECT_NONFATAL_FAILURE(ASSERT_HRESULT_FAILED(stream->Seek(0)), "closed");
  boolean can_read;
  EXPECT_NONFATAL_FAILURE(ASSERT_HRESULT_FAILED(stream->get_CanRead(&can_read)),
                          "closed");
  boolean can_write;
  EXPECT_NONFATAL_FAILURE(
      ASSERT_HRESULT_FAILED(stream->get_CanRead(&can_write)), "closed");

  // IClosable APIs
  EXPECT_NONFATAL_FAILURE(ASSERT_HRESULT_FAILED(stream->Close()), "closed");

  // IInputStream APIs
  auto buffer = Make<FakeBuffer>(4);
  ComPtr<IAsyncOperationWithProgress<IBuffer*, UINT32>> read_operation;
  EXPECT_NONFATAL_FAILURE(
      ASSERT_HRESULT_FAILED(stream->ReadAsync(
          buffer.Get(), 4, InputStreamOptions::InputStreamOptions_None,
          &read_operation)),
      "closed");

  // IOutputStream APIs
  ComPtr<IAsyncOperationWithProgress<UINT32, UINT32>> write_operation;
  EXPECT_NONFATAL_FAILURE(
      ASSERT_HRESULT_FAILED(stream->WriteAsync(buffer.Get(), &write_operation)),
      "closed");
  ComPtr<IAsyncOperation<bool>> flush_operation;
  EXPECT_NONFATAL_FAILURE(
      ASSERT_HRESULT_FAILED(stream->FlushAsync(&flush_operation)), "closed");

  // Test APIs
  // EXPECT_FATAL_FAILURE() can only reference globals and statics.
  static ComPtr<FakeRandomAccessStream>& static_stream = stream;
  EXPECT_FATAL_FAILURE(static_stream->OnClose(base::DoNothing()), "closed");
}

TEST(FakeRandomAccessStreamTest, CompetingAsyncCalls) {
  base::test::SingleThreadTaskEnvironment task_environment;

  auto stream = Make<FakeRandomAccessStream>();

  ComPtr<IAsyncOperation<bool>> flush_operation;
  ComPtr<IAsyncOperationWithProgress<IBuffer*, UINT32>> read_operation;
  ComPtr<IAsyncOperationWithProgress<UINT32, UINT32>> write_operation;
  auto buffer = Make<FakeBuffer>(4);

  {
    base::RunLoop run_loop;
    ASSERT_HRESULT_SUCCEEDED(
        stream->WriteAsync(buffer.Get(), &write_operation));
    ASSERT_HRESULT_SUCCEEDED(write_operation->put_Completed(
        Callback<IAsyncOperationWithProgressCompletedHandler<UINT32, UINT32>>(
            [&run_loop](
                IAsyncOperationWithProgress<UINT32, UINT32>* async_operation,
                AsyncStatus async_status) {
              run_loop.Quit();
              return S_OK;
            })
            .Get()));

    EXPECT_NONFATAL_FAILURE(
        ASSERT_HRESULT_FAILED(stream->FlushAsync(&flush_operation)),
        "in progress");
    EXPECT_NONFATAL_FAILURE(
        ASSERT_HRESULT_FAILED(stream->ReadAsync(
            buffer.Get(), 4, InputStreamOptions::InputStreamOptions_None,
            &read_operation)),
        "in progress");
    EXPECT_NONFATAL_FAILURE(ASSERT_HRESULT_FAILED(stream->WriteAsync(
                                buffer.Get(), &write_operation)),
                            "in progress");
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    ASSERT_HRESULT_SUCCEEDED(stream->ReadAsync(
        buffer.Get(), 4, InputStreamOptions::InputStreamOptions_None,
        &read_operation));
    ASSERT_HRESULT_SUCCEEDED(read_operation->put_Completed(
        Callback<IAsyncOperationWithProgressCompletedHandler<IBuffer*, UINT32>>(
            [&run_loop](
                IAsyncOperationWithProgress<IBuffer*, UINT32>* async_operation,
                AsyncStatus async_status) {
              run_loop.Quit();
              return S_OK;
            })
            .Get()));

    EXPECT_NONFATAL_FAILURE(
        ASSERT_HRESULT_FAILED(stream->FlushAsync(&flush_operation)),
        "in progress");
    EXPECT_NONFATAL_FAILURE(
        ASSERT_HRESULT_FAILED(stream->ReadAsync(
            buffer.Get(), 4, InputStreamOptions::InputStreamOptions_None,
            &read_operation)),
        "in progress");
    EXPECT_NONFATAL_FAILURE(ASSERT_HRESULT_FAILED(stream->WriteAsync(
                                buffer.Get(), &write_operation)),
                            "in progress");
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    ASSERT_HRESULT_SUCCEEDED(stream->FlushAsync(&flush_operation));
    ASSERT_HRESULT_SUCCEEDED(flush_operation->put_Completed(
        Callback<IAsyncOperationCompletedHandler<bool>>(
            [&run_loop](IAsyncOperation<bool>* async_operation,
                        AsyncStatus async_status) {
              run_loop.Quit();
              return S_OK;
            })
            .Get()));

    EXPECT_NONFATAL_FAILURE(
        ASSERT_HRESULT_FAILED(stream->FlushAsync(&flush_operation)),
        "in progress");
    EXPECT_NONFATAL_FAILURE(
        ASSERT_HRESULT_FAILED(stream->ReadAsync(
            buffer.Get(), 4, InputStreamOptions::InputStreamOptions_None,
            &read_operation)),
        "in progress");
    EXPECT_NONFATAL_FAILURE(ASSERT_HRESULT_FAILED(stream->WriteAsync(
                                buffer.Get(), &write_operation)),
                            "in progress");
    run_loop.Run();
  }
  ASSERT_HRESULT_SUCCEEDED(stream->Close());
}

TEST(FakeRandomAccessStreamTest, BasicReadWrite) {
  base::test::SingleThreadTaskEnvironment task_environment;

  // Initialize an input and output stream based on the same stream
  ComPtr<IInputStream> input_stream;
  ComPtr<IOutputStream> output_stream;
  {
    auto stream = Make<FakeRandomAccessStream>();
    UINT64 position;
    ASSERT_HRESULT_SUCCEEDED(stream->get_Position(&position));
    ASSERT_EQ(position, 0u);

    ASSERT_HRESULT_SUCCEEDED(stream->GetInputStreamAt(0, &input_stream));
    ASSERT_HRESULT_SUCCEEDED(stream->GetOutputStreamAt(0, &output_stream));
    ASSERT_HRESULT_SUCCEEDED(stream->Close());
  }

  // Create a filled buffer that reads "abcd"
  auto buffer = Make<FakeBuffer>(4);
  ASSERT_HRESULT_SUCCEEDED(buffer->put_Length(4));
  byte* raw_buffer;
  ASSERT_HRESULT_SUCCEEDED(buffer->Buffer(&raw_buffer));
  raw_buffer[0] = 'a';
  raw_buffer[1] = 'b';
  raw_buffer[2] = 'c';
  raw_buffer[3] = 'd';

  // Write the buffer to the output stream
  {
    base::RunLoop run_loop;
    ComPtr<IAsyncOperationWithProgress<UINT32, UINT32>> write_operation;
    ASSERT_HRESULT_SUCCEEDED(
        output_stream->WriteAsync(buffer.Get(), &write_operation));
    ASSERT_HRESULT_SUCCEEDED(write_operation->put_Completed(
        Callback<IAsyncOperationWithProgressCompletedHandler<UINT32, UINT32>>(
            [&run_loop](
                IAsyncOperationWithProgress<UINT32, UINT32>* async_operation,
                AsyncStatus async_status) {
              EXPECT_EQ(async_status, AsyncStatus::Completed);
              UINT32 results;
              EXPECT_HRESULT_SUCCEEDED(async_operation->GetResults(&results));
              EXPECT_EQ(results, 4u);
              run_loop.Quit();
              return S_OK;
            })
            .Get()));
    run_loop.Run();
  }

  // Update the same buffer to now read "ef"
  raw_buffer[0] = 'e';
  raw_buffer[1] = 'f';
  ASSERT_HRESULT_SUCCEEDED(buffer->put_Length(2));

  // Write the buffer to the output stream
  {
    base::RunLoop run_loop;
    ComPtr<IAsyncOperationWithProgress<UINT32, UINT32>> write_operation;
    ASSERT_HRESULT_SUCCEEDED(
        output_stream->WriteAsync(buffer.Get(), &write_operation));
    ASSERT_HRESULT_SUCCEEDED(write_operation->put_Completed(
        Callback<IAsyncOperationWithProgressCompletedHandler<UINT32, UINT32>>(
            [&run_loop](
                IAsyncOperationWithProgress<UINT32, UINT32>* async_operation,
                AsyncStatus async_status) {
              EXPECT_EQ(async_status, AsyncStatus::Completed);
              UINT32 results;
              EXPECT_HRESULT_SUCCEEDED(async_operation->GetResults(&results));
              EXPECT_EQ(results, 2u);
              run_loop.Quit();
              return S_OK;
            })
            .Get()));
    run_loop.Run();
  }

  // Flush the output stream
  {
    base::RunLoop run_loop;
    ComPtr<IAsyncOperation<bool>> flush_operation;
    ASSERT_HRESULT_SUCCEEDED(output_stream->FlushAsync(&flush_operation));
    ASSERT_HRESULT_SUCCEEDED(flush_operation->put_Completed(
        Callback<IAsyncOperationCompletedHandler<bool>>(
            [&run_loop](IAsyncOperation<bool>* async_operation,
                        AsyncStatus async_status) {
              EXPECT_EQ(async_status, AsyncStatus::Completed);
              boolean results;
              EXPECT_HRESULT_SUCCEEDED(async_operation->GetResults(&results));
              EXPECT_EQ(results, TRUE);
              run_loop.Quit();
              return S_OK;
            })
            .Get()));
    run_loop.Run();
    ComPtr<IClosable> closable_stream;
    ASSERT_HRESULT_SUCCEEDED(output_stream.As(&closable_stream));
    ASSERT_HRESULT_SUCCEEDED(closable_stream->Close());
  }

  // Read the input stream to the buffer
  {
    base::RunLoop run_loop;
    ComPtr<IAsyncOperationWithProgress<IBuffer*, UINT32>> read_operation;
    ASSERT_HRESULT_SUCCEEDED(input_stream->ReadAsync(
        buffer.Get(), 4, InputStreamOptions::InputStreamOptions_None,
        &read_operation));
    ASSERT_HRESULT_SUCCEEDED(read_operation->put_Completed(
        Callback<IAsyncOperationWithProgressCompletedHandler<IBuffer*, UINT32>>(
            [&run_loop, &buffer](
                IAsyncOperationWithProgress<IBuffer*, UINT32>* async_operation,
                AsyncStatus async_status) {
              EXPECT_EQ(async_status, AsyncStatus::Completed);
              ComPtr<IBuffer> results;
              EXPECT_HRESULT_SUCCEEDED(async_operation->GetResults(&results));
              EXPECT_EQ(results, buffer);
              run_loop.Quit();
              return S_OK;
            })
            .Get()));
    run_loop.Run();
  }

  UINT32 length;
  ASSERT_HRESULT_SUCCEEDED(buffer->get_Length(&length));
  ASSERT_EQ(length, 4u);
  ASSERT_EQ(raw_buffer[0], 'a');
  ASSERT_EQ(raw_buffer[1], 'b');
  ASSERT_EQ(raw_buffer[2], 'c');
  ASSERT_EQ(raw_buffer[3], 'd');

  // Read the remaining input stream to the buffer
  {
    base::RunLoop run_loop;
    ComPtr<IAsyncOperationWithProgress<IBuffer*, UINT32>> read_operation;
    ASSERT_HRESULT_SUCCEEDED(input_stream->ReadAsync(
        buffer.Get(), 2, InputStreamOptions::InputStreamOptions_None,
        &read_operation));
    ASSERT_HRESULT_SUCCEEDED(read_operation->put_Completed(
        Callback<IAsyncOperationWithProgressCompletedHandler<IBuffer*, UINT32>>(
            [&run_loop, &buffer](
                IAsyncOperationWithProgress<IBuffer*, UINT32>* async_operation,
                AsyncStatus async_status) {
              EXPECT_EQ(async_status, AsyncStatus::Completed);
              ComPtr<IBuffer> results;
              EXPECT_HRESULT_SUCCEEDED(async_operation->GetResults(&results));
              EXPECT_EQ(results, buffer);
              run_loop.Quit();
              return S_OK;
            })
            .Get()));
    run_loop.Run();
    ComPtr<IClosable> closable_stream;
    ASSERT_HRESULT_SUCCEEDED(input_stream.As(&closable_stream));
    ASSERT_HRESULT_SUCCEEDED(closable_stream->Close());
  }

  ASSERT_HRESULT_SUCCEEDED(buffer->get_Length(&length));
  ASSERT_EQ(length, 2u);
  ASSERT_EQ(raw_buffer[0], 'e');
  ASSERT_EQ(raw_buffer[1], 'f');
}

}  // namespace webshare
