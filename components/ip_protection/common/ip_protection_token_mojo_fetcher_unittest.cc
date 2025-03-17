// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_token_mojo_fetcher.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/ip_protection/common/ip_protection_core_host_remote.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/mojom/core.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ip_protection {

namespace {

class FakeCoreHost : public ip_protection::mojom::CoreHost {
  void TryGetAuthTokens(uint32_t batch_size,
                        ip_protection::ProxyLayer proxy_layer,
                        TryGetAuthTokensCallback callback) override {
    std::move(callback).Run(std::nullopt, std::nullopt);
  }

  void GetProxyConfig(GetProxyConfigCallback callback) override {
    NOTREACHED();
  }

  void TryGetProbabilisticRevealTokens(
      TryGetProbabilisticRevealTokensCallback callback) override {
    NOTREACHED();
  }
};

TEST(IpProtectionTokenMojoFetcherTest, CallsThrough) {
  base::test::TaskEnvironment task_environment;
  FakeCoreHost fake_core_host;
  mojo::Receiver<ip_protection::mojom::CoreHost> receiver{&fake_core_host};
  auto core_host_remote = base::MakeRefCounted<IpProtectionCoreHostRemote>(
      receiver.BindNewPipeAndPassRemote());
  IpProtectionTokenMojoFetcher fetcher(core_host_remote.get());
  base::test::TestFuture<std::optional<std::vector<BlindSignedAuthToken>>,
                         std::optional<::base::Time>>
      future;
  fetcher.TryGetAuthTokens(10, ProxyLayer::kProxyA, future.GetCallback());
  auto [tokens, backoff] = future.Get();
  ASSERT_EQ(tokens, std::nullopt);
  ASSERT_EQ(backoff, std::nullopt);
}

}  // namespace

}  // namespace ip_protection
