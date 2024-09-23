// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_AUTH_HUB_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_AUTH_HUB_IMPL_H_

#include <memory>
#include <optional>

#include "base/callback_list.h"
#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/osauth/impl/auth_factor_presence_cache.h"
#include "chromeos/ash/components/osauth/impl/auth_hub_attempt_handler.h"
#include "chromeos/ash/components/osauth/impl/auth_hub_common.h"
#include "chromeos/ash/components/osauth/impl/auth_hub_mode_lifecycle.h"
#include "chromeos/ash/components/osauth/impl/auth_hub_vector_lifecycle.h"
#include "chromeos/ash/components/osauth/public/auth_attempt_consumer.h"
#include "chromeos/ash/components/osauth/public/auth_factor_engine.h"
#include "chromeos/ash/components/osauth/public/auth_hub.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "components/account_id/account_id.h"

namespace ash {

// AuthHub implementation.
// This class owns 3 additional classes that cover different stages of
// authentication behavior:
// * `AuthHubModeLifecycle` governs top-level lifecycle: initialization/
//    shutdown when transitioning between login screen (multiple users,
//    only one authentication purpose, cryptohome-based factors only,
//    no direct access to user policies) and in-session (single user,
//    multiple purposes, additional factors, direct access to policies)
// * `AuthHubVectorLifecycle` governs steps required to set up
//    authentication flow for particular user/purpose combination.
// * `AuthHubAttemptHandler` handles events within single authentication
//    flow: enabling/disabling particular factors, tracking attempts to
//    use particular factor, etc.

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH) AuthHubImpl
    : public AuthHub,
      public AuthHubModeLifecycle::Owner,
      public AuthHubVectorLifecycle::Owner,
      public AuthHubAttemptHandler::Owner {
 public:
  explicit AuthHubImpl(AuthFactorPresenceCache* factor_cache);
  ~AuthHubImpl() override;

  // ----- AuthHub implementation:

  // High-level lifecycle:
  void InitializeForMode(AuthHubMode target) override;
  void EnsureInitialized(base::OnceClosure on_initialized) override;

  void StartAuthentication(AccountId accountId,
                           AuthPurpose purpose,
                           AuthAttemptConsumer* consumer) override;

  void CancelCurrentAttempt(AuthHubConnector* connector) override;
  void Shutdown() override;

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
  void OnAttemptCleanedUp(const AuthAttemptVector& attempt) override;
  void OnAttemptFinished(const AuthAttemptVector& attempt) override;
  void OnAttemptCancelled(const AuthAttemptVector& attempt) override;
  void OnIdle() override;

  // AuthHubAttemptHandler::Owner
  void UpdateFactorUiCache(const AuthAttemptVector& attempt,
                           AuthFactorsSet available_factors) override;
  void OnAuthenticationSuccess(const AuthAttemptVector& attempt,
                               AshAuthFactor factor) override;
  void OnFactorAttemptFailed(const AuthAttemptVector& attempt,
                             AshAuthFactor factor) override;

 private:
  void SwitchToModeImpl(AuthHubMode target);
  bool PurposeMatchesMode(AuthPurpose purpose, AuthHubMode mode);
  // Checks if `first` attempt have higher priority and should
  // override `second`.
  bool AttemptShouldOverrideAnother(const AuthAttemptVector& first,
                                    const AuthAttemptVector& second);
  void OnFactorAttemptFinishedForCancel();

  AuthEnginesMap engines_;

  std::optional<AuthAttemptVector> current_attempt_;
  raw_ptr<AuthAttemptConsumer> attempt_consumer_ = nullptr;

  std::optional<AuthAttemptVector> pending_attempt_;
  raw_ptr<AuthAttemptConsumer> pending_consumer_ = nullptr;
  std::optional<AshAuthFactor> authenticated_factor_;

  // Target mode for initialization, used to store last request when
  // some extra actions are required before mode can be switched.
  // If another mode change is requested during such actions, it
  // is safe to just replace target_mode_.
  std::optional<AuthHubMode> target_mode_;

  base::OnceCallbackList<void()> on_initialized_listeners_;

  std::unique_ptr<AuthHubAttemptHandler> attempt_handler_;
  std::unique_ptr<AuthHubVectorLifecycle> vector_lifecycle_;
  std::unique_ptr<AuthHubModeLifecycle> mode_lifecycle_;
  raw_ptr<AuthFactorPresenceCache> cache_;

  base::WeakPtrFactory<AuthHubImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_AUTH_HUB_IMPL_H_
