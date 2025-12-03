// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/phosphor/mock_blind_sign_auth.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/notreached.h"
#include "base/threading/platform_thread.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_auth_interface.h"
#include "third_party/abseil-cpp/absl/types/span.h"

namespace legion::phosphor {

MockBlindSignAuth::MockBlindSignAuth() = default;

MockBlindSignAuth::~MockBlindSignAuth() = default;

void MockBlindSignAuth::GetTokens(
    std::optional<std::string> oauth_token,
    int num_tokens,
    quiche::ProxyLayer proxy_layer,
    quiche::BlindSignAuthServiceType /*service_type*/,
    quiche::SignedTokenCallback callback) {
  get_tokens_called_ = true;
  last_thread_id_ = base::PlatformThread::CurrentId();
  oauth_token_ = oauth_token ? *oauth_token : "";
  num_tokens_ = num_tokens;
  proxy_layer_ = proxy_layer;

  if (status_.ok()) {
    std::move(callback)(absl::Span<quiche::BlindSignToken>(tokens_));
  } else {
    std::move(callback)(status_);
  }
}

void MockBlindSignAuth::GetAttestationTokens(
    int num_tokens,
    quiche::ProxyLayer layer,
    quiche::AttestationDataCallback attestation_data_callback,
    quiche::SignedTokenCallback token_callback) {
  NOTREACHED() << "Not implemented";
}

bool MockBlindSignAuth::GetTokensCalledInDifferentThread() {
  return get_tokens_called_ &&
         last_thread_id_ != base::PlatformThread::CurrentId();
}

}  // namespace legion::phosphor
