// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_TESTING_FAKE_TOKEN_MANAGER_H_
#define COMPONENTS_PRIVATE_AI_TESTING_FAKE_TOKEN_MANAGER_H_

#include <deque>
#include <optional>

#include "base/functional/callback.h"
#include "base/test/test_future.h"
#include "components/private_ai/phosphor/token_manager.h"

namespace private_ai {

class FakeTokenManager : public phosphor::TokenManager {
 public:
  FakeTokenManager();
  ~FakeTokenManager() override;

  // phosphor::TokenManager:
  void GetAuthToken(GetAuthTokenCallback callback) override;
  void PrefetchAuthTokens() override;
  void GetAuthTokenForProxy(GetAuthTokenCallback callback) override;
  void PrefetchAuthTokensForProxy() override;

  // Test helpers:
  void SetReturnToken(bool return_token);
  void RunPendingCallbacks();
  void RunPendingProxyCallbacks();

  // An alternative to RunPending*Callbacks that allows custom token values.
  void RespondToGetAuthToken(
      std::optional<phosphor::BlindSignedAuthToken> token);
  void RespondToGetAuthTokenForProxy(
      std::optional<phosphor::BlindSignedAuthToken> token);

  static const char kFakeToken[];
  static const char kFakeProxyToken[];

 private:
  std::optional<phosphor::BlindSignedAuthToken> GetToken();

  bool return_token_ = true;
  base::test::TestFuture<GetAuthTokenCallback> callback_future_;
  base::test::TestFuture<GetAuthTokenCallback> proxy_callback_future_;
};

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_TESTING_FAKE_TOKEN_MANAGER_H_
