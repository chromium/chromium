// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_ACCESSIBILITY_AX_CLIENT_LAUNCHER_H_
#define CHROME_TEST_ACCESSIBILITY_AX_CLIENT_LAUNCHER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "chrome/test/accessibility/ax_client/ax_client.test-mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace ax_client {

// Launches and manages the lifetime of the ax_client process. The process is
// forcibly terminated on destruction if it is still active.
class Launcher {
 public:
  enum class ClientApi {
    // Use the UI Automation client APIs.
    kUiAutomation,

    // Use the IAccessible2 w/ Microsoft Active Accessibility client APIs.
    kIAccessible2,
  };

  Launcher();
  Launcher(const Launcher&) = delete;
  Launcher& operator=(const Launcher&) = delete;
  ~Launcher();

  // Launches the ax_client process and returns a remote to its `AxClient`.
  // `client_api` indicates which platform API the client is to use to interact
  // with the browser. `on_process_error` will be run if the connection
  // encounters any kind of error condition; e.g., a message validation failure
  // or permanent disconnection.
  mojo::PendingRemote<mojom::AxClient> Launch(
      ClientApi client_api,
      base::RepeatingCallback<void(const std::string&)> on_process_error);

 private:
  // A trampoline to run the owner's on_process_error callback in case of error.
  void OnProcessError(const std::string& error);

  base::Process process_;
  base::RepeatingCallback<void(const std::string&)> on_process_error_;
  base::WeakPtrFactory<Launcher> weak_ptr_factory_{this};
};

}  // namespace ax_client

#endif  // CHROME_TEST_ACCESSIBILITY_AX_CLIENT_LAUNCHER_H_
