// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_RECEIVER_BOCA_DEVICE_AUTH_TOKEN_SERVICE_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_RECEIVER_BOCA_DEVICE_AUTH_TOKEN_SERVICE_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "google_apis/common/auth_service_interface.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"

class GoogleServiceAuthError;

namespace base {
class Time;
}  // namespace base

namespace google_apis {
class AuthServiceObserver;
}  // namespace google_apis

namespace ash::boca_receiver {

class BocaDeviceAuthTokenServiceBase
    : public google_apis::AuthServiceInterface,
      public OAuth2AccessTokenManager::Consumer {
 public:
  BocaDeviceAuthTokenServiceBase(const BocaDeviceAuthTokenServiceBase&) =
      delete;
  BocaDeviceAuthTokenServiceBase& operator=(
      const BocaDeviceAuthTokenServiceBase&) = delete;

  ~BocaDeviceAuthTokenServiceBase() override;

  // google_apis::AuthServiceInterface:
  void AddObserver(google_apis::AuthServiceObserver* observer) override;
  void RemoveObserver(google_apis::AuthServiceObserver* observer) override;
  void StartAuthentication(google_apis::AuthStatusCallback callback) override;
  bool HasAccessToken() const override;
  bool HasRefreshToken() const override;
  const std::string& access_token() const override;
  void ClearAccessToken() override;
  void ClearRefreshToken() override;

  // OAuth2AccessTokenManager::Consumer:
  void OnGetTokenSuccess(
      const OAuth2AccessTokenManager::Request* request,
      const OAuth2AccessTokenConsumer::TokenResponse& token_response) override;
  void OnGetTokenFailure(const OAuth2AccessTokenManager::Request* request,
                         const GoogleServiceAuthError& error) override;

 protected:
  BocaDeviceAuthTokenServiceBase(
      const OAuth2AccessTokenManager::ScopeSet& scopes,
      std::string_view requester_id,
      base::Clock* clock);

  virtual std::unique_ptr<OAuth2AccessTokenManager::Request>
  StartAccessTokenRequest() = 0;

  virtual bool RefreshTokenIsAvailable() const = 0;

  virtual void InvalidateAccessToken() = 0;

  const OAuth2AccessTokenManager::ScopeSet& scopes() const;

 private:
  struct AccessTokenData {
    std::string access_token;
    base::Time expiration_time;
  };

  SEQUENCE_CHECKER(sequence_checker_);
  const OAuth2AccessTokenManager::ScopeSet scopes_;
  const raw_ref<const base::Clock> clock_;
  std::unique_ptr<OAuth2AccessTokenManager::Request> request_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::optional<AccessTokenData> access_token_data_
      GUARDED_BY_CONTEXT(sequence_checker_);
  google_apis::AuthStatusCallback callback_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

template <class T>
class BocaDeviceAuthTokenService : public BocaDeviceAuthTokenServiceBase {
 public:
  BocaDeviceAuthTokenService(
      const OAuth2AccessTokenManager::ScopeSet& scopes,
      std::string_view requester_id,
      base::Clock* clock = base::DefaultClock::GetInstance())
      : BocaDeviceAuthTokenServiceBase(scopes, requester_id, clock) {}

  BocaDeviceAuthTokenService(const BocaDeviceAuthTokenService&) = delete;
  BocaDeviceAuthTokenService& operator=(const BocaDeviceAuthTokenService&) =
      delete;

  ~BocaDeviceAuthTokenService() override = default;

 protected:
  std::unique_ptr<OAuth2AccessTokenManager::Request> StartAccessTokenRequest()
      override {
    CHECK(T::Get());
    return T::Get()->StartAccessTokenRequest(scopes(), this);
  }

  bool RefreshTokenIsAvailable() const override {
    CHECK(T::Get());
    return T::Get()->RefreshTokenIsAvailable();
  }

  void InvalidateAccessToken() override {
    CHECK(T::Get());
    T::Get()->InvalidateAccessToken(scopes(), access_token());
  }
};

}  // namespace ash::boca_receiver

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_RECEIVER_BOCA_DEVICE_AUTH_TOKEN_SERVICE_H_
