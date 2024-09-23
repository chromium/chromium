// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/webshare/win/fake_data_writer_factory.h"

#include <robuffer.h>
#include <windows.foundation.h>
#include <wrl/async.h>

#include "base/memory/weak_ptr.h"
#include "base/test/fake_iasync_operation_win.h"
#include "chrome/browser/webshare/win/fake_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

using ABI::Windows::Foundation::AsyncStatus;
using ABI::Windows::Foundation::DateTime;
using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Foundation::IAsyncOperationWithProgress;
using ABI::Windows::Foundation::IAsyncOperationWithProgressCompletedHandler;
using ABI::Windows::Foundation::TimeSpan;
using ABI::Windows::Storage::Streams::ByteOrder;
using ABI::Windows::Storage::Streams::IBuffer;
using ABI::Windows::Storage::Streams::IDataWriter;
using ABI::Windows::Storage::Streams::IOutputStream;
using ABI::Windows::Storage::Streams::UnicodeEncoding;
using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Make;
using Microsoft::WRL::RuntimeClass;
using Microsoft::WRL::RuntimeClassFlags;
using Windows::Storage::Streams::IBufferByteAccess;

namespace webshare {
namespace {

class FakeDataWriter final
    : public RuntimeClass<RuntimeClassFlags<Microsoft::WRL::WinRtClassicComMix>,
                          IDataWriter> {
 public:
  explicit FakeDataWriter(IOutputStream* output_stream)
      : output_stream_(output_stream) {}
  FakeDataWriter(const FakeDataWriter&) = delete;
  FakeDataWriter& operator=(const FakeDataWriter&) = delete;
  ~FakeDataWriter() final {
    EXPECT_FALSE(buffer_)
        << "FakeDataWriter destroyed with data pending storage.";
    EXPECT_FALSE(store_async_in_progress_)
        << "FakeDataWriter destroyed while store operation is in progress.";
    EXPECT_TRUE(flush_called_)
        << "FakeDataWriter destroyed without calling FlushAsync.";
  }

  // IDataWriter
  IFACEMETHODIMP get_UnstoredBufferLength(UINT32* value) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_UnicodeEncoding(UnicodeEncoding* value) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP put_UnicodeEncoding(UnicodeEncoding value) final {
    return S_OK;
  }
  IFACEMETHODIMP
  get_ByteOrder(ByteOrder* value) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP
  put_ByteOrder(ByteOrder value) final { return S_OK; }
  IFACEMETHODIMP WriteByte(BYTE value) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP WriteBytes(UINT32 value_length, BYTE* value) final {
    if (store_async_in_progress_) {
      ADD_FAILURE()
          << "WriteBytes called while a store operation is in progress.";
      return E_ILLEGAL_METHOD_CALL;
    }
    if (flush_called_) {
      // Though it is technically legal to call FlushAsync multiple times, there
      // is no good reason to do so, so presumably points to a coding error.
      ADD_FAILURE() << "WriteBytes called after FlushAsync has been called.";
      return E_ILLEGAL_METHOD_CALL;
    }
    if (buffer_) {
      // Multiple 'write' calls would be allowed between calls to StoreAsync,
      // but would be a deviation from the preferred usage pattern of working
      // with files and streams through internally managed chunks. If a good
      // reason to break from this pattern is introduced this class should be
      // updated to keep a queue of IBuffers rather than a single one.
      ADD_FAILURE() << "WriteBytes called with data already pending storage.";
      return E_ILLEGAL_METHOD_CALL;
    }
    buffer_ = Make<FakeBuffer>(value_length);

    HRESULT hr = buffer_->put_Length(value_length);
    if (FAILED(hr)) {
      EXPECT_HRESULT_SUCCEEDED(hr);
      return hr;
    }

    ComPtr<IBufferByteAccess> buffer_byte_access;
    hr = buffer_.As(&buffer_byte_access);
    if (FAILED(hr)) {
      EXPECT_HRESULT_SUCCEEDED(hr);
      return hr;
    }

    byte* raw_buffer;
    hr = buffer_byte_access->Buffer(&raw_buffer);
    if (FAILED(hr)) {
      EXPECT_HRESULT_SUCCEEDED(hr);
      return hr;
    }

    for (UINT32 i = 0; i < value_length; i++) {
      raw_buffer[i] = value[i];
    }
    return S_OK;
  }
  IFACEMETHODIMP
  WriteBuffer(IBuffer* buffer) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP
  WriteBufferRange(IBuffer* buffer, UINT32 start, UINT32 count) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP WriteBoolean(boolean value) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP WriteGuid(GUID value) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP WriteInt16(INT16 value) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP WriteInt32(INT32 value) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP WriteInt64(INT64 value) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP WriteUInt16(UINT16 value) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP WriteUInt32(UINT32 value) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP WriteUInt64(UINT64 value) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP WriteSingle(FLOAT value) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP WriteDouble(DOUBLE value) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP
  WriteDateTime(DateTime value) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP
  WriteTimeSpan(TimeSpan value) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP WriteString(HSTRING value, UINT32* code_unit_count) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP MeasureString(HSTRING value, UINT32* code_unit_count) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP
  StoreAsync(IAsyncOperation<UINT32>** operation) final {
    if (store_async_in_progress_) {
      ADD_FAILURE()
          << "StoreAsync called while a store operation is in progress.";
      return E_ILLEGAL_METHOD_CALL;
    }
    if (flush_called_) {
      // Though it is technically legal to call FlushAsync multiple times, there
      // is no good reason to do so, so presumably points to a coding error.
      ADD_FAILURE() << "StoreAsync called after FlushAsync has been called.";
      return E_ILLEGAL_METHOD_CALL;
    }
    if (!buffer_) {
      ADD_FAILURE() << "StoreAsync called with no data pending for storage.";
      return E_ILLEGAL_METHOD_CALL;
    }

    auto fake_iasync_operation = Make<base::win::FakeIAsyncOperation<UINT32>>();
    HRESULT hr = fake_iasync_operation->QueryInterface(IID_PPV_ARGS(operation));
    if (FAILED(hr)) {
      EXPECT_HRESULT_SUCCEEDED(hr);
      return hr;
    }

    ComPtr<IAsyncOperationWithProgress<UINT32, UINT32>> write_operation;
    hr = output_stream_->WriteAsync(buffer_.Get(), &write_operation);
    if (FAILED(hr)) {
      EXPECT_HRESULT_SUCCEEDED(hr);
      return hr;
    }
    buffer_.Reset();

    auto weak_ptr = weak_factory_.GetWeakPtr();
    hr = write_operation->put_Completed(
        Callback<IAsyncOperationWithProgressCompletedHandler<UINT32, UINT32>>(
            [weak_ptr, fake_iasync_operation](
                IAsyncOperationWithProgress<UINT32, UINT32>* async_operation,
                AsyncStatus async_status) {
              if (!weak_ptr) {
                ADD_FAILURE() << "Writer operation completed after "
                                 "FakeDataWriter was destroyed.";
                return E_UNEXPECTED;
              }
              weak_ptr->store_async_in_progress_ = false;

              EXPECT_EQ(async_status, AsyncStatus::Completed);
              UINT32 results;
              EXPECT_HRESULT_SUCCEEDED(async_operation->GetResults(&results));
              fake_iasync_operation->CompleteWithResults(results);
              return S_OK;
            })
            .Get());
    if (FAILED(hr)) {
      EXPECT_HRESULT_SUCCEEDED(hr);
      return hr;
    }

    store_async_in_progress_ = true;
    return S_OK;
  }
  IFACEMETHODIMP
  FlushAsync(IAsyncOperation<bool>** operation) final {
    if (store_async_in_progress_) {
      ADD_FAILURE()
          << "FlushAsync called while a store operation is in progress.";
      return E_ILLEGAL_METHOD_CALL;
    }
    if (flush_called_) {
      // Though it is technically legal to call FlushAsync multiple times, there
      // is no good reason to do so, so presumably points to a coding error.
      ADD_FAILURE() << "FlushAsync called multiple times.";
      return E_UNEXPECTED;
    }
    if (buffer_) {
      ADD_FAILURE() << "FlushAsync called with data pending storage.";
      return E_ILLEGAL_METHOD_CALL;
    }
    flush_called_ = true;
    HRESULT hr = output_stream_->FlushAsync(operation);
    if (FAILED(hr)) {
      EXPECT_HRESULT_SUCCEEDED(hr);
      return hr;
    }
    return hr;
  }
  IFACEMETHODIMP
  DetachBuffer(IBuffer** buffer) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP DetachStream(IOutputStream** output_stream) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }

 private:
  ComPtr<IBuffer> buffer_;
  bool flush_called_ = false;
  ComPtr<IOutputStream> output_stream_;
  bool store_async_in_progress_ = false;
  base::WeakPtrFactory<FakeDataWriter> weak_factory_{this};
};

}  // namespace

FakeDataWriterFactory::FakeDataWriterFactory() = default;
FakeDataWriterFactory::~FakeDataWriterFactory() = default;

IFACEMETHODIMP FakeDataWriterFactory::CreateDataWriter(
    IOutputStream* output_stream,
    IDataWriter** data_writer) {
  if (!output_stream) {
    ADD_FAILURE() << "CreateDataWriter called with null output_stream.";
    return E_INVALIDARG;
  }
  auto fake_data_writer = Make<FakeDataWriter>(output_stream);
  HRESULT hr = fake_data_writer->QueryInterface(IID_PPV_ARGS(data_writer));
  if (FAILED(hr)) {
    EXPECT_HRESULT_SUCCEEDED(hr);
    return hr;
  }
  return S_OK;
}

}  // namespace webshare
