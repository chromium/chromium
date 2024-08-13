// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_token_fetcher.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/check.h"
#include "base/sequence_checker.h"
#include "components/ip_protection/common/ip_protection_config_http.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_auth.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_auth_interface.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/proto/blind_sign_auth_options.pb.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/proto/spend_token_data.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/abseil-cpp/absl/status/status.h"

namespace ip_protection {

IpProtectionTokenFetcher::IpProtectionTokenFetcher(
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_url_loader_factory,
    std::unique_ptr<quiche::BlindSignAuthInterface>
        blind_sign_auth_for_testing) {
  CHECK(pending_url_loader_factory);
  url_loader_factory_ = network::SharedURLLoaderFactory::Create(
      std::move(pending_url_loader_factory));
  ip_protection_config_http_ =
      std::make_unique<ip_protection::IpProtectionConfigHttp>(
          url_loader_factory_.get());

  if (blind_sign_auth_for_testing) {
    blind_sign_auth_ = std::move(blind_sign_auth_for_testing);
    return;
  }
  privacy::ppn::BlindSignAuthOptions bsa_options{};
  bsa_options.set_enable_privacy_pass(true);

  blind_sign_auth_ = std::make_unique<quiche::BlindSignAuth>(
      ip_protection_config_http_.get(), std::move(bsa_options));
}

IpProtectionTokenFetcher::~IpProtectionTokenFetcher() = default;

void IpProtectionTokenFetcher::FetchBlindSignedToken(
    std::optional<std::string> access_token,
    uint32_t batch_size,
    quiche::ProxyLayer proxy_layer,
    FetchBlindSignedTokenCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  blind_sign_auth_->GetTokens(
      std::move(access_token), batch_size, proxy_layer,
      quiche::BlindSignAuthServiceType::kChromeIpBlinding,
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
