// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/webshare/win/fake_random_access_stream.h"

#include <robuffer.h>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/fake_iasync_operation_win.h"
#include "chrome/browser/webshare/win/fake_iasync_operation_with_progress.h"
#include "testing/gtest/include/gtest/gtest.h"

using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Foundation::IAsyncOperationWithProgress;
using ABI::Windows::Storage::Streams::IBuffer;
using ABI::Windows::Storage::Streams::IInputStream;
using ABI::Windows::Storage::Streams::InputStreamOptions;
using ABI::Windows::Storage::Streams::IOutputStream;
using ABI::Windows::Storage::Streams::IRandomAccessStream;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Make;
using Windows::Storage::Streams::IBufferByteAccess;

namespace ABI {
namespace Windows {
namespace Foundation {

// Define template specializations for the types used. These uuids were randomly
// generated.
template <>
struct __declspec(uuid("99159E96-2AAD-4F4C-91AD-DBD5A92ACF12"))
    IAsyncOperation<unsigned char> : IAsyncOperation_impl<unsigned char> {};

template <>
struct __declspec(uuid("9AF0D4FD-CD18-492E-A17E-27056D4F6481"))
    IAsyncOperationCompletedHandler<unsigned char>
    : IAsyncOperationCompletedHandler_impl<unsigned char> {};

}  // namespace Foundation
}  // namespace Windows
}  // namespace ABI

namespace webshare {
class StreamData final : public base::RefCountedThreadSafe<StreamData> {
 public:
  StreamData() = default;
  StreamData(const StreamData& other) = delete;
  StreamData& operator=(const StreamData&) = delete;

 public:
  HRESULT get_Size(UINT64* value) {
    *value = data_.size();
    return S_OK;
  }

  HRESULT put_Size(UINT64 value) {
    if (flush_async_in_progress_) {
      ADD_FAILURE()
          << "put_Size called while a flush operation is in progress.";
      return E_ILLEGAL_METHOD_CALL;
    }
    if (read_async_in_progress_) {
      ADD_FAILURE() << "put_Size called while a read operation is in progress.";
      return E_ILLEGAL_METHOD_CALL;
    }
    if (write_async_in_progress_) {
      ADD_FAILURE()
          << "put_Size called while a write operation is in progress.";
      return E_ILLEGAL_METHOD_CALL;
    }

    data_.resize(value);
    return S_OK;
  }

  HRESULT ReadAsync(scoped_refptr<base::RefCountedData<UINT64>> position,
                    IBuffer* buffer,
                    UINT32 count,
                    InputStreamOptions options,
                    IAsyncOperationWithProgress<IBuffer*, UINT32>** operation) {
    if (flush_async_in_progress_) {
      ADD_FAILURE()
          << "ReadAsync called while a flush operation is in progress.";
      return E_ILLEGAL_METHOD_CALL;
    }
    if (read_async_in_progress_) {
      ADD_FAILURE()
          << "ReadAsync called while a read operation is in progress.";
      return E_ILLEGAL_METHOD_CALL;
    }
    if (write_async_in_progress_) {
      ADD_FAILURE()
          << "ReadAsync called while a write operation is in progress.";
      return E_ILLEGAL_METHOD_CALL;
    }

    ComPtr<IBuffer> captured_buffer = buffer;
    auto fake_iasync_operation =
        Make<FakeIAsyncOperationWithProgress<IBuffer*, UINT32>>();

    HRESULT hr = fake_iasync_operation->QueryInterface(IID_PPV_ARGS(operation));
    if (FAILED(hr)) {
      EXPECT_HRESULT_SUCCEEDED(hr);
      return hr;
    }

    bool success = base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&StreamData::OnReadAsync, weak_factory_.GetWeakPtr(),
                       position, fake_iasync_operation, captured_buffer,
                       count));
    if (!success) {
      EXPECT_TRUE(success);
      return E_ASYNC_OPERATION_NOT_STARTED;
    }

