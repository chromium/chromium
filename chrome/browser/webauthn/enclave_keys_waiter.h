// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_ENCLAVE_KEYS_WAITER_H_
#define CHROME_BROWSER_WEBAUTHN_ENCLAVE_KEYS_WAITER_H_

#include "base/run_loop.h"
#include "chrome/browser/webauthn/enclave_manager.h"

// A utility class for waiting for completion of the operation of storing the
// passkey secret retrieved from an out-of-context flow (e.g., an opportunistic
// retrieval). This class observes enclave manager, starts a RunLoop, and allows
// to wait for the completion of the out-of-context recovery.
class EnclaveKeysWaiter : public EnclaveManager::Observer {
 public:
  explicit EnclaveKeysWaiter(EnclaveManager* enclave_manager);
  ~EnclaveKeysWaiter() override;

  EnclaveManager::OutOfContextRecoveryOutcome Wait();

 private:
  // EnclaveManager::Observer:
  void OnOutOfContextRecoveryCompletion(
      EnclaveManager::OutOfContextRecoveryOutcome outcome) override;

  raw_ptr<EnclaveManager> enclave_manager_;
  std::unique_ptr<base::RunLoop> run_loop_ = std::make_unique<base::RunLoop>();
  EnclaveManager::OutOfContextRecoveryOutcome outcome_;
};

#endif  // CHROME_BROWSER_WEBAUTHN_ENCLAVE_KEYS_WAITER_H_
