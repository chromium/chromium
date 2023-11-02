// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBSHARE_WIN_FAKE_STORAGE_FILE_STATICS_H_
#define CHROME_BROWSER_WEBSHARE_WIN_FAKE_STORAGE_FILE_STATICS_H_

#include <windows.storage.h>
#include <wrl/implements.h>

namespace webshare {

// Provides an implementation of IStorageFileStatics for use in GTests.
class __declspec(uuid("70A03B12-27C0-499A-B8AE-18E6060BDEDD"))
    FakeStorageFileStatics final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::WinRtClassicComMix>,
          ABI::Windows::Storage::IStorageFileStatics> {
 public:
  FakeStorageFileStatics();
  FakeStorageFileStatics(const FakeStorageFileStatics&) = delete;
  FakeStorageFileStatics& operator=(const FakeStorageFileStatics&) = delete;
  ~FakeStorageFileStatics() final;

  // ABI::Windows::Storage::IStorageFileStatics:
  IFACEMETHODIMP GetFileFromPathAsync(
      HSTRING path,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Storage::StorageFile*>** operation) final;
  IFACEMETHODIMP GetFileFromApplicationUriAsync(
      ABI::Windows::Foundation::IUriRuntimeClass* uri,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Storage::StorageFile*>** operation) final;
  IFACEMETHODIMP CreateStreamedFileAsync(
      HSTRING display_name_with_extension,
      ABI::Windows::Storage::IStreamedFileDataRequestedHandler* data_requested,
      ABI::Windows::Storage::Streams::IRandomAccessStreamReference* thumbnail,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Storage::StorageFile*>** operation) final;
  IFACEMETHODIMP ReplaceWithStreamedFileAsync(
      ABI::Windows::Storage::IStorageFile* file_to_replace,
      ABI::Windows::Storage::IStreamedFileDataRequestedHandler* data_requested,
      ABI::Windows::Storage::Streams::IRandomAccessStreamReference* thumbnail,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Storage::StorageFile*>** operation) final;
  IFACEMETHODIMP CreateStreamedFileFromUriAsync(
      HSTRING display_name_with_extension,
      ABI::Windows::Foundation::IUriRuntimeClass* uri,
      ABI::Windows::Storage::Streams::IRandomAccessStreamReference* thumbnail,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Storage::StorageFile*>** operation) final;
  IFACEMETHODIMP ReplaceWithStreamedFileFromUriAsync(
      ABI::Windows::Storage::IStorageFile* file_to_replace,
      ABI::Windows::Foundation::IUriRuntimeClass* uri,
      ABI::Windows::Storage::Streams::IRandomAccessStreamReference* thumbnail,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Storage::StorageFile*>** operation) final;
};

}  // namespace webshare

#endif  // CHROME_BROWSER_WEBSHARE_WIN_FAKE_STORAGE_FILE_STATICS_H_
