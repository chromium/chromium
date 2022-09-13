// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TRUSTED_VAULT_TRUSTED_VAULT_DEGRADED_RECOVERABILITY_HANDLER_H_
#define COMPONENTS_SYNC_TRUSTED_VAULT_TRUSTED_VAULT_DEGRADED_RECOVERABILITY_HANDLER_H_

#include <memory>
#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/trusted_vault/trusted_vault_connection.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace sync_pb {
class LocalTrustedVaultDegradedRecoverabilityState;
}  // namespace sync_pb

namespace syncer {
// Exposed only for testing.
constexpr base::TimeDelta kLongDegradedRecoverabilityRefreshPeriod =
    base::Days(7);
constexpr base::TimeDelta kShortDegradedRecoverabilityRefreshPeriod =
    base::Hours(1);

// Refreshs the degraded recoverability state by scheduling the requests based
// on the current state, heuristics and last refresh time.
class TrustedVaultDegradedRecoverabilityHandler {
 public:
  class Delegate {
   public:
    Delegate() = default;
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;
    virtual ~Delegate() = default;

    virtual void WriteDegradedRecoverabilityState(
        const sync_pb::LocalTrustedVaultDegradedRecoverabilityState&
            degraded_recoverability_state) = 0;
    virtual void OnDegradedRecoverabilityChanged() = 0;
  };

  // `connection` and `delegate` must not be null and must outlive this object.
  TrustedVaultDegradedRecoverabilityHandler(
      TrustedVaultConnection* connection,
      Delegate* delegate,
      const CoreAccountInfo& account_info,
      const sync_pb::LocalTrustedVaultDegradedRecoverabilityState&
          degraded_recoverability_state);
  TrustedVaultDegradedRecoverabilityHandler(
      const TrustedVaultDegradedRecoverabilityHandler&) = delete;
  TrustedVaultDegradedRecoverabilityHandler& operator=(
      const TrustedVaultDegradedRecoverabilityHandler&) = delete;
  ~TrustedVaultDegradedRecoverabilityHandler();

  void HintDegradedRecoverabilityChanged();
  void StartLongIntervalRefreshing();
  void StartShortIntervalRefreshing();
  void RefreshImmediately();

 private:
  void Start();
  void Refresh();
  void OnRecoverabilityIsDegradedDownloaded(
      TrustedVaultRecoverabilityStatus status);

  const raw_ptr<TrustedVaultConnection> connection_;
  const raw_ptr<Delegate> delegate_;
  CoreAccountInfo account_info_;
  // A "timer" takes care of invoking Refresh() in the future, once after a
  // `current_refresh_period_` delay has elapsed.
  base::OneShotTimer next_refresh_timer_;
  base::TimeDelta current_refresh_period_;
  bool is_recoverability_degraded_;
  // The last time Refresh has executed, it's initially null until the first
  // Refresh() execution.
  base::TimeTicks last_refresh_time_;
  std::unique_ptr<TrustedVaultConnection::Request>
      ongoing_get_recoverability_request_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TRUSTED_VAULT_TRUSTED_VAULT_DEGRADED_RECOVERABILITY_HANDLER_H_