    read_async_in_progress_ = true;
    return S_OK;
  }

  HRESULT
  WriteAsync(scoped_refptr<base::RefCountedData<UINT64>> position,
             IBuffer* buffer,
             IAsyncOperationWithProgress<UINT32, UINT32>** operation) {
    if (flush_async_in_progress_) {
      ADD_FAILURE()
          << "WriteAsync called while a flush operation is in progress.";
      return E_ILLEGAL_METHOD_CALL;
    }
    if (read_async_in_progress_) {
      ADD_FAILURE()
          << "WriteAsync called while a read operation is in progress.";
      return E_ILLEGAL_METHOD_CALL;
    }
    if (write_async_in_progress_) {
      ADD_FAILURE()
          << "WriteAsync called while a write operation is in progress.";
      return E_ILLEGAL_METHOD_CALL;
    }

    ComPtr<IBuffer> captured_buffer = buffer;
    auto fake_iasync_operation =
        Make<FakeIAsyncOperationWithProgress<UINT32, UINT32>>();

    HRESULT hr = fake_iasync_operation->QueryInterface(IID_PPV_ARGS(operation));
    if (FAILED(hr)) {
      EXPECT_HRESULT_SUCCEEDED(hr);
      return hr;
    }

    bool success = base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&StreamData::OnWriteAsync, weak_factory_.GetWeakPtr(),
                       position, fake_iasync_operation, captured_buffer));
    if (!success) {
      EXPECT_TRUE(success);
      return E_ASYNC_OPERATION_NOT_STARTED;
    }

    write_async_in_progress_ = true;
    return S_OK;
  }

  HRESULT
  FlushAsync(IAsyncOperation<bool>** operation) {
    if (flush_async_in_progress_) {
      ADD_FAILURE()
          << "FlushAsync called while a flush operation is in progress.";
      return E_ILLEGAL_METHOD_CALL;
    }
    if (read_async_in_progress_) {
      ADD_FAILURE()
          << "FlushAsync called while a read operation is in progress.";
      return E_ILLEGAL_METHOD_CALL;
    }
    if (write_async_in_progress_) {
      ADD_FAILURE()
          << "FlushAsync called while a write operation is in progress.";
      return E_ILLEGAL_METHOD_CALL;
    }

    auto fake_iasync_operation = Make<base::win::FakeIAsyncOperation<bool>>();

    HRESULT hr = fake_iasync_operation->QueryInterface(IID_PPV_ARGS(operation));
    if (FAILED(hr)) {
      EXPECT_HRESULT_SUCCEEDED(hr);
      return hr;
    }

    bool success = base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&StreamData::OnFlushAsync, weak_factory_.GetWeakPtr(),
                       fake_iasync_operation));
    if (!success) {
      EXPECT_TRUE(success);
      return E_ASYNC_OPERATION_NOT_STARTED;
    }

    flush_async_in_progress_ = true;
    return S_OK;
  }

 private:
  friend class base::RefCountedThreadSafe<StreamData>;

  virtual ~StreamData() {
    EXPECT_FALSE(flush_async_in_progress_)
        << "StreamData destroyed while flush operation is in progress.";
    EXPECT_FALSE(read_async_in_progress_)
        << "StreamData destroyed while read operation is in progress.";
    EXPECT_FALSE(write_async_in_progress_)
        << "StreamData destroyed while write operation is in progress.";
  }

  void OnFlushAsync(
      ComPtr<base::win::FakeIAsyncOperation<bool>> fake_iasync_operation) {
    ASSERT_TRUE(flush_async_in_progress_);
    flush_async_in_progress_ = false;

    fake_iasync_operation->CompleteWithResults(true);
  }

  void OnReadAsync(scoped_refptr<base::RefCountedData<UINT64>> position,
                   ComPtr<FakeIAsyncOperationWithProgress<IBuffer*, UINT32>>
                       fake_iasync_operation,
                   ComPtr<IBuffer> buffer,
                   UINT32 count) {
    ASSERT_TRUE(read_async_in_progress_);
    read_async_in_progress_ = false;

    // If reading |count| bytes would attempt to read past the end of our inner
    // |data_|, reduce it to only read to the end of our |data_|.
    if (position->data + count > data_.size())
      count = data_.size() - position->data;

    // Fetch the raw buffer to write to.
    ComPtr<IBufferByteAccess> buffer_byte_access;
    EXPECT_HRESULT_SUCCEEDED(buffer.As(&buffer_byte_access));
    byte* raw_buffer;
    EXPECT_HRESULT_SUCCEEDED(buffer_byte_access->Buffer(&raw_buffer));

    // Write the data to the buffer, updating the position and the buffer's
    // length
    EXPECT_HRESULT_SUCCEEDED(buffer->put_Length(count));
    for (UINT32 i = 0; i < count; i++) {
      raw_buffer[i] = data_[position->data + i];
    }
    position->data += count;

    fake_iasync_operation->CompleteWithResults(buffer.Get());
  }

  void OnWriteAsync(scoped_refptr<base::RefCountedData<UINT64>> position,
                    ComPtr<FakeIAsyncOperationWithProgress<UINT32, UINT32>>
                        fake_iasync_operation,
                    ComPtr<IBuffer> buffer) {
    ASSERT_TRUE(write_async_in_progress_);
    write_async_in_progress_ = false;

    UINT32 length;
    ASSERT_HRESULT_SUCCEEDED(buffer->get_Length(&length));

    // Fetch the raw buffer to read from.
    ComPtr<IBufferByteAccess> buffer_byte_access;
    EXPECT_HRESULT_SUCCEEDED(buffer.As(&buffer_byte_access));
    byte* raw_buffer;
    EXPECT_HRESULT_SUCCEEDED(buffer_byte_access->Buffer(&raw_buffer));

    // If reading the full buffer would take more room than is currently in our
    // inner |data_|, resize it to fit.
    if (position->data + length > data_.size())
      data_.resize(position->data + length);

    // Write the buffer to our inner |data_| and update the position.
    for (UINT32 i = 0; i < length; i++) {
      data_[position->data + i] = raw_buffer[i];
    }
    position->data += length;

    fake_iasync_operation->CompleteWithResults(length);
  }

  std::vector<unsigned char> data_;
  bool flush_async_in_progress_ = false;
  bool read_async_in_progress_ = false;
  bool write_async_in_progress_ = false;
  base::WeakPtrFactory<StreamData> weak_factory_{this};
};

