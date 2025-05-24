// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_probabilistic_reveal_token_mojo_fetcher.h"

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/ip_protection/common/ip_protection_core_host_remote.h"
#include "components/ip_protection/mojom/core.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ip_protection {

namespace {

class FakeCoreHost : public ip_protection::mojom::CoreHost {
 public:
  FakeCoreHost(std::vector<ProbabilisticRevealToken> tokens,
               std::string public_key,
               std::uint64_t expiration,
               std::uint64_t next_start,
               std::int32_t num_tokens_with_signal)
      : tokens_(std::move(tokens)),
        public_key_(std::move(public_key)),
        expiration_(expiration),
        next_start_(next_start),
        num_tokens_with_signal_(num_tokens_with_signal) {}

  void TryGetAuthTokens(uint32_t batch_size,
                        ip_protection::ProxyLayer proxy_layer,
                        TryGetAuthTokensCallback callback) override {
    NOTREACHED();
  }

  void GetProxyConfig(GetProxyConfigCallback callback) override {
    NOTREACHED();
  }

  void TryGetProbabilisticRevealTokens(
      TryGetProbabilisticRevealTokensCallback callback) override {
    if (tokens_.size() == 0) {
      std::move(callback).Run(
          {}, ip_protection::TryGetProbabilisticRevealTokensResult{
                  ip_protection::TryGetProbabilisticRevealTokensStatus::
                      kNullResponse,
                  static_cast<int32_t>(net::OK)});
      return;
    }
    TryGetProbabilisticRevealTokensOutcome outcome;
    outcome.tokens = tokens_;
    outcome.public_key = public_key_;
    outcome.expiration_time_seconds = expiration_;
    outcome.next_epoch_start_time_seconds = next_start_;
    outcome.num_tokens_with_signal = num_tokens_with_signal_;
    std::move(callback).Run(
        {std::move(outcome)},
        ip_protection::TryGetProbabilisticRevealTokensResult{
            ip_protection::TryGetProbabilisticRevealTokensStatus::kSuccess,
            static_cast<int32_t>(net::OK)});
  }

 private:
  std::vector<ProbabilisticRevealToken> tokens_;
  std::string public_key_;
  std::uint64_t expiration_;
  std::uint64_t next_start_;
  std::int32_t num_tokens_with_signal_;
};

}  // namespace

TEST(IpProtectionProbabilisticRevealTokenMojoFetcherTest, Success) {
  base::test::TaskEnvironment task_environment;
  std::unique_ptr<FakeCoreHost> core_host;
  {
    std::vector<ProbabilisticRevealToken> tokens;
    for (int i = 0; i < 10; ++i) {
      tokens.emplace_back(3, "u", "e");
    }
    std::string public_key = "y";
    core_host = std::make_unique<FakeCoreHost>(std::move(tokens),
                                               std::move(public_key), 3, 7, 11);
  }

  mojo::Receiver<mojom::CoreHost> receiver{core_host.get()};
  auto core_host_remote = base::MakeRefCounted<IpProtectionCoreHostRemote>(
      receiver.BindNewPipeAndPassRemote());
  IpProtectionProbabilisticRevealTokenMojoFetcher fetcher(
      core_host_remote.get());
  base::test::TestFuture<std::optional<TryGetProbabilisticRevealTokensOutcome>,
                         TryGetProbabilisticRevealTokensResult>
      future;
  fetcher.TryGetProbabilisticRevealTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  EXPECT_TRUE(future.Get<0>());
  const auto& outcome = future.Get<0>().value();
  const auto& tokens = outcome.tokens;
  EXPECT_THAT(tokens, testing::SizeIs(10));
  EXPECT_EQ(tokens[7].version, 3);
  EXPECT_EQ(tokens[7].u, "u");
  EXPECT_EQ(tokens[7].e, "e");
  EXPECT_EQ(outcome.public_key, "y");
  EXPECT_EQ(outcome.expiration_time_seconds, uint64_t(3));
  EXPECT_EQ(outcome.next_epoch_start_time_seconds, uint64_t(7));
  EXPECT_EQ(outcome.num_tokens_with_signal, int32_t(11));

  const auto& result = future.Get<1>();
  EXPECT_EQ(result.status, TryGetProbabilisticRevealTokensStatus::kSuccess);
  EXPECT_EQ(result.network_error_code, net::OK);
  EXPECT_EQ(result.try_again_after, std::nullopt);
}

TEST(IpProtectionProbabilisticRevealTokenMojoFetcherTest, NullOutcome) {
  base::test::TaskEnvironment task_environment;
  // Empty tokens will result null outcome in FakeCoreHost.
  std::vector<ProbabilisticRevealToken> tokens;
  std::string public_key = "";
  FakeCoreHost core_host(std::move(tokens), std::move(public_key), 1, 1, 1);

  mojo::Receiver<mojom::CoreHost> receiver{&core_host};
  auto core_host_remote = base::MakeRefCounted<IpProtectionCoreHostRemote>(
      receiver.BindNewPipeAndPassRemote());
  IpProtectionProbabilisticRevealTokenMojoFetcher fetcher(
      core_host_remote.get());
  base::test::TestFuture<std::optional<TryGetProbabilisticRevealTokensOutcome>,
                         TryGetProbabilisticRevealTokensResult>
      future;
  fetcher.TryGetProbabilisticRevealTokens(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  ASSERT_FALSE(future.Get<0>());

  const auto& result = future.Get<1>();
  EXPECT_EQ(result.status,
            TryGetProbabilisticRevealTokensStatus::kNullResponse);
  EXPECT_EQ(result.network_error_code, net::OK);
  EXPECT_EQ(result.try_again_after, std::nullopt);
}

}  // namespace ip_protection
