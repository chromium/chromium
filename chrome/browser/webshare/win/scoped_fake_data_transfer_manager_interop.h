// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBSHARE_WIN_SCOPED_FAKE_DATA_TRANSFER_MANAGER_INTEROP_H_
#define CHROME_BROWSER_WEBSHARE_WIN_SCOPED_FAKE_DATA_TRANSFER_MANAGER_INTEROP_H_

#include <wrl/client.h>

namespace webshare {

class FakeDataTransferManagerInterop;

// Creates and registers a FakeDataTransferManagerInterop on creation and cleans
// it up on tear down, allowing GTests to easily simulate the Windows APIs used
// for the Share contract.
class ScopedFakeDataTransferManagerInterop {
 public:
  ScopedFakeDataTransferManagerInterop();
  ScopedFakeDataTransferManagerInterop(
      const ScopedFakeDataTransferManagerInterop&) = delete;
  ScopedFakeDataTransferManagerInterop& operator=(
      const ScopedFakeDataTransferManagerInterop&) = delete;
  ~ScopedFakeDataTransferManagerInterop();

  // Initializes this component, creating test failures if anything does not
  // succeed. Intended to be called from a test's SetUp function, after having
  // verified this is a supported environment.
  void SetUp();

  FakeDataTransferManagerInterop& instance();

 private:
  bool set_up_ = false;
  Microsoft::WRL::ComPtr<FakeDataTransferManagerInterop> instance_;
};

}  // namespace webshare

#endif  // CHROME_BROWSER_WEBSHARE_WIN_SCOPED_FAKE_DATA_TRANSFER_MANAGER_INTEROP_H_
