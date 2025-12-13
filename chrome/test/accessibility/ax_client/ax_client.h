// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_ACCESSIBILITY_AX_CLIENT_AX_CLIENT_H_
#define CHROME_TEST_ACCESSIBILITY_AX_CLIENT_AX_CLIENT_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/threading/thread_checker.h"
#include "chrome/test/accessibility/ax_client/ax_client.test-mojom.h"

namespace ax_client {

class AxClientImpl;

class AxClient : public mojom::AxClient {
 public:
  enum class ClientApi {
    // Use the UI Automation client APIs.
    kUiAutomation,

    // Use the IAccessible2 w/ Microsoft Active Accessibility client APIs.
    kIAccessible2,
  };

  // `on_destroyed` is a callback to be run when the instance is destroyed due
  // to the connection to the remote being broken.
  AxClient(ClientApi client_api, base::OnceClosure on_destroyed);
  AxClient(const AxClient&) = delete;
  AxClient& operator=(const AxClient&) = delete;
  ~AxClient() override;

  // mojom::AxClient:
  void Initialize(uint32_t hwnd, InitializeCallback callback) override;
  void FindAll(FindAllCallback callback) override;
  void Shutdown(ShutdownCallback callback) override;
  void Terminate() override;

 private:
  // A callback run when the connection to the remote is broken.
  base::OnceClosure on_destroyed_ GUARDED_BY_CONTEXT(thread_checker_);

  // The underlying implementation of the client. Selected at runtime based on
  // the desired `ClientApi`.
  std::unique_ptr<AxClientImpl> impl_ GUARDED_BY_CONTEXT(thread_checker_);

  THREAD_CHECKER(thread_checker_);
};

}  // namespace ax_client

#endif  // CHROME_TEST_ACCESSIBILITY_AX_CLIENT_AX_CLIENT_H_
