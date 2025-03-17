// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_proxy_config_mojo_fetcher.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/ip_protection/common/ip_protection_core_host_remote.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/mojom/core.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/proxy_chain.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ip_protection {

namespace {

class FakeCoreHost : public ip_protection::mojom::CoreHost {
  void TryGetAuthTokens(uint32_t batch_size,
                        ip_protection::ProxyLayer proxy_layer,
                        TryGetAuthTokensCallback callback) override {
    NOTREACHED();
  }

  void GetProxyConfig(GetProxyConfigCallback callback) override {
    std::move(callback).Run(std::nullopt, std::nullopt);
  }

  void TryGetProbabilisticRevealTokens(
      TryGetProbabilisticRevealTokensCallback callback) override {
    NOTREACHED();
  }
};

TEST(IpProtectionProxyConfigMojoFetcherTest, CallsThrough) {
  base::test::TaskEnvironment task_environment;
  FakeCoreHost fake_core_host;
  mojo::Receiver<ip_protection::mojom::CoreHost> receiver{&fake_core_host};
  auto core_host_remote = base::MakeRefCounted<IpProtectionCoreHostRemote>(
      receiver.BindNewPipeAndPassRemote());
  IpProtectionProxyConfigMojoFetcher fetcher(core_host_remote.get());
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
