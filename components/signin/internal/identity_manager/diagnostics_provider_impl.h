// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_DIAGNOSTICS_PROVIDER_IMPL_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_DIAGNOSTICS_PROVIDER_IMPL_H_

#include "base/macros.h"
#include "components/signin/public/identity_manager/diagnostics_provider.h"

class GaiaCookieManagerService;
class ProfileOAuth2TokenService;

namespace signin {

// Concrete implementation of the DiagnosticsProvider interface.
class DiagnosticsProviderImpl final : public DiagnosticsProvider {
 public:
  DiagnosticsProviderImpl(
      ProfileOAuth2TokenService* profile_oauth2_token_service,
      GaiaCookieManagerService* gaia_cookie_manager_service);
  ~DiagnosticsProviderImpl() override;

  // Returns the state of the load credentials operation.
  signin::LoadCredentialsState GetDetailedStateOfLoadingOfRefreshTokens()
      const override;

  // Returns the time until a access token request can be sent (will be zero if
  // the release time is in the past).
  base::TimeDelta GetDelayBeforeMakingAccessTokenRequests() const override;

  // Returns the time until a cookie request can be sent (will be zero if the
  // release time is in the past).
  base::TimeDelta GetDelayBeforeMakingCookieRequests() const override;

 private:
  GaiaCookieManagerService* gaia_cookie_manager_service_;
  ProfileOAuth2TokenService* profile_oauth2_token_service_;

  DISALLOW_COPY_AND_ASSIGN(DiagnosticsProviderImpl);
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_DIAGNOSTICS_PROVIDER_IMPL_H_
