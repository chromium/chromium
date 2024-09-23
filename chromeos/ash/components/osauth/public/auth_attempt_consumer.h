// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_AUTH_ATTEMPT_CONSUMER_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_AUTH_ATTEMPT_CONSUMER_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/osauth/public/common_types.h"

namespace ash {

class AuthFactorStatusConsumer;

// Opaque class that represents state of current user authentication attempt,
// used to interact with particular AuthFactors via API.
class AuthHubConnector;

// Interface between AuthHub and a UI surface that requests authentication from
// AuthHub. It is used by AuthHub to notify surface about result of the
// authentication attempt.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH) AuthAttemptConsumer {
 public:
  virtual ~AuthAttemptConsumer() = default;

  // Called by AuthHub when `RequestAuthentication` can not be performed
  // in current state.
  // One example would be when when there is another user authentication
  // attempt happening with "higher" or equal priority:
  // When session is locked, attempt to enter settings (e.g. triggered
  // by some script) would be automatically rejected.
  //
  // Implementing surface might retry an attempt, but should apply
  // reasonable back-off logic.
  virtual void OnUserAuthAttemptRejected() = 0;

  // AuthHub would call this method to confirm start of user auth flow,
  // and obtain a reference to `AuthFactorStatusConsumer` that would
  // receive individual factor updates.
  // AuthHub would immediately call `InitializeUi` on the `out_consumer`
  // after this call.
  // AuthHub expects `out_consumer` to be valid, and guarantees that
  // reference to `connector` would be valid until either method is called
  // to indicating end of authentication flow:
  //   * `OnUserAuthAttemptCancelled`
  //   * `OnAccountNotFound`
  //   * `OnUserAuthSuccess`
  virtual void OnUserAuthAttemptConfirmed(
      AuthHubConnector* connector,
      raw_ptr<AuthFactorStatusConsumer>& out_consumer) = 0;

  // AuthHub would call this method in edge-case scenario when authentication
  // was requested for account that is not present on the device.
  virtual void OnAccountNotFound() = 0;

  // AuthHub would this method if user auth flow is cancelled for some
  // reason.
  // For example, user verification for WebAuthN would be cancelled if
  // session gets locked (another `RequestAuthentication` with `kScreenUnlock`
  // purpose is called).
  //
  // This notification is similar to `OnUserAuthAttemptRejected`, but
  // is called if `OnUserAuthAttemptConfirmed` was called before.
  // Similar to `OnUserAuthAttemptRejected` surface might retry an attempt,
  // but should apply reasonable back-off logic.
  virtual void OnUserAuthAttemptCancelled() = 0;

  // This method is called by AuthHub upon each failed attempt.
  // UI surface might handle it in a special way (e.g. suggest the recovery).
  virtual void OnFactorAttemptFailed(AshAuthFactor factor) = 0;

  // AuthHub would call this method upon successful user authentication.
  // `factor` indicates a factor that was used to successfully authenticate
  // the user.
  // `token` is a reference to resulting UserContext in `AuthSessionStorage`.
  virtual void OnUserAuthSuccess(AshAuthFactor factor,
                                 const AuthProofToken& token) = 0;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_AUTH_ATTEMPT_CONSUMER_H_
