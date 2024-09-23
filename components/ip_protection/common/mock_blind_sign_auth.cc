// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/mock_blind_sign_auth.h"

#include <memory>
#include <optional>
#include <string>

#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_auth_interface.h"
#include "third_party/abseil-cpp/absl/status/statusor.h"
#include "third_party/abseil-cpp/absl/types/span.h"

namespace ip_protection {

MockBlindSignAuth::MockBlindSignAuth() = default;

MockBlindSignAuth::~MockBlindSignAuth() = default;

void MockBlindSignAuth::GetTokens(
    std::optional<std::string> oauth_token,
    int num_tokens,
    quiche::ProxyLayer proxy_layer,
    quiche::BlindSignAuthServiceType /*service_type*/,
    quiche::SignedTokenCallback callback) {
  get_tokens_called_ = true;
  oauth_token_ = oauth_token ? *oauth_token : "";
  num_tokens_ = num_tokens;
  proxy_layer_ = proxy_layer;

  absl::StatusOr<absl::Span<quiche::BlindSignToken>> result;
  if (status_.ok()) {
    result = absl::Span<quiche::BlindSignToken>(tokens_);
  } else {
    result = status_;
  }

  std::move(callback)(std::move(result));
}

}  // namespace ip_protection
