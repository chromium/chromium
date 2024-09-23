// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/mock_profile_oauth2_token_service_observer.h"

#include "base/scoped_observation.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate.h"

namespace signin {

MockProfileOAuth2TokenServiceObserver::MockProfileOAuth2TokenServiceObserver(
    ProfileOAuth2TokenService* service) {
  token_service_observation_.Observe(service);
}
MockProfileOAuth2TokenServiceObserver::MockProfileOAuth2TokenServiceObserver(
    ProfileOAuth2TokenServiceDelegate* delegate) {
  token_service_delegate_observation_.Observe(delegate);
}
MockProfileOAuth2TokenServiceObserver::
    ~MockProfileOAuth2TokenServiceObserver() = default;

}  // namespace signin
