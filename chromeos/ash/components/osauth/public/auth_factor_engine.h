// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_AUTH_FACTOR_ENGINE_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_AUTH_FACTOR_ENGINE_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "components/account_id/account_id.h"

namespace ash {

// Common interface for interaction with implementations of various
// authentication factors during authentication.
// As methods for establishing/editing different factors vary too much,
// factor editing capabilities are not covered by this interface.
//
// The overall interaction between AuthHub and factor engines is:
// * Once AuthHub is initialized (on login screen / inside the session),
//   it would attempt to initialize all factor engines (`InitializeCommon`),
//   giving them a chance to wait for underlying services to become ready.
// * Once authentication attempt starts (for user/purpose), AuthHub
//   invokes `StartAuthFlow` on each factor engine to query if the factor
//   is present for the user/purpose. Factors are started in `kDisabled` state
//   and would be enabled by AuthHub once all factors are ready.
// * If factor is not present, `StopAuthFlow` would eventually be called on
//   corresponding engine to release resources.
// * If factor is present, AuthHub would check if there are any restrictions
//   on using this factor, by querying methods like `IsDisabledByPolicy`.
//   Once all restrictions are checked, factor might be disabled via calling
//   `SetUsageAllowed(kDisabled)` method. Disabled factor should not allow
//   any authentication attempts.
// * While authentication is active, factor engine should notify AuthHub
//   about any events that might change factor restrictions: policy changes,
//   factor lockout, etc.
//   Upon notification, AuthHub would query the restriction using corresponding
//   methods and might enable/disable factor as a result.
//   Factor might also be disabled for other reasons (e.g. parallel
//   authentication attempt using another factor, e.g. entering password and
//   touching fingerprint sensor at the same time).
// * If there is an authentication attempt while factor is disabled (e.g.
//   fingerprint touch is detected while another factor is being checked),
//   engine might queue it and run it when factor is enabled again.
// * Engine also notifies AuthHub when authentication attempt is triggered for
//   the factor (AuthHub would disable other factors, and wait for attempt
//   result), and when the outcome of the attempt is known (success/failure).
// * In case of failure, AuthHub would re-enable all factors, and wait for
//   another attempt.
// * In case of success, AuthHub would eventually call `StopAuthFlow` on all
//   engines.
// * Engine should not send any updates over FactorEngineObserver after
//   receiving `StopAuthFlow`.
// * It is guaranteed that `StopAuthFlow` would be called before another
//   `StartAuthFlow`, e.g. when another user pod is selected on the login
//   screen.
// * Upon system shutdown, `ShutdownCommon` would be called for all engines,
//   allowing engines needs to release global resources. No operations would be
//   invoked after `ShutdownCommon` is called.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH) AuthFactorEngine {
 public:
  // Interface for Engine to communicate with AuthHub.
  // When notifying methods, engine should identify itself by
  // providing `factor` value same as one returned by `GetFactor()`.
  class FactorEngineObserver {
   public:
    virtual ~FactorEngineObserver() = default;

    // Notify AuthHub about result of factor presence check.
    virtual void OnFactorPresenceChecked(AshAuthFactor factor,
                                         bool factor_present) = 0;

    // Notify AuthHub about start of authentication attempt using this factor.
    virtual void OnFactorAttempt(AshAuthFactor factor) = 0;
    // Notify AuthHub about result of authentication attempt.
    virtual void OnFactorAttemptResult(AshAuthFactor factor, bool success) = 0;

    // Notify AuthHub about possible changes in various possible restrictions.

    virtual void OnPolicyChanged(AshAuthFactor factor) = 0;
    virtual void OnLockoutChanged(AshAuthFactor factor) = 0;
    virtual void OnFactorSpecificRestrictionsChanged(AshAuthFactor factor) = 0;

    // Notify AuthHub about some critical error. AuthHub would treat
    // this factor as disabled.
    virtual void OnCriticalError(AshAuthFactor factor) = 0;

    // A way for the engine to send an extra signal to UI that is not
    // covered by AuthFactorState. For example, unlocking via nearby
    // paired smartphone might use this to signal that UI showing
    // phone state might need to be updated.
    virtual void OnFactorCustomSignal(AshAuthFactor factor) = 0;
  };

