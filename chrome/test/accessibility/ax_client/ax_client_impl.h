// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_ACCESSIBILITY_AX_CLIENT_AX_CLIENT_IMPL_H_
#define CHROME_TEST_ACCESSIBILITY_AX_CLIENT_AX_CLIENT_IMPL_H_

#include "base/win/windows_types.h"

namespace ax_client {

// An abstract interface to concrete implementations of the client; one for each
// client API (UIA vs IA2/MSAA).
class AxClientImpl {
 public:
  AxClientImpl(const AxClientImpl&) = delete;
  AxClientImpl& operator=(const AxClientImpl&) = delete;
  virtual ~AxClientImpl() = default;

  // Implementations of the methods exposed by ax_client::mojom::AxClient.
  virtual HRESULT Initialize(HWND browser_window) = 0;
  virtual HRESULT FindAll() = 0;
  virtual void Shutdown() = 0;

 protected:
  AxClientImpl() = default;
};

}  // namespace ax_client

#endif  // CHROME_TEST_ACCESSIBILITY_AX_CLIENT_AX_CLIENT_IMPL_H_
