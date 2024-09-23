// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_AUTH_HUB_H_
#define CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_AUTH_HUB_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "chromeos/ash/components/osauth/public/auth_parts.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "components/account_id/account_id.h"

namespace ash {

class AuthAttemptConsumer;
class AuthHubConnector;

// Main entry point for ChromeOS local Authentication.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_OSAUTH) AuthHub {
 public:
  // Convenience method.
  static inline AuthHub* Get() { return AuthParts::Get()->GetAuthHub(); }

  // AuthHub is not initialized immediately after creation, to allow
  // registering extra FactorEngines in AuthParts, so it starts in
  // `AuthHubMode::kNone`.
  //
  // Usually AuthHub would go `kNone` -> `kLoginScreen` -> `kInSession`,
  // but there are two exceptions:
  // * after in-session crash AuthHub would go `kNone` -> `kInSession`;
  // * Until ChromeOS multi-profile is made obsolette by Lacros
  //   AuthHub would need to go `kInSession`->`kLoginScreen`->`kInSession`
  //   when showing/hiding "Add user" screen.
  // This method should not be called from other AuthHub callbacks
  // to prevent reenterant loops.
  virtual void InitializeForMode(AuthHubMode target) = 0;

  virtual void EnsureInitialized(base::OnceClosure on_initialized) = 0;

  virtual void StartAuthentication(AccountId accountId,
                                   AuthPurpose purpose,
                                   AuthAttemptConsumer* consumer) = 0;

  // Cancel the current attempt, eventually leads to
  // `AuthAttemptConsumer::OnUserAuthAttemptCancelled` being called, and the
  // destruction of the UI.
  virtual void CancelCurrentAttempt(AuthHubConnector* connector) = 0;

  virtual void Shutdown() = 0;

  virtual ~AuthHub() = default;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_OSAUTH_PUBLIC_AUTH_HUB_H_
