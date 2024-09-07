// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_token_fetcher.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_auth_interface.h"
#include "third_party/abseil-cpp/absl/status/statusor.h"

namespace ip_protection {

// static
void IpProtectionTokenFetcher::GetTokensFromBlindSignAuth(
    quiche::BlindSignAuthInterface* blind_sign_auth,
    quiche::BlindSignAuthServiceType service_type,
    std::optional<std::string> access_token,
    uint32_t batch_size,
    quiche::ProxyLayer proxy_layer,
    FetchBlindSignedTokenCallback callback) {
  blind_sign_auth->GetTokens(
      std::move(access_token), batch_size, proxy_layer, service_type,
      [callback = std::move(callback)](
          absl::StatusOr<absl::Span<quiche::BlindSignToken>> tokens) mutable {
        if (tokens.ok()) {
          std::move(callback).Run(std::vector<quiche::BlindSignToken>(
              tokens->begin(), tokens->end()));
        } else {
          std::move(callback).Run(tokens.status());
        }
      });
}

}  // namespace ip_protection
