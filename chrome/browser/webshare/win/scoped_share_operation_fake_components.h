// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBSHARE_WIN_SCOPED_SHARE_OPERATION_FAKE_COMPONENTS_H_
#define CHROME_BROWSER_WEBSHARE_WIN_SCOPED_SHARE_OPERATION_FAKE_COMPONENTS_H_

#include <wrl/client.h>

#include "chrome/browser/webshare/win/scoped_fake_data_transfer_manager_interop.h"

namespace webshare {

class FakeDataTransferManagerInterop;
class FakeDataWriterFactory;
class FakeStorageFileStatics;
class FakeUriRuntimeClassFactory;

// Creates, registers, and maintains the fake equivalents of various Windows
// APIS used by the ShareOperation, allowing GTests to easily use a
// ShareOperation.
class ScopedShareOperationFakeComponents final {
 public:
  ScopedShareOperationFakeComponents();
  ScopedShareOperationFakeComponents(
      const ScopedShareOperationFakeComponents&) = delete;
  ScopedShareOperationFakeComponents& operator=(
      const ScopedShareOperationFakeComponents&) = delete;
  ~ScopedShareOperationFakeComponents();

  // Initializes this component, creating test failures if anything does not
  // succeed. Intended to be called from a test's SetUp function, after having
  // verified this is a supported environment.
  void SetUp();

  FakeDataTransferManagerInterop& fake_data_transfer_manager_interop();

 private:
  Microsoft::WRL::ComPtr<FakeDataWriterFactory> fake_data_writer_factory_;
  Microsoft::WRL::ComPtr<FakeStorageFileStatics> fake_storage_file_statics_;
  Microsoft::WRL::ComPtr<FakeUriRuntimeClassFactory>
      fake_uri_runtime_class_factory_;
  ScopedFakeDataTransferManagerInterop
      scoped_fake_data_transfer_manager_interop_;
};

}  // namespace webshare

#endif  // CHROME_BROWSER_WEBSHARE_WIN_SCOPED_SHARE_OPERATION_FAKE_COMPONENTS_H_
