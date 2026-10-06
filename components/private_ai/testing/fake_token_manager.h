// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_TESTING_FAKE_TOKEN_MANAGER_H_
#define COMPONENTS_PRIVATE_AI_TESTING_FAKE_TOKEN_MANAGER_H_

#include "base/functional/callback.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "components/private_ai/common/private_ai_logger.h"
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
      base::expected<phosphor::BlindSignedAuthToken, Error> token);
  void RespondToGetAuthTokenForProxy(
      base::expected<phosphor::BlindSignedAuthToken, Error> token);

  void OnAccountStatusChanged(bool available) override;

  static const char kFakeToken[];
  static const char kFakeProxyToken[];

 private:
  base::expected<phosphor::BlindSignedAuthToken, Error> GetToken();

  bool return_token_ = true;
  base::test::TestFuture<GetAuthTokenCallback> callback_future_;
  base::test::TestFuture<GetAuthTokenCallback> proxy_callback_future_;

  PrivateAiLogger logger_;
};

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_TESTING_FAKE_TOKEN_MANAGER_H_
