// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_token_mojo_fetcher.h"

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
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
    std::move(callback).Run(std::nullopt, std::nullopt);
  }

  void GetProxyConfig(GetProxyConfigCallback callback) override {
    NOTREACHED();
  }

 protected:
  ~MockIpProtectionConfigGetter() override = default;
};

TEST(IpProtectionTokenMojoFetcherTest, CallsThrough) {
  auto getter = base::MakeRefCounted<MockIpProtectionConfigGetter>();
  IpProtectionTokenMojoFetcher fetcher(getter.get());
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
