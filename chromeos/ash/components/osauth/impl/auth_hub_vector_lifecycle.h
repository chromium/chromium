// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_AUTH_HUB_VECTOR_LIFECYCLE_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_AUTH_HUB_VECTOR_LIFECYCLE_H_

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/osauth/impl/auth_hub_common.h"
#include "chromeos/ash/components/osauth/public/auth_factor_engine.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

// This class manages inner lifecycle of the auth hub.
// Outer lifecycle includes switching between `AuthAttemptVector`s,
// basically a combination of `AccountId`+`AuthPurpose`.
// This class is re-created every time owning `AuthHub` changes mode,
// and receives initialized engines as parameter.
// Lifecycle is:
//  * Call each engine's `StartAuthFlow` with auth vector, and wait for
//  completion.
//  * Notify `Owner` about available / failed auth factors.
//  * Once attempt is finished, call each engine's `StopAuthFlow`, and wait for
//  completion.
//  * Notify `Owner` that attempt is finished.
// `AuthHubVectorLifecycle` correctly handles attempt start/finish, even if
// request to start/finish was requested in the middle of ongoing start/finish
// sequence.

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH) AuthHubVectorLifecycle
    : public AuthFactorEngine::FactorEngineObserver {
 public:
  // Interface to interact with owning AuthHub:
  class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH) Owner {
   public:
    virtual ~Owner();
    virtual AuthFactorEngine::FactorEngineObserver* AsEngineObserver() = 0;
    virtual void OnAttemptStarted(const AuthAttemptVector& attempt,
                                  AuthFactorsSet available_factors,
                                  AuthFactorsSet failed_factors) = 0;
    virtual void OnAttemptFinished(const AuthAttemptVector& attempt) = 0;
    virtual void OnIdle() = 0;
  };

  AuthHubVectorLifecycle(Owner* owner,
                         AuthHubMode mode,
                         const AuthEnginesMap& engines);
  ~AuthHubVectorLifecycle() override;

  void StartAttempt(const AuthAttemptVector& vector);
  void OnFactorInitialized(AshAuthFactor factor);
  void CancelAttempt();
  bool IsIdle() const;

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

 private:
  enum class Stage {
    kIdle,
    kStartingAttempt,
    kStarted,
    kFinishingAttempt,
  };

  struct FactorAttemptState;

  void StartForTargetAttempt();
  void OnAttemptStartWatchdog();
  void ProceedIfAllFactorsStarted();

  void FinishAttempt();
  void OnFactorFinished(AshAuthFactor factor);
  void OnAttemptFinishWatchdog();
  void ProceedIfAllFactorsFinished();

  Stage stage_ = Stage::kIdle;

  absl::optional<AuthAttemptVector> current_attempt_;
  absl::optional<AuthAttemptVector> target_attempt_;
  absl::optional<AuthAttemptVector> initializing_for_;
  absl::optional<AuthAttemptVector> last_started_attempt_;

  AuthEnginesMap available_engines_;

  base::flat_map<AshAuthFactor, FactorAttemptState> engines_;

  base::OneShotTimer watchdog_;
  base::raw_ptr<Owner> owner_;
  base::WeakPtrFactory<AuthHubVectorLifecycle> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_AUTH_HUB_VECTOR_LIFECYCLE_H_
