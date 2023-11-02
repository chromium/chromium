// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBSHARE_WIN_FAKE_DATA_WRITER_FACTORY_H_
#define CHROME_BROWSER_WEBSHARE_WIN_FAKE_DATA_WRITER_FACTORY_H_

#include <windows.storage.streams.h>
#include <wrl/implements.h>

namespace webshare {

// Provides an implementation of IDataWriterFactory for use in GTests.
class __declspec(uuid("4A0022E7-D891-4B52-8736-77497F9FFE14"))
    FakeDataWriterFactory final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::WinRtClassicComMix>,
          ABI::Windows::Storage::Streams::IDataWriterFactory> {
 public:
  FakeDataWriterFactory();
  FakeDataWriterFactory(const FakeDataWriterFactory&) = delete;
  FakeDataWriterFactory& operator=(const FakeDataWriterFactory&) = delete;
  ~FakeDataWriterFactory() final;

  // ABI::Windows::Storage::Streams::IDataWriterFactory:
  IFACEMETHODIMP CreateDataWriter(
      ABI::Windows::Storage::Streams::IOutputStream* outputStream,
      ABI::Windows::Storage::Streams::IDataWriter** data_writer) final;
};

}  // namespace webshare

#endif  // CHROME_BROWSER_WEBSHARE_WIN_FAKE_DATA_WRITER_FACTORY_H_
