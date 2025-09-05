// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/receiver/boca_device_auth_token_service.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/notimplemented.h"
#include "base/sequence_checker.h"
#include "base/time/clock.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/auth_service_observer.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"

namespace ash::boca_receiver {

BocaDeviceAuthTokenServiceBase::BocaDeviceAuthTokenServiceBase(
    const OAuth2AccessTokenManager::ScopeSet& scopes,
    std::string_view requester_id,
    base::Clock* clock)
    : OAuth2AccessTokenManager::Consumer(std::string(requester_id)),
      scopes_(scopes),
      clock_(*clock) {}

BocaDeviceAuthTokenServiceBase::~BocaDeviceAuthTokenServiceBase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void BocaDeviceAuthTokenServiceBase::AddObserver(
    google_apis::AuthServiceObserver* observer) {
  // Currently the only consumer `google_apis::RequestSender` does not need this
  // method.
  NOTIMPLEMENTED_LOG_ONCE();
}

void BocaDeviceAuthTokenServiceBase::RemoveObserver(
    google_apis::AuthServiceObserver* observer) {
  // Currently the only consumer `google_apis::RequestSender` does not need this
  // method.
  NOTIMPLEMENTED_LOG_ONCE();
}

void BocaDeviceAuthTokenServiceBase::StartAuthentication(
    google_apis::AuthStatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  callback_ = std::move(callback);
  request_ = StartAccessTokenRequest();
}

bool BocaDeviceAuthTokenServiceBase::HasAccessToken() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return access_token_data_.has_value() &&
         access_token_data_->expiration_time > clock_->Now();
}

bool BocaDeviceAuthTokenServiceBase::HasRefreshToken() const {
  return RefreshTokenIsAvailable();
}

const std::string& BocaDeviceAuthTokenServiceBase::access_token() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static const std::string kEmptyToken = "";
  return HasAccessToken() ? access_token_data_->access_token : kEmptyToken;
}

void BocaDeviceAuthTokenServiceBase::ClearAccessToken() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  InvalidateAccessToken();
  access_token_data_.reset();
}

void BocaDeviceAuthTokenServiceBase::ClearRefreshToken() {
  // Currently the only consumer `google_apis::RequestSender` does not need this
  // method.
  NOTIMPLEMENTED_LOG_ONCE();
}

void BocaDeviceAuthTokenServiceBase::OnGetTokenSuccess(
    const OAuth2AccessTokenManager::Request* request,
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  request_.reset();
  access_token_data_ = {token_response.access_token,
                        token_response.expiration_time};
  std::move(callback_).Run(google_apis::HTTP_SUCCESS,
                           access_token_data_->access_token);
}

void BocaDeviceAuthTokenServiceBase::OnGetTokenFailure(
    const OAuth2AccessTokenManager::Request* request,
    const GoogleServiceAuthError& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  request_.reset();
  if (error.state() == GoogleServiceAuthError::CONNECTION_FAILED) {
    std::move(callback_).Run(google_apis::NO_CONNECTION, std::string());
    return;
  }
  std::move(callback_).Run(google_apis::HTTP_UNAUTHORIZED, std::string());
}

const OAuth2AccessTokenManager::ScopeSet&
BocaDeviceAuthTokenServiceBase::scopes() const {
  return scopes_;
}

}  // namespace ash::boca_receiver
