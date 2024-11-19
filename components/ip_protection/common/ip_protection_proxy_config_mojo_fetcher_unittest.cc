// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_proxy_config_mojo_fetcher.h"

#include <memory>
#include <optional>

#include "base/memory/scoped_refptr.h"
#include "base/test/test_future.h"
#include "components/ip_protection/common/ip_protection_config_getter.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ip_protection {

namespace {

class MockIpProtectionConfigGetter : public IpProtectionConfigGetter {
 public:
  bool IsAvailable() override { return true; }

  void TryGetAuthTokens(uint32_t batch_size,
                        ProxyLayer proxy_layer,
                        TryGetAuthTokensCallback callback) override {
    NOTREACHED();
  }

  void GetProxyConfig(GetProxyConfigCallback callback) override {
    std::move(callback).Run(std::nullopt, std::nullopt);
  }

 protected:
  ~MockIpProtectionConfigGetter() override = default;
};

TEST(IpProtectionProxyConfigMojoFetcherTest, CallsThrough) {
  auto getter = base::MakeRefCounted<MockIpProtectionConfigGetter>();
  IpProtectionProxyConfigMojoFetcher fetcher(getter.get());
  base::test::TestFuture<const std::optional<std::vector<::net::ProxyChain>>,
                         const std::optional<GeoHint>>
      future;
  fetcher.GetProxyConfig(future.GetCallback());
  auto [proxy_chain, geo_hint] = future.Get();
  ASSERT_EQ(proxy_chain, std::nullopt);
  ASSERT_EQ(geo_hint, std::nullopt);
}

}  // namespace
}  // namespace ip_protection
