// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_TOKEN_DIRECT_FETCHER_H_
#define COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_TOKEN_DIRECT_FETCHER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "components/ip_protection/common/ip_protection_config_http.h"
#include "components/ip_protection/common/ip_protection_token_fetcher.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ip_protection {

// An implementation of IpProtectionTokenFetcher that uses HTTP fetching in
// the `quiche::BlindSignAuth` library for retrieving blind-signed
// authentication tokens for IP Protection.
class IpProtectionTokenDirectFetcher : public IpProtectionTokenFetcher {
 public:
  explicit IpProtectionTokenDirectFetcher(
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          url_loader_factory,
      std::unique_ptr<quiche::BlindSignAuthInterface>
          blind_sign_auth_for_testing = nullptr);

  ~IpProtectionTokenDirectFetcher() override;

  // IpProtectionTokenFetcher implementation:
  void FetchBlindSignedToken(std::optional<std::string> access_token,
                             uint32_t batch_size,
                             quiche::ProxyLayer proxy_layer,
                             FetchBlindSignedTokenCallback callback) override;

 private:
  // The BlindSignAuth implementation used to fetch blind-signed auth tokens. A
  // raw pointer to `url_loader_factory_` gets passed to
  // `ip_protection_config_http_`, so we ensure it stays alive by storing its
  // scoped_refptr here.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<IpProtectionConfigHttp> ip_protection_config_http_;
  std::unique_ptr<quiche::BlindSignAuthInterface> blind_sign_auth_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_TOKEN_DIRECT_FETCHER_H_
