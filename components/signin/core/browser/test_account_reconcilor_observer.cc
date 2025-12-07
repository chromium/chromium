// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/test_account_reconcilor_observer.h"

using ::signin_metrics::AccountReconcilorState;

TestAccountReconcilorObserver::TestAccountReconcilorObserver(
    AccountReconcilor* reconcilor,
    std::optional<AccountReconcilorState> wait_state)
    : wait_state_(wait_state) {
  scoped_observation_.Observe(reconcilor);
}

TestAccountReconcilorObserver::~TestAccountReconcilorObserver() = default;

void TestAccountReconcilorObserver::WaitForStateChange() {
  run_loop_.Run();
}

void TestAccountReconcilorObserver::OnStateChanged(
    AccountReconcilorState state) {
  if (state == AccountReconcilorState::kRunning) {
    ++started_count_;
  }
  if (state == AccountReconcilorState::kError) {
    ++error_count_;
  }
  if (wait_state_.has_value() && state != *wait_state_) {
    return;
  }
  run_loop_.Quit();
}

void TestAccountReconcilorObserver::OnBlockReconcile() {
  ++blocked_count_;
}

void TestAccountReconcilorObserver::OnUnblockReconcile() {
  ++unblocked_count_;
}
