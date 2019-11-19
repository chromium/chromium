// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_DIAGNOSTICS_PROVIDER_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_DIAGNOSTICS_PROVIDER_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "components/signin/public/identity_manager/load_credentials_state.h"

namespace signin {

// DiagnosticsProvider is the interface to obtain diagnostics about
// IdentityManager internals.
class DiagnosticsProvider {
 public:
  DiagnosticsProvider() = default;
  virtual ~DiagnosticsProvider() = default;

  // Returns the state of the load credentials operation.
  virtual signin::LoadCredentialsState
  GetDetailedStateOfLoadingOfRefreshTokens() const = 0;

  // Returns the time until a access token request can be sent (will be zero if
  // the release time is in the past).
  virtual base::TimeDelta GetDelayBeforeMakingAccessTokenRequests() const = 0;

  // Returns the time until a cookie request can be sent (will be zero if the
  // release time is in the past).
  virtual base::TimeDelta GetDelayBeforeMakingCookieRequests() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(DiagnosticsProvider);
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_DIAGNOSTICS_PROVIDER_H_
