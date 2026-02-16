// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEGION_TESTING_FAKE_TOKEN_MANAGER_H_
#define COMPONENTS_LEGION_TESTING_FAKE_TOKEN_MANAGER_H_

#include <deque>
#include <optional>

#include "base/functional/callback.h"
#include "components/legion/phosphor/token_manager.h"

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
  size_t GetPendingCallbackCount();
  size_t GetPendingProxyCallbackCount();

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
  std::deque<GetAuthTokenCallback> pending_callbacks_;
  std::deque<GetAuthTokenCallback> pending_proxy_callbacks_;
};

}  // namespace private_ai

#endif  // COMPONENTS_LEGION_TESTING_FAKE_TOKEN_MANAGER_H_
