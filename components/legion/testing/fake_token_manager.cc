// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/testing/fake_token_manager.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/time/time.h"

namespace private_ai {

const char FakeTokenManager::kFakeToken[] = "test_token";
const char FakeTokenManager::kFakeProxyToken[] = "proxy_token";

FakeTokenManager::FakeTokenManager() = default;
FakeTokenManager::~FakeTokenManager() = default;

void FakeTokenManager::GetAuthToken(GetAuthTokenCallback callback) {
  pending_callbacks_.push_back(std::move(callback));
}

void FakeTokenManager::PrefetchAuthTokens() {}

void FakeTokenManager::GetAuthTokenForProxy(GetAuthTokenCallback callback) {
  pending_proxy_callbacks_.push_back(std::move(callback));
}

void FakeTokenManager::PrefetchAuthTokensForProxy() {}

void FakeTokenManager::SetReturnToken(bool return_token) {
  return_token_ = return_token;
}

void FakeTokenManager::RunPendingCallbacks() {
  while (!pending_callbacks_.empty()) {
    std::move(pending_callbacks_.front()).Run(GetToken());
    pending_callbacks_.pop_front();
  }
}

void FakeTokenManager::RunPendingProxyCallbacks() {
  while (!pending_proxy_callbacks_.empty()) {
    std::optional<phosphor::BlindSignedAuthToken> token;
    if (return_token_) {
      token = phosphor::BlindSignedAuthToken{
          .token = kFakeProxyToken,
          .encoded_extensions = "proxy_extensions",
          .expiration = base::Time::Now() + base::Minutes(1)};
    }
    std::move(pending_proxy_callbacks_.front()).Run(std::move(token));
    pending_proxy_callbacks_.pop_front();
  }
}

size_t FakeTokenManager::GetPendingCallbackCount() {
  return pending_callbacks_.size();
}

size_t FakeTokenManager::GetPendingProxyCallbackCount() {
  return pending_proxy_callbacks_.size();
}

void FakeTokenManager::RespondToGetAuthToken(
    std::optional<phosphor::BlindSignedAuthToken> token) {
  CHECK(!pending_callbacks_.empty());
  std::move(pending_callbacks_.front()).Run(std::move(token));
  pending_callbacks_.pop_front();
}

void FakeTokenManager::RespondToGetAuthTokenForProxy(
    std::optional<phosphor::BlindSignedAuthToken> token) {
  CHECK(!pending_proxy_callbacks_.empty());
  std::move(pending_proxy_callbacks_.front()).Run(std::move(token));
  pending_proxy_callbacks_.pop_front();
}

std::optional<phosphor::BlindSignedAuthToken> FakeTokenManager::GetToken() {
  if (!return_token_) {
    return std::nullopt;
  }
  return phosphor::BlindSignedAuthToken{
      .token = kFakeToken,
      .encoded_extensions = "test_extensions",
      .expiration = base::Time::Now() + base::Minutes(1)};
}

}  // namespace private_ai
