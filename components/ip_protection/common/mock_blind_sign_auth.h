// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_MOCK_BLIND_SIGN_AUTH_H_
#define COMPONENTS_IP_PROTECTION_COMMON_MOCK_BLIND_SIGN_AUTH_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_auth_interface.h"
#include "third_party/abseil-cpp/absl/status/status.h"

namespace ip_protection {

// A mock implementation of the quiche BlindSignAuth library to fetch
// BlindSignTokens for tests.
class MockBlindSignAuth : public quiche::BlindSignAuthInterface {
 public:
  explicit MockBlindSignAuth();
  ~MockBlindSignAuth() override;

  // BlindSignAuthInterface implementation:
  void GetTokens(std::optional<std::string> oauth_token,
                 int num_tokens,
                 quiche::ProxyLayer proxy_layer,
                 quiche::BlindSignAuthServiceType /*service_type*/,
                 quiche::SignedTokenCallback callback) override;

  void set_tokens(std::vector<quiche::BlindSignToken> tokens) {
    tokens_ = std::move(tokens);
  }

  void set_status(absl::Status status) { status_ = std::move(status); }

  bool get_tokens_called() const { return get_tokens_called_; }

  std::optional<std::string> oauth_token() const {
    return oauth_token_.empty() ? std::nullopt
                                : std::optional<std::string>{oauth_token_};
  }

  int num_tokens() const { return num_tokens_; }

  quiche::ProxyLayer proxy_layer() const { return proxy_layer_; }

  const absl::Status& status() const { return status_; }

  const std::vector<quiche::BlindSignToken>& tokens() const { return tokens_; }

 private:
  // True if `GetTokens()` was called.
  bool get_tokens_called_ = false;

  // The token with which `GetTokens()` was called.
  std::string oauth_token_ = "";

  // The num_tokens with which `GetTokens()` was called.
  int num_tokens_ = 0;

  // The proxy for which the tokens are intended for.
  quiche::ProxyLayer proxy_layer_ = quiche::ProxyLayer::kProxyA;

  // If not Ok, the status that will be returned from `GetTokens()`.
  absl::Status status_ = absl::OkStatus();

  // The tokens that will be returned from `GetTokens()` , if `status_` is not
  // `OkStatus`.
  std::vector<quiche::BlindSignToken> tokens_;
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_MOCK_BLIND_SIGN_AUTH_H_
