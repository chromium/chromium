// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_AUTH_HUB_MODE_LIFECYCLE_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_AUTH_HUB_MODE_LIFECYCLE_H_

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/osauth/impl/auth_hub_common.h"
#include "chromeos/ash/components/osauth/public/common_types.h"

namespace ash {

// This class manages outer lifecycle of the auth hub.
// Outer lifecycle includes switching between AuthHub modes, and usually
// would use the following route:
// `kNone` -> `kLoginScreen` -> `kInSession` -> `kNone`, but there are
// two notable exceptions:
// * after in-session crash AuthHub would go `kNone` -> `kInSession`
// * Until ChromeOS multi-profile is made obsolete by Lacros
//   AuthHub would need to support `kInSession`->`kLoginScreen`->`kInSession`
//   when showing/hiding "Add user" screen.
// Each mode lifecycle includes:
//  * Using `AuthFactorEngineFactory`-ies to create `AuthFactorEngine`s;
//  * Calling `InitializeCommon(...)` for all engines and waiting for them to
//    complete;
//  * Running in given mode (outside the scope of this class)
//  * Calling `ShutdownCommon(...)` for all engines and waiting for them to
//    complete;
//  * Destroying all engine instances;
//  * Switching to another mode if necessary.
// `AuthHubModeLifecycle` correctly handles attempt mode
// initialization/shutdown, even if request to switch mode/shut down  was
// requested in the middle of ongoing init/shutdown sequence.

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH) AuthHubModeLifecycle {
 public:
  // Interface to interact with owning AuthHub:
  class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH) Owner {
   public:
    virtual ~Owner();
    virtual void OnReadyForMode(AuthHubMode mode,
                                AuthEnginesMap available_engines) = 0;
    virtual void OnExitedMode(AuthHubMode mode) = 0;
    virtual void OnModeShutdown() = 0;
  };

  explicit AuthHubModeLifecycle(Owner* owner);
  ~AuthHubModeLifecycle();

  bool IsReady();
  AuthEnginesMap GetAvailableEngines();
  AuthHubMode GetCurrentMode() const;

  // Starts initialization sequence, or updates `target_mode_`, if stage is not
  // `kUninitialized`, triggering `ShutDownEngines` if necessary.
  void SwitchToMode(AuthHubMode mode);

 private:
  enum class Stage {
    kUninitialized,
    kStartingServices,
    kStarted,
    kShuttingDownServices,
  };

  struct EngineState;

  // Start initialization sequence.
  void InitializeEnginesForMode();
  // Called for each auth factor, would trigger `CheckInitializationStatus`.
  void OnAuthEngineInitialized(AshAuthFactor factor);
  // Ensures that shutdown sequence would eventually finish, triggers
  // `CheckInitializationStatus`.
  void OnInitializationWatchdog();
  // Checks if initialization sequence is completed, notifies `owner_`.
  // If there is a new `target_mode_`, would trigger `ShutDownEngines` without
  // notifying `owner_`.
  void CheckInitializationStatus();

  // Starts shutdown sequence.
  void ShutDownEngines();
  // Called for each auth factor, would trigger `CheckShutdownStatus`.
  void OnAuthEngineShutdown(AshAuthFactor factor);
  // Ensures that shutdown sequence would eventually finish, triggers
  // `CheckShutdownStatus`.
  void OnShutdownWatchdog();
  // Checks if shutdown sequence is completed, notifies `owner_`.
  // If there is a new `target_mode_`, would trigger `InitializeEnginesForMode`.
  void CheckShutdownStatus();

  AuthHubMode mode_ = AuthHubMode::kNone;
  AuthHubMode initializing_for_mode_ = AuthHubMode::kNone;
  AuthHubMode target_mode_ = AuthHubMode::kNone;

  Stage stage_ = Stage::kUninitialized;

  base::flat_map<AshAuthFactor, EngineState> engines_;

  base::OneShotTimer watchdog_;
  raw_ptr<Owner> owner_;
  base::WeakPtrFactory<AuthHubModeLifecycle> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_IMPL_AUTH_HUB_MODE_LIFECYCLE_H_