FakeRandomAccessStream::FakeRandomAccessStream() {
  position_ = base::MakeRefCounted<base::RefCountedData<UINT64>>();
  shared_data_ = base::MakeRefCounted<StreamData>();
}
FakeRandomAccessStream::~FakeRandomAccessStream() {
  EXPECT_TRUE(is_closed_)
      << "FakeRandomAccessStream destroyed without being closed.";
}

IFACEMETHODIMP FakeRandomAccessStream::get_Size(UINT64* value) {
  if (is_closed_) {
    ADD_FAILURE() << "get_Size called on closed FakeRandomAccessStream.";
    return RO_E_CLOSED;
  }
  return shared_data_->get_Size(value);
}

IFACEMETHODIMP FakeRandomAccessStream::put_Size(UINT64 value) {
  if (is_closed_) {
    ADD_FAILURE() << "put_Size called on closed FakeRandomAccessStream.";
    return RO_E_CLOSED;
  }
  return shared_data_->put_Size(value);
}

IFACEMETHODIMP
FakeRandomAccessStream::GetInputStreamAt(UINT64 position,
                                         IInputStream** stream) {
  if (is_closed_) {
    ADD_FAILURE()
        << "GetInputStreamAt called on closed FakeRandomAccessStream.";
    return RO_E_CLOSED;
  }
  auto copy = Make<FakeRandomAccessStream>();
  copy->position_->data = position;
  copy->shared_data_ = shared_data_;
  EXPECT_HRESULT_SUCCEEDED(copy->QueryInterface(IID_PPV_ARGS(stream)));
  return S_OK;
}

IFACEMETHODIMP
FakeRandomAccessStream::GetOutputStreamAt(UINT64 position,
                                          IOutputStream** stream) {
  if (is_closed_) {
    ADD_FAILURE()
        << "GetOutputStreamAt called on closed FakeRandomAccessStream.";
    return RO_E_CLOSED;
  }
  auto copy = Make<FakeRandomAccessStream>();
  copy->position_->data = position;
  copy->shared_data_ = shared_data_;
  EXPECT_HRESULT_SUCCEEDED(copy->QueryInterface(IID_PPV_ARGS(stream)));
  return S_OK;
}

