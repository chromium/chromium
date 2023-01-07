// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBSHARE_WIN_FAKE_RANDOM_ACCESS_STREAM_H_
#define CHROME_BROWSER_WEBSHARE_WIN_FAKE_RANDOM_ACCESS_STREAM_H_

#include <windows.foundation.h>
#include <windows.storage.streams.h>
#include <wrl/implements.h>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"

namespace webshare {

class StreamData;

// Provides an implementation of IRandomAccessStream for use in GTests.
class __declspec(uuid("66DAD26A-BEDE-4A54-8316-088838CC65A0"))
    FakeRandomAccessStream final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::WinRtClassicComMix>,
          ABI::Windows::Storage::Streams::IRandomAccessStream,
          ABI::Windows::Foundation::IClosable,
          ABI::Windows::Storage::Streams::IInputStream,
          ABI::Windows::Storage::Streams::IOutputStream> {
 public:
  FakeRandomAccessStream();
  FakeRandomAccessStream(const FakeRandomAccessStream& other) = delete;
  FakeRandomAccessStream& operator=(const FakeRandomAccessStream&) = delete;
  ~FakeRandomAccessStream() final;

  // ABI::Windows::Storage::Streams::IRandomAccessStream:
  IFACEMETHODIMP get_Size(UINT64* value) final;
  IFACEMETHODIMP put_Size(UINT64 value) final;
  IFACEMETHODIMP
  GetInputStreamAt(UINT64 position,
                   ABI::Windows::Storage::Streams::IInputStream** stream) final;
  IFACEMETHODIMP
  GetOutputStreamAt(
      UINT64 position,
      ABI::Windows::Storage::Streams::IOutputStream** stream) final;
  IFACEMETHODIMP get_Position(UINT64* value) final;
  IFACEMETHODIMP Seek(UINT64 position) final;
  IFACEMETHODIMP
  CloneStream(IRandomAccessStream** stream) final;
  IFACEMETHODIMP get_CanRead(boolean* value) final;
  IFACEMETHODIMP get_CanWrite(boolean* value) final;

  // ABI::Windows::Foundation::IClosable:
  IFACEMETHODIMP Close() final;

  // ABI::Windows::Storage::Streams::IInputStream:
  IFACEMETHODIMP ReadAsync(
      ABI::Windows::Storage::Streams::IBuffer* buffer,
      UINT32 count,
      ABI::Windows::Storage::Streams::InputStreamOptions options,
      ABI::Windows::Foundation::IAsyncOperationWithProgress<
          ABI::Windows::Storage::Streams::IBuffer*,
          UINT32>** operation) final;

  // ABI::Windows::Storage::Streams::IOutputStream:
  IFACEMETHODIMP
  WriteAsync(
      ABI::Windows::Storage::Streams::IBuffer* buffer,
      ABI::Windows::Foundation::IAsyncOperationWithProgress<UINT32, UINT32>**
          operation) final;
  IFACEMETHODIMP
  FlushAsync(ABI::Windows::Foundation::IAsyncOperation<bool>** operation) final;

  // Sets a callback to be invoked when |Close| is called.
  void OnClose(base::OnceClosure on_close);

 private:
  scoped_refptr<StreamData> shared_data_;
  scoped_refptr<base::RefCountedData<UINT64>> position_;
  bool is_closed_ = false;
  base::OnceClosure on_close_;
};

}  // namespace webshare

#endif  // CHROME_BROWSER_WEBSHARE_WIN_FAKE_RANDOM_ACCESS_STREAM_H_
