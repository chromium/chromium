// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_AUTH_HUB_ATTEMPT_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_AUTH_HUB_ATTEMPT_HANDLER_H_

#include <optional>

#include "base/callback_list.h"
#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/osauth/impl/auth_hub_common.h"
#include "chromeos/ash/components/osauth/public/auth_factor_engine.h"
#include "chromeos/ash/components/osauth/public/auth_factor_status_consumer.h"
#include "chromeos/ash/components/osauth/public/common_types.h"

namespace ash {

// This class handles all post-initialization interaction with
// `AuthFactorEngine`s during single authentication flow (specified by
// AuthAttemptVector).
// It combines events from individual `AuthFactorEngine`s, calculates
// resulting factor's status, and notifies `AuthFactorStatusConsumer`
// upon all changes.
// This class also enables/disables `AuthFactorEngine`s according to
// their status/parallel authentication attempts; it also tracks
// overall authentication success.
//
// This class is re-created every time owning `AuthHub` starts new,
// user authentication attempt.

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH) AuthHubAttemptHandler
    : public AuthFactorEngine::FactorEngineObserver,
      public AuthHubConnector {
 public:
  // Interface to interact with owning AuthHub:
  class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH) Owner {
   public:
    virtual ~Owner();
    virtual void UpdateFactorUiCache(const AuthAttemptVector& attempt,
                                     AuthFactorsSet configured_factors) = 0;
    virtual void OnAuthenticationSuccess(const AuthAttemptVector& attempt,
                                         AshAuthFactor factor) = 0;
    virtual void OnFactorAttemptFailed(const AuthAttemptVector& attempt,
                                       AshAuthFactor factor) = 0;
  };

  AuthHubAttemptHandler(Owner* owner,
                        const AuthAttemptVector& attempt,
                        const AuthEnginesMap& engines,
                        AuthFactorsSet expected_factors);
  ~AuthHubAttemptHandler() override;

  AuthHubConnector* GetConnector();
  void SetConsumer(raw_ptr<AuthFactorStatusConsumer> consumer);

  // Returns true if there is an ongoing factor attempt.
  bool HasOngoingAttempt() const;

  // Waits for the result of ongoing factor attempt, and calls
  // `callback`, without re-enabling engines.
  void PrepareForShutdown(base::OnceClosure callback);

  // This method is called by owning AuthHub once VectorLifecycle
  // starts all auth engines.
  // This method notifies StatusConsumer about finalized factors list,
  // notifies Owner if cache needs to be updated,
  // and enables all applicable factors to handle authentication attempts.
  void OnFactorsChecked(AuthFactorsSet available_factors,
                        AuthFactorsSet failed_factors);

  // AuthFactorEngine::FactorEngineObserver:
  void OnFactorPresenceChecked(AshAuthFactor factor,
                               bool factor_present) override;
  void OnFactorAttempt(AshAuthFactor factor) override;
  void OnFactorAttemptResult(AshAuthFactor factor, bool success) override;
  void OnPolicyChanged(AshAuthFactor factor) override;
  void OnLockoutChanged(AshAuthFactor factor) override;
  void OnFactorSpecificRestrictionsChanged(AshAuthFactor factor) override;
  void OnCriticalError(AshAuthFactor factor) override;
  void OnFactorCustomSignal(AshAuthFactor factor) override;
  // AuthHubConnector:
  AuthFactorEngine* GetEngine(AshAuthFactor factor) override;

 private:
  // See also a comment for `AuthFactorState`.
  struct FactorAttemptState {
    // Factor is considered failed, no updates / auth attempts
    // should happen for the factor.
    bool engine_failed = false;
    // Reasons for disabling factor:
    bool disabled_by_policy = false;
    bool locked_out = false;
    bool factor_specific_restricted = false;
    // Up-to date usage value for the engine, might not be propagated to
    // engine yet.
    AuthFactorEngine::UsageAllowed intended_usage =
        AuthFactorEngine::UsageAllowed::kDisabled;
    // Latest value propagated to the engine.
    AuthFactorEngine::UsageAllowed engine_usage =
        AuthFactorEngine::UsageAllowed::kDisabled;

    // Up-to date state of the factor, might be not reported to
    // `AuthFactorStatusConsumer` yet.
    AuthFactorState internal_state = AuthFactorState::kCheckingForPresence;
    // Latest value reported to 'AuthFactorStatusConsumer'.
    AuthFactorState reported_state = AuthFactorState::kCheckingForPresence;
  };

  void FillAllStatusValues(AshAuthFactor factor, FactorAttemptState& state);
  void CalculateFactorState(AshAuthFactor factor, FactorAttemptState& state);
  void PropagateEnginesEnabledStatus();
  void PropagateStatusUpdates();
  void UpdateAllFactorStates();

  // TODO(b/271248452): Those `raw_ptr`s are currently dangling due to a problem
  // with the destruction order. Something causes those ptrs to be freed before
  // the execution reaches the destruction of this class. Likely a higher level
  // component in the ownership graph has the objects pointed to below and the
  // current class as downstream dependencies.
  raw_ptr<Owner, DanglingUntriaged> owner_;
  raw_ptr<AuthFactorStatusConsumer, DanglingUntriaged> status_consumer_;

  AuthAttemptVector attempt_;
  AuthEnginesMap engines_;
  AuthFactorsSet initial_factors_;

  AuthFactorsSet available_factors_;

  base::flat_map<AshAuthFactor, FactorAttemptState> factor_state_;

  std::optional<AshAuthFactor> ongoing_attempt_factor_;
  bool authenticated_ = false;

  bool shutting_down_ = false;

  base::OnceCallbackList<void(void)> shutdown_callbacks_;

  base::WeakPtrFactory<AuthHubAttemptHandler> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_AUTH_HUB_ATTEMPT_HANDLER_H_
