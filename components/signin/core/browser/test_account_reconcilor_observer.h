// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_TEST_ACCOUNT_RECONCILOR_OBSERVER_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_TEST_ACCOUNT_RECONCILOR_OBSERVER_H_

#include <optional>

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/public/base/signin_metrics.h"

class TestAccountReconcilorObserver : public AccountReconcilor::Observer {
 public:
  // If `wait_state` is provided, `WaitForStateChange()` will only return
  // after the reconcilor reaches the given state.
  explicit TestAccountReconcilorObserver(
      AccountReconcilor* reconcilor,
      std::optional<signin_metrics::AccountReconcilorState> wait_state =
          std::nullopt);

  ~TestAccountReconcilorObserver() override;

  TestAccountReconcilorObserver(const TestAccountReconcilorObserver&) = delete;
  TestAccountReconcilorObserver& operator=(
      const TestAccountReconcilorObserver&) = delete;

  int started_count() const { return started_count_; }
  int blocked_count() const { return blocked_count_; }
  int unblocked_count() const { return unblocked_count_; }
  int error_count() const { return error_count_; }

  // Waits for the reconcilor to reach the `wait_state_` if provided, otherwise
  // waits for any state change.
  void WaitForStateChange();

  // AccountReconcilor::Observer:
  void OnStateChanged(signin_metrics::AccountReconcilorState state) override;
  void OnBlockReconcile() override;
  void OnUnblockReconcile() override;

 private:
  int started_count_ = 0;
  int blocked_count_ = 0;
  int unblocked_count_ = 0;
  int error_count_ = 0;

  const std::optional<signin_metrics::AccountReconcilorState> wait_state_;

  base::RunLoop run_loop_;

  base::ScopedObservation<AccountReconcilor, AccountReconcilor::Observer>
      scoped_observation_{this};
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_TEST_ACCOUNT_RECONCILOR_OBSERVER_H_
