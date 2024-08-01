// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/webshare/win/fake_data_writer_factory.h"

#include <wrl/event.h>
#include <wrl/implements.h>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/browser/webshare/win/fake_buffer.h"
#include "chrome/browser/webshare/win/fake_random_access_stream.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"

using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Foundation::IAsyncOperationCompletedHandler;
using ABI::Windows::Foundation::IAsyncOperationWithProgress;
using ABI::Windows::Foundation::IAsyncOperationWithProgressCompletedHandler;
using ABI::Windows::Storage::Streams::IBuffer;
using ABI::Windows::Storage::Streams::IDataReader;
using ABI::Windows::Storage::Streams::IDataWriter;
using ABI::Windows::Storage::Streams::InputStreamOptions;
using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Make;

namespace webshare {

TEST(FakeDataWriterFactoryTest, RacingStoreAsync) {
  base::test::SingleThreadTaskEnvironment task_environment;

  auto writer_factory = Make<FakeDataWriterFactory>();
  auto stream = Make<FakeRandomAccessStream>();

  ComPtr<IDataWriter> data_writer;
  ASSERT_HRESULT_SUCCEEDED(
      writer_factory->CreateDataWriter(stream.Get(), &data_writer));

  std::vector<unsigned char> bytes = {'h', 'i'};
  ASSERT_HRESULT_SUCCEEDED(data_writer->WriteBytes(bytes.size(), bytes.data()));

  base::RunLoop run_loop;
  ComPtr<IAsyncOperation<UINT32>> store_operation;
  ASSERT_HRESULT_SUCCEEDED(data_writer->StoreAsync(&store_operation));

  EXPECT_NONFATAL_FAILURE(ASSERT_HRESULT_FAILED(data_writer->WriteBytes(
                              bytes.size(), bytes.data())),
                          "WriteBytes");

  ComPtr<IAsyncOperation<UINT32>> store_operation_2;
  EXPECT_NONFATAL_FAILURE(
      ASSERT_HRESULT_FAILED(data_writer->StoreAsync(&store_operation_2)),
      "StoreAsync");

  ComPtr<IAsyncOperation<bool>> flush_operation;
  EXPECT_NONFATAL_FAILURE(
      ASSERT_HRESULT_FAILED(data_writer->FlushAsync(&flush_operation)),
      "FlushAsync");

  store_operation->put_Completed(
      Callback<IAsyncOperationCompletedHandler<UINT32>>(
          [&run_loop](IAsyncOperation<UINT32>* async_operation,
                      AsyncStatus async_status) {
            run_loop.Quit();
            return S_OK;
          })
          .Get());
  run_loop.Run();

  // Expect a failure from the destructor due to the unflushed data
  EXPECT_NONFATAL_FAILURE(data_writer.Reset(), "FlushAsync");

  // Cleanup
  ASSERT_HRESULT_SUCCEEDED(stream->Close());
}

TEST(FakeDataWriterFactoryTest, UnstoredBytes) {
  auto writer_factory = Make<FakeDataWriterFactory>();
  auto stream = Make<FakeRandomAccessStream>();

  ComPtr<IDataWriter> data_writer;
  ASSERT_HRESULT_SUCCEEDED(
      writer_factory->CreateDataWriter(stream.Get(), &data_writer));

  std::vector<unsigned char> bytes = {'h', 'i'};
  ASSERT_HRESULT_SUCCEEDED(data_writer->WriteBytes(bytes.size(), bytes.data()));

  ComPtr<IAsyncOperation<bool>> flush_operation;
  EXPECT_NONFATAL_FAILURE(
      ASSERT_HRESULT_FAILED(data_writer->FlushAsync(&flush_operation)),
      "FlushAsync");

  {
    ::testing::TestPartResultArray gtest_failures;
    ::testing::ScopedFakeTestPartResultReporter gtest_reporter(
        ::testing::ScopedFakeTestPartResultReporter::
            INTERCEPT_ONLY_CURRENT_THREAD,
        &gtest_failures);
    // Expect failures from the destructor from not storing the data and not
    // flushing the data
    data_writer.Reset();
    EXPECT_EQ(gtest_failures.size(), 2) << "pending storage";
  }

  // Cleanup
  ASSERT_HRESULT_SUCCEEDED(stream->Close());
}

TEST(FakeDataWriterFactoryTest, WriteBytes) {
  base::test::SingleThreadTaskEnvironment task_environment;

  auto writer_factory = Make<FakeDataWriterFactory>();
  auto stream = Make<FakeRandomAccessStream>();

  ComPtr<IDataWriter> data_writer;
  ASSERT_HRESULT_SUCCEEDED(
      writer_factory->CreateDataWriter(stream.Get(), &data_writer));

  {
    std::vector<unsigned char> bytes = {'h', 'i'};
    ASSERT_HRESULT_SUCCEEDED(
        data_writer->WriteBytes(bytes.size(), bytes.data()));
    base::RunLoop run_loop;
    ComPtr<IAsyncOperation<UINT32>> store_operation;
    ASSERT_HRESULT_SUCCEEDED(data_writer->StoreAsync(&store_operation));
    store_operation->put_Completed(
        Callback<IAsyncOperationCompletedHandler<UINT32>>(
            [&run_loop, &bytes](IAsyncOperation<UINT32>* async_operation,
                                AsyncStatus async_status) {
              EXPECT_EQ(async_status, AsyncStatus::Completed);
              UINT32 results;
              EXPECT_HRESULT_SUCCEEDED(async_operation->GetResults(&results));
              EXPECT_EQ(results, bytes.size());
              run_loop.Quit();
              return S_OK;
            })
            .Get());
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    std::vector<unsigned char> more_bytes = {' ', 't', 'h', 'e', 'r', 'e'};
    ASSERT_HRESULT_SUCCEEDED(
        data_writer->WriteBytes(more_bytes.size(), more_bytes.data()));
    ComPtr<IAsyncOperation<UINT32>> store_operation;
    ASSERT_HRESULT_SUCCEEDED(data_writer->StoreAsync(&store_operation));
    store_operation->put_Completed(
        Callback<IAsyncOperationCompletedHandler<UINT32>>(
            [&run_loop, &more_bytes](IAsyncOperation<UINT32>* async_operation,
                                     AsyncStatus async_status) {
              EXPECT_EQ(async_status, AsyncStatus::Completed);
              UINT32 results;
              EXPECT_HRESULT_SUCCEEDED(async_operation->GetResults(&results));
              EXPECT_EQ(results, more_bytes.size());
              run_loop.Quit();
              return S_OK;
            })
            .Get());
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    ComPtr<IAsyncOperation<bool>> flush_operation;
    ASSERT_HRESULT_SUCCEEDED(data_writer->FlushAsync(&flush_operation));
    flush_operation->put_Completed(
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
            .Get());
    run_loop.Run();
  }

  ASSERT_HRESULT_SUCCEEDED(stream->Seek(0));
  UINT64 size;
  stream->get_Size(&size);
  ASSERT_EQ(size, 8u);
  auto buffer = Make<FakeBuffer>(size);

  {
    base::RunLoop run_loop;
    ComPtr<IAsyncOperationWithProgress<IBuffer*, UINT32>> read_operation;
    ASSERT_HRESULT_SUCCEEDED(stream->ReadAsync(
        buffer.Get(), size, InputStreamOptions::InputStreamOptions_None,
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
  ASSERT_EQ(length, 8u);
  byte* raw_buffer;
  ASSERT_HRESULT_SUCCEEDED(buffer->Buffer(&raw_buffer));
  ASSERT_EQ(raw_buffer[0], 'h');
  ASSERT_EQ(raw_buffer[1], 'i');
  ASSERT_EQ(raw_buffer[2], ' ');
  ASSERT_EQ(raw_buffer[3], 't');
  ASSERT_EQ(raw_buffer[4], 'h');
  ASSERT_EQ(raw_buffer[5], 'e');
  ASSERT_EQ(raw_buffer[6], 'r');
  ASSERT_EQ(raw_buffer[7], 'e');

  // Cleanup
  ASSERT_HRESULT_SUCCEEDED(stream->Close());
}

}  // namespace webshare