  // Defines how engine should react to authentication attempts.
  enum class UsageAllowed {
    kEnabled,                  // Allow authentication attempts;
    kDisabledParallelAttempt,  // Enqueue or discard authentication attempt;
    kDisabled,                 // Discard authentication attempts.
  };

  using CleanupCallback = base::OnceCallback<void(AshAuthFactor)>;
  using CommonInitCallback = base::OnceCallback<void(AshAuthFactor)>;
  using ShutdownCallback = base::OnceCallback<void(AshAuthFactor)>;

  virtual ~AuthFactorEngine() = default;

  virtual AshAuthFactor GetFactor() const = 0;

  // Factor initialization stage that is not dependent on particular user.
  // E.g. awaiting the required DBus service to start.
  virtual void InitializeCommon(CommonInitCallback callback) = 0;
  virtual void ShutdownCommon(ShutdownCallback callback) = 0;

  // Initialization for particular user/purpose.
  // It is expected that FactorEngine would start a check if the
  // factor is configured for user/purpose, and would notify `observer`
  // by calling either `OnFactorPresenceChecked` or `OnCriticalError`.
  // After that, engine would notify `observer` upon authentication
  // attempts using the factor, changes in affecting policies or
  // other restrictions.
  // It is guaranteed that only one user authentication attempt
  // would be running at time.
  virtual void StartAuthFlow(const AccountId& account,
                             AuthPurpose purpose,
                             FactorEngineObserver* observer) = 0;

  // The way for the owning object to change the object that would
  // be notified about engine events.
  // All events after this call should be sent using new `observer`.
  virtual void UpdateObserver(FactorEngineObserver* observer) = 0;

  // Engine tears down any internal/external state that needs
  // access of allocated resources, such as UserContext, after authentication
  // and before shutdown. Typically, cryptohome based Engine should
  // use this function to call TerminateAuthFactor.
  virtual void CleanUp(CleanupCallback callback) = 0;

  // After this call Engine should stop notifying an `observer` set in
  // `StartAttempt`, and release any resources allocated as a result of
  // starting attempt. Engine should not assume UserContext is available
  // in this function.
  virtual void StopAuthFlow(ShutdownCallback callback) = 0;

  // Client should call this method only when `OnFactorAttemptResult`
  // returned `true`.
  // This method should store all data related to authentication
  // to the `AuthSessionStorage` and return resulting AuthProofToken.
  virtual AuthProofToken StoreAuthenticationContext() = 0;

  // Used by AuthHub to control if authentication attempts can be performed
  // by the engine. Most relevant for factors like fingerprint that can not
  // be disabled at UI level.
  // If `kDisabledParallelAttempt`, engine should not send `OnAuthAttempt`
  // events to observer, but it may queue them and post them as separate
  // event once engine returns to `kEnabled`.
  // If usage is `kDisabled`, then attempts should be ignored.
  virtual void SetUsageAllowed(UsageAllowed usage) = 0;

  // Following group of methods would only be called between `StartAuthFlow`
  // and `StopAuthFlow`, and are related to account/purpose used to start
  // authentication flow.
  // They might be called immediately after `OnFactorPresenceCheck` is
  // called on observer, as well as after corresponding `OnNNNChanged`
  // observer calls.

  virtual bool IsDisabledByPolicy() = 0;
  virtual bool IsLockedOut() = 0;
  // Relevant for factors like fingerprint, where in some
  // device orientations FP sensor can be used unintentionally.
  virtual bool IsFactorSpecificRestricted() = 0;

  // Engines might override these methods to gracefully handle
  // timeout during relevant lifecycle operations.
  virtual void InitializationTimedOut() {}
  virtual void ShutdownTimedOut() {}
  virtual void StartFlowTimedOut() {}
  virtual void StopFlowTimedOut() {}

  // Called when any engine successfully authenticates an auth factor. Engines
  // can override this when they have some action (e.g. removing a lockout) that
  // should be carried out upon auth even when doing via another engine.
  virtual void OnSuccessfulAuthentiation() {}
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_AUTH_FACTOR_ENGINE_H_
