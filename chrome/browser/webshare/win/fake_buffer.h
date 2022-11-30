// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBSHARE_WIN_FAKE_BUFFER_H_
#define CHROME_BROWSER_WEBSHARE_WIN_FAKE_BUFFER_H_

#include <robuffer.h>
#include <windows.storage.streams.h>
#include <wrl/implements.h>

#include <vector>

namespace webshare {

// Provides an implementation of IBuffer for use in GTests.
class __declspec(uuid("23BAE777-CA37-40B3-8F1C-0B86C1F43EE5")) FakeBuffer final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::WinRtClassicComMix>,
          ABI::Windows::Storage::Streams::IBuffer,
          Windows::Storage::Streams::IBufferByteAccess> {
 public:
  explicit FakeBuffer(UINT32 capacity);
  FakeBuffer(const FakeBuffer&) = delete;
  FakeBuffer& operator=(const FakeBuffer&) = delete;
  ~FakeBuffer() final;

  // ABI::Windows::Storage::Streams::IBuffer:
  IFACEMETHODIMP get_Capacity(UINT32* value) final;
  IFACEMETHODIMP get_Length(UINT32* value) final;
  IFACEMETHODIMP put_Length(UINT32 value) final;

  // Windows::Storage::Streams::IBufferByteAccess
  IFACEMETHODIMP Buffer(byte** value) final;

 private:
  std::vector<unsigned char> data_;
  UINT32 length_ = 0;
};

}  // namespace webshare

#endif  // CHROME_BROWSER_WEBSHARE_WIN_FAKE_BUFFER_H_
