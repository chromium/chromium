// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_TOKEN_MOJO_FETCHER_H_
#define COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_TOKEN_MOJO_FETCHER_H_

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "components/ip_protection/common/ip_protection_config_getter.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/ip_protection_token_fetcher.h"

namespace ip_protection {

// Manages fetching tokens via Mojo. This is a simple wrapper around a config
// getter.
class IpProtectionTokenMojoFetcher : public IpProtectionTokenFetcher {
 public:
  explicit IpProtectionTokenMojoFetcher(
      scoped_refptr<IpProtectionConfigGetter> config_getter);
  ~IpProtectionTokenMojoFetcher() override;

  // IpProtectionTokenFetcher implementation:
  void TryGetAuthTokens(uint32_t batch_size,
                        ProxyLayer proxy_layer,
                        TryGetAuthTokensCallback callback) override;

 private:
  scoped_refptr<IpProtectionConfigGetter> config_getter_;
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_TOKEN_MOJO_FETCHER_H_
