// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_DEGRADED_RECOVERABILITY_HANDLER_H_
#define COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_DEGRADED_RECOVERABILITY_HANDLER_H_

#include <memory>
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "components/trusted_vault/trusted_vault_histograms.h"

namespace trusted_vault_pb {
class LocalTrustedVaultDegradedRecoverabilityState;
enum DegradedRecoverabilityValue : int;
}  // namespace trusted_vault_pb

namespace trusted_vault {
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
        const trusted_vault_pb::LocalTrustedVaultDegradedRecoverabilityState&
            degraded_recoverability_state) = 0;
    virtual void OnDegradedRecoverabilityChanged() = 0;
  };

  // `connection` and `delegate` must not be null and must outlive this object.
  TrustedVaultDegradedRecoverabilityHandler(
      TrustedVaultConnection* connection,
      Delegate* delegate,
      const CoreAccountInfo& account_info,
      const trusted_vault_pb::LocalTrustedVaultDegradedRecoverabilityState&
          degraded_recoverability_state);
  TrustedVaultDegradedRecoverabilityHandler(
      const TrustedVaultDegradedRecoverabilityHandler&) = delete;
  TrustedVaultDegradedRecoverabilityHandler& operator=(
      const TrustedVaultDegradedRecoverabilityHandler&) = delete;
  ~TrustedVaultDegradedRecoverabilityHandler();

  void HintDegradedRecoverabilityChanged(
      TrustedVaultHintDegradedRecoverabilityChangedReasonForUMA reason);
  // The scheduler actually starts with the first call to
  // GetIsRecoverabilityDegraded().
  void GetIsRecoverabilityDegraded(base::OnceCallback<void(bool)> cb);

 private:
  void UpdateCurrentRefreshPeriod();
  void Start();
  void Refresh();
  void OnRecoverabilityIsDegradedDownloaded(
      TrustedVaultRecoverabilityStatus status);

  base::TimeDelta long_degraded_recoverability_refresh_period_;
  base::TimeDelta short_degraded_recoverability_refresh_period_;
  const raw_ptr<TrustedVaultConnection> connection_;
  const raw_ptr<Delegate> delegate_;
  CoreAccountInfo account_info_;
  // A "timer" takes care of invoking Refresh() in the future, once after a
  // `current_refresh_period_` delay has elapsed.
  base::OneShotTimer next_refresh_timer_;
  base::TimeDelta current_refresh_period_;
  trusted_vault_pb::DegradedRecoverabilityValue degraded_recoverability_value_;
  // The last time Refresh has executed, it's initially null until the first
  // Refresh() execution.
  base::TimeTicks last_refresh_time_;
  std::unique_ptr<TrustedVaultConnection::Request>
      ongoing_get_recoverability_request_;

  // If GetIsRecoverabilityDegraded(callback) gets invoked before the first
  // recoverability request to the server, the callback gets deferred until the
  // request is completed.
  base::OnceCallback<void(bool)>
      pending_get_is_recoverability_degraded_callback_;
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_TRUSTED_VAULT_DEGRADED_RECOVERABILITY_HANDLER_H_
