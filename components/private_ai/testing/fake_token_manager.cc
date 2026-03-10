// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/testing/fake_token_manager.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/test/run_until.h"
#include "base/time/time.h"

namespace private_ai {

const char FakeTokenManager::kFakeToken[] = "dGVzdF90b2tlbg==";
const char FakeTokenManager::kFakeProxyToken[] = "cHJveHlfdG9rZW4=";

FakeTokenManager::FakeTokenManager() = default;
FakeTokenManager::~FakeTokenManager() = default;

void FakeTokenManager::GetAuthToken(GetAuthTokenCallback callback) {
  CHECK(!callback_future_.IsReady());
  callback_future_.SetValue(std::move(callback));
}

void FakeTokenManager::PrefetchAuthTokens() {}

void FakeTokenManager::GetAuthTokenForProxy(GetAuthTokenCallback callback) {
  CHECK(!proxy_callback_future_.IsReady());
  proxy_callback_future_.SetValue(std::move(callback));
}

void FakeTokenManager::PrefetchAuthTokensForProxy() {}

void FakeTokenManager::SetReturnToken(bool return_token) {
  return_token_ = return_token;
}

void FakeTokenManager::RunPendingCallbacks() {
  callback_future_.Take().Run(GetToken());
}

void FakeTokenManager::RunPendingProxyCallbacks() {
    std::optional<phosphor::BlindSignedAuthToken> token;
    if (return_token_) {
      token = phosphor::BlindSignedAuthToken{
          .token = kFakeProxyToken,
          .encoded_extensions = "cHJveHlfZXh0ZW5zaW9ucw==",
          .expiration = base::Time::Now() + base::Minutes(1)};
    }
    proxy_callback_future_.Take().Run(std::move(token));
}

void FakeTokenManager::RespondToGetAuthToken(
    std::optional<phosphor::BlindSignedAuthToken> token) {
  callback_future_.Take().Run(token);
}

void FakeTokenManager::RespondToGetAuthTokenForProxy(
    std::optional<phosphor::BlindSignedAuthToken> token) {
  proxy_callback_future_.Take().Run(std::move(token));
}

void FakeTokenManager::OnAccountStatusChanged(bool available) {}

PrivateAiLogger* FakeTokenManager::GetLogger() {
  return &logger_;
}

std::optional<phosphor::BlindSignedAuthToken> FakeTokenManager::GetToken() {
  if (!return_token_) {
    return std::nullopt;
  }
  return phosphor::BlindSignedAuthToken{
      .token = kFakeToken,
      .encoded_extensions = "dGVzdF9leHRlbnNpb25z",
      .expiration = base::Time::Now() + base::Minutes(1)};
}

}  // namespace private_ai
