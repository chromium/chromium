// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/enclave_keys_waiter.h"

#include "base/functional/callback.h"
#include "base/run_loop.h"

EnclaveKeysWaiter::EnclaveKeysWaiter(EnclaveManager* enclave_manager)
    : enclave_manager_(enclave_manager) {
  enclave_manager->AddObserver(this);
}

EnclaveKeysWaiter::~EnclaveKeysWaiter() {
  enclave_manager_->RemoveObserver(this);
}

EnclaveManager::OutOfContextRecoveryOutcome EnclaveKeysWaiter::Wait() {
  run_loop_->Run();
  return outcome_;
}

void EnclaveKeysWaiter::OnOutOfContextRecoveryCompletion(
    EnclaveManager::OutOfContextRecoveryOutcome outcome) {
  run_loop_->Quit();
  outcome_ = outcome;
}
