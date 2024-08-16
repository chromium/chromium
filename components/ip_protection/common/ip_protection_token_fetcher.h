// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_TOKEN_FETCHER_H_
#define COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_TOKEN_FETCHER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "components/ip_protection/common/ip_protection_config_http.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/abseil-cpp/absl/status/status.h"

namespace quiche {
class BlindSignAuthInterface;
enum class ProxyLayer;
struct BlindSignToken;
}  // namespace quiche

namespace ip_protection {

// Manages requesting and fetching blind-signed authentication tokens for IP
// Protection using the `quiche::BlindSignAuth` library.
class IpProtectionTokenFetcher {
 public:
  using FetchBlindSignedTokenCallback = base::OnceCallback<void(
      absl::StatusOr<std::vector<quiche::BlindSignToken>>)>;

  explicit IpProtectionTokenFetcher(
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          url_loader_factory,
      std::unique_ptr<quiche::BlindSignAuthInterface>
          blind_sign_auth_for_testing = nullptr);

  ~IpProtectionTokenFetcher();

  // `FetchBlindSignedToken()` calls into the `quiche::BlindSignAuth` library to
  // request a blind-signed auth token for use at the IP Protection proxies.
  void FetchBlindSignedToken(std::optional<std::string> access_token,
                             uint32_t batch_size,
                             quiche::ProxyLayer proxy_layer,
                             FetchBlindSignedTokenCallback callback);

 private:
  // The BlindSignAuth implementation used to fetch blind-signed auth tokens. A
  // raw pointer to `url_loader_factory_` gets passed to
  // `ip_protection_config_http_`, so we ensure it stays alive by storing its
  // scoped_refptr here.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<ip_protection::IpProtectionConfigHttp>
      ip_protection_config_http_;
  std::unique_ptr<quiche::BlindSignAuthInterface> blind_sign_auth_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_TOKEN_FETCHER_H_
