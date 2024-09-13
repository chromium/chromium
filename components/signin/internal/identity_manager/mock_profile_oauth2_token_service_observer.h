// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_MOCK_PROFILE_OAUTH2_TOKEN_SERVICE_OBSERVER_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_MOCK_PROFILE_OAUTH2_TOKEN_SERVICE_OBSERVER_H_

#include "base/scoped_observation.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_observer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace signin {

class MockProfileOAuth2TokenServiceObserver
    : public ProfileOAuth2TokenServiceObserver {
 public:
  explicit MockProfileOAuth2TokenServiceObserver(
      ProfileOAuth2TokenService* service);
  explicit MockProfileOAuth2TokenServiceObserver(
      ProfileOAuth2TokenServiceDelegate* delegate);
  ~MockProfileOAuth2TokenServiceObserver() override;

  MockProfileOAuth2TokenServiceObserver(
      const MockProfileOAuth2TokenServiceObserver&) = delete;
  MockProfileOAuth2TokenServiceObserver& operator=(
      const MockProfileOAuth2TokenServiceObserver&) = delete;

  MOCK_METHOD(void,
              OnRefreshTokenAvailable,
              (const CoreAccountId& account_id),
              (override));
  MOCK_METHOD(void,
              OnRefreshTokenRevoked,
              (const CoreAccountId& account_id),
              (override));
  MOCK_METHOD(void, OnRefreshTokensLoaded, (), (override));
  MOCK_METHOD(void, OnEndBatchChanges, (), (override));
  MOCK_METHOD(void,
              OnAuthErrorChanged,
              (const CoreAccountId&,
               const GoogleServiceAuthError&,
               signin_metrics::SourceForRefreshTokenOperation source),
              (override));

 private:
  base::ScopedObservation<ProfileOAuth2TokenService,
                          ProfileOAuth2TokenServiceObserver>
      token_service_observation_{this};
  base::ScopedObservation<ProfileOAuth2TokenServiceDelegate,
                          ProfileOAuth2TokenServiceObserver>
      token_service_delegate_observation_{this};
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_MOCK_PROFILE_OAUTH2_TOKEN_SERVICE_OBSERVER_H_
