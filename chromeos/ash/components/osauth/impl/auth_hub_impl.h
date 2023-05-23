// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_AUTH_HUB_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_AUTH_HUB_IMPL_H_

#include <memory>

#include "base/callback_list.h"
#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/osauth/impl/auth_hub_common.h"
#include "chromeos/ash/components/osauth/impl/auth_hub_mode_lifecycle.h"
#include "chromeos/ash/components/osauth/impl/auth_hub_vector_lifecycle.h"
#include "chromeos/ash/components/osauth/public/auth_attempt_consumer.h"
#include "chromeos/ash/components/osauth/public/auth_factor_engine.h"
#include "chromeos/ash/components/osauth/public/auth_hub.h"
#include "chromeos/ash/components/osauth/public/common_types.h"

namespace ash {

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH) AuthHubImpl
    : public AuthHub,
      public AuthHubModeLifecycle::Owner,
      public AuthHubVectorLifecycle::Owner,
      public AuthFactorEngine::FactorEngineObserver {
 public:
  explicit AuthHubImpl();
  ~AuthHubImpl() override;

  // ----- AuthHub implementation:

  // High-level lifecycle:
  void InitializeForMode(AuthHubMode target) override;
  void EnsureInitialized(base::OnceClosure on_initialized) override;

  void StartAuthentication(AccountId accountId,
                           AuthPurpose purpose,
                           AuthAttemptConsumer* consumer) override;

  // AuthHubModeLifecycle::Owner:
  void OnReadyForMode(AuthHubMode mode,
                      AuthEnginesMap available_engines) override;
  void OnExitedMode(AuthHubMode mode) override;
  void OnModeShutdown() override;

  // AuthHubVectorLifecycle::Owner:
  AuthFactorEngine::FactorEngineObserver* AsEngineObserver() override;
  void OnAttemptStarted(const AuthAttemptVector& attempt,
                        AuthFactorsSet available_factors,
                        AuthFactorsSet failed_factors) override;
  void OnAttemptFinished(const AuthAttemptVector& attempt) override;
  void OnIdle() override;

  // AuthFactorEngine::FactorEngineObserver:
  void OnFactorPresenceChecked(AshAuthFactor factor,
                               bool factor_present) override;
  void OnFactorAttempt(AshAuthFactor factor) override;
  void OnFactorAttemptResult(AshAuthFactor factor, bool success) override;
  void OnPolicyChanged(AshAuthFactor factor) override;
  void OnLockoutChanged(AshAuthFactor factor) override;
  void OnOrientationRestrictionsChanged(AshAuthFactor factor) override;
  void OnCriticalError(AshAuthFactor factor) override;
  void OnFactorCustomSignal(AshAuthFactor factor) override;

 private:
  bool PurposeMatchesMode(AuthPurpose purpose, AuthHubMode mode);
  // Checks if `first` attempt have higher priority and should
  // override `second`.
  bool AttemptShouldOverrideAnother(const AuthAttemptVector& first,
                                    const AuthAttemptVector& second);

  AuthEnginesMap engines_;

  absl::optional<AuthAttemptVector> current_attempt_;
  base::raw_ptr<AuthAttemptConsumer> attempt_consumer_ = nullptr;

  absl::optional<AuthAttemptVector> pending_attempt_;
  base::raw_ptr<AuthAttemptConsumer> pending_consumer_ = nullptr;

  // Target mode for initialization, used to store last request when
  // some extra actions are required before mode can be switched.
  // If another mode change is requested during such actions, it
  // is safe to just replace target_mode_.
  absl::optional<AuthHubMode> target_mode_;

  base::OnceCallbackList<void()> on_initialized_listeners_;
  std::unique_ptr<AuthHubVectorLifecycle> vector_lifecycle_;
  std::unique_ptr<AuthHubModeLifecycle> mode_lifecycle_;
  base::WeakPtrFactory<AuthHubImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_AUTH_HUB_IMPL_H_