IFACEMETHODIMP FakeRandomAccessStream::get_Position(UINT64* value) {
  if (is_closed_) {
    ADD_FAILURE() << "get_Position called on closed FakeRandomAccessStream.";
    return RO_E_CLOSED;
  }
  *value = position_->data;
  return S_OK;
}

IFACEMETHODIMP FakeRandomAccessStream::Seek(UINT64 position) {
  if (is_closed_) {
    ADD_FAILURE() << "Seek called on closed FakeRandomAccessStream.";
    return RO_E_CLOSED;
  }
  UINT64 size;
  HRESULT hr = shared_data_->get_Size(&size);
  if (FAILED(hr))
    return hr;

  if (position > size) {
    // Though it is technically legal to call Seek with an invalid |position|
    // value, there is no good reason to do so, so presumably points to a coding
    // error.
    // https://docs.microsoft.com/en-us/uwp/api/windows.storage.streams.irandomaccessstream.seek#remarks
    ADD_FAILURE() << "Seek called with position outside the known valid range.";
    return E_BOUNDS;
  }

  position_->data = position;
  return S_OK;
}

IFACEMETHODIMP
FakeRandomAccessStream::CloneStream(IRandomAccessStream** stream) {
  NOTREACHED_IN_MIGRATION();
  return E_NOTIMPL;
}

IFACEMETHODIMP FakeRandomAccessStream::get_CanRead(boolean* value) {
  if (is_closed_) {
    ADD_FAILURE() << "get_CanRead called on closed FakeRandomAccessStream.";
    return RO_E_CLOSED;
  }
  *value = TRUE;
  return S_OK;
}

IFACEMETHODIMP FakeRandomAccessStream::get_CanWrite(boolean* value) {
  if (is_closed_) {
    ADD_FAILURE() << "get_CanWrite called on closed FakeRandomAccessStream.";
    return RO_E_CLOSED;
  }
  *value = TRUE;
  return S_OK;
}

IFACEMETHODIMP FakeRandomAccessStream::Close() {
  if (is_closed_) {
    ADD_FAILURE() << "Close called on closed FakeRandomAccessStream.";
    return RO_E_CLOSED;
  }
  is_closed_ = true;
  if (on_close_)
    std::move(on_close_).Run();
  return S_OK;
}

IFACEMETHODIMP FakeRandomAccessStream::ReadAsync(
    IBuffer* buffer,
    UINT32 count,
    InputStreamOptions options,
    IAsyncOperationWithProgress<IBuffer*, UINT32>** operation) {
  if (is_closed_) {
    ADD_FAILURE() << "ReadAsync called on closed FakeRandomAccessStream.";
    return RO_E_CLOSED;
  }
  return shared_data_->ReadAsync(position_, buffer, count, options, operation);
}

IFACEMETHODIMP
FakeRandomAccessStream::WriteAsync(
    IBuffer* buffer,
    IAsyncOperationWithProgress<UINT32, UINT32>** operation) {
  if (is_closed_) {
    ADD_FAILURE() << "WriteAsync called on closed FakeRandomAccessStream.";
    return RO_E_CLOSED;
  }
  return shared_data_->WriteAsync(position_, buffer, operation);
}

IFACEMETHODIMP
FakeRandomAccessStream::FlushAsync(IAsyncOperation<bool>** operation) {
  if (is_closed_) {
    ADD_FAILURE() << "FlushAsync called on closed FakeRandomAccessStream.";
    return RO_E_CLOSED;
  }
  return shared_data_->FlushAsync(operation);
}

void FakeRandomAccessStream::OnClose(base::OnceClosure on_close) {
  ASSERT_FALSE(is_closed_)
      << "OnClose called on closed FakeRandomAccessStream.";
  ASSERT_FALSE(on_close_) << "OnClose called on FakeRandomAccessStream that "
                             "already has an OnClose handler defined.";
  on_close_ = std::move(on_close);
}

}  // namespace webshare
