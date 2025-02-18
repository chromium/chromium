// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_probabilistic_reveal_token_manager.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/ip_protection/common/ip_protection_probabilistic_reveal_token_crypter.h"
#include "components/ip_protection/common/ip_protection_probabilistic_reveal_token_fetcher.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/private-join-and-compute/src/crypto/context.h"
#include "third_party/private-join-and-compute/src/crypto/ec_group.h"
#include "third_party/private-join-and-compute/src/crypto/ec_point.h"
#include "third_party/private-join-and-compute/src/crypto/elgamal.h"

namespace ip_protection {

namespace {

using ::private_join_and_compute::Context;
using ::private_join_and_compute::ECGroup;
using ::private_join_and_compute::ECPoint;
using ::private_join_and_compute::ElGamalDecrypter;
using ::private_join_and_compute::ElGamalEncrypter;
using ::private_join_and_compute::elgamal::Ciphertext;
using ::private_join_and_compute::elgamal::PrivateKey;
using ::private_join_and_compute::elgamal::PublicKey;

// Mocks PRT issuer server capabilities, used to create/decrypt tokens for
// tests.
class MockIssuer {
 public:
  static absl::StatusOr<std::unique_ptr<MockIssuer>> Create(
      uint64_t private_key,
      size_t num_tokens,
      base::Time expiration,
      base::Time next_start,
      int32_t num_tokens_with_signal) {
    auto context = std::make_unique<Context>();
    std::unique_ptr<ECGroup> group;
    {
      ASSIGN_OR_RETURN(ECGroup local_group,
                       ECGroup::Create(NID_secp224r1, context.get()));
      group = std::make_unique<ECGroup>(std::move(local_group));
    }

    std::unique_ptr<ElGamalEncrypter> encrypter;
    std::string serialized_public_key;
    {
      ASSIGN_OR_RETURN(ECPoint g, group->GetFixedGenerator());
      ASSIGN_OR_RETURN(ECPoint y, g.Mul(context->CreateBigNum(private_key)));
      ASSIGN_OR_RETURN(serialized_public_key, y.ToBytesCompressed());
      encrypter = std::make_unique<ElGamalEncrypter>(
          group.get(),
          std::make_unique<PublicKey>(PublicKey{std::move(g), std::move(y)}));
    }

    auto decrypter =
        std::make_unique<ElGamalDecrypter>(std::make_unique<PrivateKey>(
            PrivateKey{context->CreateBigNum(private_key)}));

    std::vector<ProbabilisticRevealToken> tokens;
    tokens.reserve(num_tokens);
    for (std::size_t i = 0; i < num_tokens; ++i) {
      ASSIGN_OR_RETURN(
          ECPoint plaintext_point,
          group->GetPointByHashingToCurveSha256(
              "awesome-probabilistic-reveal-token-" + base::NumberToString(i)));
      ASSIGN_OR_RETURN(Ciphertext ciphertext,
                       encrypter->Encrypt(plaintext_point));
      ASSIGN_OR_RETURN(std::string u_compressed,
                       ciphertext.u.ToBytesCompressed());
      ASSIGN_OR_RETURN(std::string e_compressed,
                       ciphertext.e.ToBytesCompressed());
      tokens.emplace_back(1, std::move(u_compressed), std::move(e_compressed));
    }
    return base::WrapUnique<MockIssuer>(new MockIssuer(
        std::move(context), std::move(group), std::move(encrypter),
        std::move(decrypter), std::move(serialized_public_key),
        std::move(tokens), expiration, next_start, num_tokens_with_signal));
  }

  const std::vector<ProbabilisticRevealToken>& Tokens() const {
    return tokens_;
  }

  void SetTokens(std::vector<ProbabilisticRevealToken> tokens) {
    tokens_ = std::move(tokens);
  }

  std::string GetSerializedPublicKey() const { return serialized_public_key_; }

  // Decrypt given token, serialize returned point, and base64 encode.
  absl::StatusOr<std::string> DecryptSerializeEncode(
      const ProbabilisticRevealToken& token) {
    ASSIGN_OR_RETURN(ECPoint u, group_->CreateECPoint(token.u));
    ASSIGN_OR_RETURN(ECPoint e, group_->CreateECPoint(token.e));
    Ciphertext ciphertext{std::move(u), std::move(e)};
    ASSIGN_OR_RETURN(ECPoint point, decrypter_->Decrypt(ciphertext));
    ASSIGN_OR_RETURN(std::string serialized_point, point.ToBytesCompressed());
    return base::Base64Encode(serialized_point);
  }

  absl::StatusOr<std::vector<std::string>> DecryptSerializeEncode(
      const std::vector<ProbabilisticRevealToken>& tokens) {
    std::vector<std::string> encoded;
    for (const auto& t : tokens) {
      ASSIGN_OR_RETURN(std::string sp, DecryptSerializeEncode(t));
      encoded.push_back(std::move(sp));
    }
    return encoded;
  }

  base::Time Expiration() const { return expiration_; }
  base::Time NextStart() const { return next_start_; }
  int32_t NumTokensWithSignal() const { return num_tokens_with_signal_; }

 private:
  MockIssuer(std::unique_ptr<Context> context,
             std::unique_ptr<ECGroup> group,
             std::unique_ptr<ElGamalEncrypter> encrypter,
             std::unique_ptr<ElGamalDecrypter> decrypter,
             std::string serialized_public_key,
             std::vector<ProbabilisticRevealToken> tokens,
             base::Time expiration,
             base::Time next_start,
             int32_t num_tokens_with_signal)
      : context_(std::move(context)),
        group_(std::move(group)),
        encrypter_(std::move(encrypter)),
        decrypter_(std::move(decrypter)),
        serialized_public_key_(std::move(serialized_public_key)),
        tokens_(std::move(tokens)),
        expiration_(expiration),
        next_start_(next_start),
        num_tokens_with_signal_(num_tokens_with_signal) {}
  std::unique_ptr<const Context> context_;
  std::unique_ptr<const ECGroup> group_;
  std::unique_ptr<const ElGamalEncrypter> encrypter_;
  std::unique_ptr<const ElGamalDecrypter> decrypter_;
  const std::string serialized_public_key_;
  std::vector<ProbabilisticRevealToken> tokens_;
  const base::Time expiration_;
  const base::Time next_start_;
  const int32_t num_tokens_with_signal_;
};

// Mocks a PRT fetcher. Uses MockIssuer for successful fetches with valid tokens
// and SetResponse to mock error results.
class MockFetcher : public IpProtectionProbabilisticRevealTokenFetcher {
 public:
  MockFetcher() = default;
  void TryGetProbabilisticRevealTokens(
      TryGetProbabilisticRevealTokensCallback callback) override {
    num_calls_++;
    std::move(callback).Run(outcome_, result_);
  }

  // Set fetcher response, used for null outcomes.
  void SetResponse(
      std::optional<TryGetProbabilisticRevealTokensOutcome> outcome,
      TryGetProbabilisticRevealTokensResult result) {
    outcome_ = std::move(outcome);
    result_ = std::move(result);
  }

  // Set issuer and fetcher response, used to get a valid outcome with valid
  // tokens.
  absl::Status SetIssuer(uint64_t private_key,
                         size_t num_tokens,
                         base::Time expiration,
                         base::Time next_start,
                         int32_t num_tokens_with_signal) {
    ASSIGN_OR_RETURN(issuer_,
                     MockIssuer::Create(private_key, num_tokens, expiration,
                                        next_start, num_tokens_with_signal));

    TryGetProbabilisticRevealTokensOutcome outcome;
    outcome.tokens = issuer_->Tokens();
    outcome.public_key = issuer_->GetSerializedPublicKey();
    outcome.expiration_time_seconds = expiration.InSecondsFSinceUnixEpoch();
    outcome.next_epoch_start_time_seconds =
        next_start.InSecondsFSinceUnixEpoch();
    outcome.num_tokens_with_signal = num_tokens_with_signal;
    SetResponse({std::move(outcome)},
                TryGetProbabilisticRevealTokensResult{
                    TryGetProbabilisticRevealTokensStatus::kSuccess, net::OK,
                    std::nullopt});
    return absl::OkStatus();
  }

  size_t NumCalls() const { return num_calls_; }

  MockIssuer* Issuer() { return issuer_.get(); }

 private:
  std::optional<TryGetProbabilisticRevealTokensOutcome> outcome_;
  TryGetProbabilisticRevealTokensResult result_;
  size_t num_calls_ = 0;
  std::unique_ptr<MockIssuer> issuer_;
};

}  // namespace

class IpProtectionProbabilisticRevealTokenManagerTest : public testing::Test {
 protected:
  // SetUp enables ip protection.
  void SetUp() override {
    // Advance clock to some arbitrary time.
    task_environment_.AdvanceClock(base::Time::UnixEpoch() + base::Days(9876) -
                                   base::Time::Now());
    fetcher_ = std::make_unique<MockFetcher>();
    auto status =
        fetcher_->SetIssuer(/*private_key=*/12345,
                            /*num_tokens=*/27,
                            /*expiration=*/base::Time::Now() + base::Hours(8),
                            /*next_start=*/base::Time::Now() + base::Hours(4),
                            /*num_tokens_with_signal=*/7);
    ASSERT_TRUE(status.ok());
    fetcher_ptr_ = fetcher_.get();
  }

 public:
  // Set fetcher response, used for error status/outcomes.
  void SetResponse(
      std::optional<TryGetProbabilisticRevealTokensOutcome> outcome,
      TryGetProbabilisticRevealTokensResult result) {
    fetcher_ptr_->SetResponse(std::move(outcome), std::move(result));
  }

  // Set issuer and fetcher response, used to get a valid outcome with valid
  // tokens.
  void SetIssuer(uint64_t private_key,
                 size_t num_tokens,
                 base::Time expiration,
                 base::Time next_start,
                 size_t num_tokens_with_signal) {
    auto status = fetcher_ptr_->SetIssuer(private_key, num_tokens, expiration,
                                          next_start, num_tokens_with_signal);
    ASSERT_TRUE(status.ok());
  }

  // Decrypt given token, serialize returned point, and base64 encode.
  std::string DecryptSerializeEncode(const ProbabilisticRevealToken& token) {
    auto maybe_serialized_point =
        fetcher_ptr_->Issuer()->DecryptSerializeEncode(token);
    EXPECT_TRUE(maybe_serialized_point.ok());
    return std::move(maybe_serialized_point.value());
  }

  std::vector<std::string> DecryptSerializeEncode(
      const std::vector<ProbabilisticRevealToken>& tokens) {
    auto maybe_serialized_points =
        fetcher_ptr_->Issuer()->DecryptSerializeEncode(tokens);
    EXPECT_TRUE(maybe_serialized_points.ok());
    return std::move(maybe_serialized_points.value());
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<MockFetcher> fetcher_;
  std::unique_ptr<IpProtectionProbabilisticRevealTokenManager> manager_;
  // fetcher_ is moved to create manager. `fetcher_ptr_` is a pointer
  // to fetcher to modify its behavior after it is moved.
  raw_ptr<MockFetcher> fetcher_ptr_;
};

// Test whether IsTokenAvailable() returns false and GetToken() returns null,
// when no response from the PRT issuer server is returned, i.e., crypter is
// null.
TEST_F(IpProtectionProbabilisticRevealTokenManagerTest,
       NoIssuerServerResponseNullCrypter) {
  SetResponse({},
              {TryGetProbabilisticRevealTokensStatus::kNullResponse, net::OK,
               /*try_again_after=*/std::nullopt});
  manager_ = std::make_unique<IpProtectionProbabilisticRevealTokenManager>(
      std::move(fetcher_));
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_FALSE(manager_->IsTokenAvailable());
  EXPECT_FALSE(manager_->GetToken("A", "b42"));
}

TEST_F(IpProtectionProbabilisticRevealTokenManagerTest,
       TokensExpireAsExpected) {
  const base::Time expiration = base::Time::Now() + base::Hours(8);
  const base::Time next_start = base::Time::Now() + base::Hours(4);
  SetIssuer(/*private_key=*/12345,
            /*num_tokens=*/10, expiration, next_start,
            /*num_tokens_with_signal=*/3);

  // Create manager, posts token fetch task in constructor and schedules next
  // fetch at `next_start`.
  manager_ = std::make_unique<IpProtectionProbabilisticRevealTokenManager>(
      std::move(fetcher_));
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_TRUE(manager_->IsTokenAvailable());
  EXPECT_TRUE(manager_->GetToken("fp.ex", "tp.ex"));

  // Advance time to 5 seconds before tokens expire.
  task_environment_.AdvanceClock(expiration - base::Time::Now() -
                                 base::Seconds(5));
  EXPECT_TRUE(manager_->IsTokenAvailable());
  EXPECT_TRUE(manager_->GetToken("fp.ex", "tp.ex"));

  // Advance time to 1 second before tokens expire.
  task_environment_.AdvanceClock(expiration - base::Time::Now() -
                                 base::Seconds(1));
  EXPECT_TRUE(manager_->IsTokenAvailable());
  EXPECT_TRUE(manager_->GetToken("fp.ex", "tp.ex"));

  // Advance time to expiration of tokens.
  task_environment_.AdvanceClock(expiration - base::Time::Now());
  EXPECT_FALSE(manager_->IsTokenAvailable());
  EXPECT_FALSE(manager_->GetToken("a.ex", "b.ex"));
}

// Test whether GetToken() returns the same token for the same
// first and third party during an epoch.
TEST_F(IpProtectionProbabilisticRevealTokenManagerTest,
       GetTokenSameFirstAndThirdParty) {
  // Create manager, posts token fetch task in constructor.
  manager_ = std::make_unique<IpProtectionProbabilisticRevealTokenManager>(
      std::move(fetcher_));

  task_environment_.FastForwardBy(base::TimeDelta());

  const std::string top_level = "awe-page.ex";
  const std::string third_party = "tp.ex";

  auto maybe_token = manager_->GetToken(top_level, third_party);
  ASSERT_TRUE(maybe_token.has_value());
  const ProbabilisticRevealToken token1 = maybe_token.value();

  for (int i = 0; i < 5; ++i) {
    maybe_token = manager_->GetToken(top_level, third_party);
    ASSERT_TRUE(maybe_token.has_value());
    const ProbabilisticRevealToken token2 = maybe_token.value();
    EXPECT_EQ(token1, token2);
  }
}

// Test whether GetToken() returns the randomized versions of the same
// token for the same first party.
TEST_F(IpProtectionProbabilisticRevealTokenManagerTest,
       GetTokenSameFirstParty) {
  // Create manager, posts token fetch task in constructor.
  manager_ = std::make_unique<IpProtectionProbabilisticRevealTokenManager>(
      std::move(fetcher_));

  task_environment_.FastForwardBy(base::TimeDelta());

  const std::string top_level = "awe-page.ex";

  auto maybe_token = manager_->GetToken(top_level, "tp.ex");
  ASSERT_TRUE(maybe_token.has_value());
  const ProbabilisticRevealToken token_ex = maybe_token.value();

  maybe_token = manager_->GetToken(top_level, "tp.com");
  ASSERT_TRUE(maybe_token.has_value());
  const ProbabilisticRevealToken token_com = maybe_token.value();

  EXPECT_NE(token_ex, token_com);

  // When decrypted, both tokens should return the same point.
  auto serialized_point_ex = DecryptSerializeEncode(token_ex);
  auto serialized_point_com = DecryptSerializeEncode(token_com);
  EXPECT_EQ(serialized_point_ex, serialized_point_com);
}

// Verify manager posts next fetch task at next epoch start.
TEST_F(IpProtectionProbabilisticRevealTokenManagerTest, RefetchSuccess) {
  const base::Time first_batch_expiration = base::Time::Now() + base::Hours(8);
  const base::Time first_batch_next_start = base::Time::Now() + base::Hours(4);
  SetIssuer(/*private_key=*/12345,
            /*num_tokens=*/10, first_batch_expiration, first_batch_next_start,
            /*num_tokens_with_signal=*/3);
  const std::vector<ProbabilisticRevealToken> first_batch_tokens =
      fetcher_ptr_->Issuer()->Tokens();

  // Constructor fetches first batch and schedules next fetch at `next_start`.
  manager_ = std::make_unique<IpProtectionProbabilisticRevealTokenManager>(
      std::move(fetcher_));
  task_environment_.FastForwardBy(base::TimeDelta());

  EXPECT_TRUE(manager_->IsTokenAvailable());

  // check that GetToken() returns a token that is in the batch
  // by decrypting token returned by `GetToken()` and checking whether
  // it is in decrypted `first_batch_tokens`.
  auto first_batch_points = DecryptSerializeEncode(first_batch_tokens);
  auto maybe_token = manager_->GetToken("a", "b");
  ASSERT_TRUE(maybe_token.has_value());
  auto point = DecryptSerializeEncode(maybe_token.value());
  EXPECT_THAT(first_batch_points, testing::Contains(point))
      << "GetToken() returned a token that is not in the current batch.";

  SetIssuer(/*private_key=*/77777,
            /*num_tokens=*/27,
            /*expiration=*/base::Time::Now() + base::Hours(16),
            /*next_start==*/base::Time::Now() + base::Hours(8),
            /*num_tokens_with_signal=*/12);
  const std::vector<ProbabilisticRevealToken> second_batch_tokens =
      fetcher_ptr_->Issuer()->Tokens();

  task_environment_.FastForwardBy(first_batch_next_start - base::Time::Now());

  // check that GetToken() returns a token that is in the second batch
  // by decrypting token returned by `GetToken()` and checking whether
  // it is in decrypted `second_batch_tokens`.
  EXPECT_TRUE(manager_->IsTokenAvailable());
  auto second_batch_points = DecryptSerializeEncode(second_batch_tokens);
  maybe_token = manager_->GetToken("a", "b");
  ASSERT_TRUE(maybe_token.has_value());
  point = DecryptSerializeEncode(maybe_token.value());
  EXPECT_THAT(second_batch_points, testing::Contains(point))
      << "GetToken() returned a token that is not in the current batch.";
}

// Check whether manager tries again in accordance with the try again returned
// by the direct fetcher.
TEST_F(IpProtectionProbabilisticRevealTokenManagerTest, NetworkErrorTryAgain) {
  SetResponse(
      std::nullopt,
      TryGetProbabilisticRevealTokensResult{
          TryGetProbabilisticRevealTokensStatus::kNetNotOk,
          net::ERR_OUT_OF_MEMORY, base::Time::Now() + base::Seconds(23)});
  manager_ = std::make_unique<IpProtectionProbabilisticRevealTokenManager>(
      std::move(fetcher_));
  task_environment_.FastForwardBy(base::TimeDelta());

  // Fetch is called 1 times, once task posted by constructor is done.
  EXPECT_EQ(fetcher_ptr_->NumCalls(), std::size_t(1));

  task_environment_.FastForwardBy(base::Seconds(22));
  EXPECT_EQ(fetcher_ptr_->NumCalls(), std::size_t(1));

  SetResponse(
      std::nullopt,
      TryGetProbabilisticRevealTokensResult{
          TryGetProbabilisticRevealTokensStatus::kNetNotOk,
          net::ERR_OUT_OF_MEMORY, base::Time::Now() + base::Seconds(42)});
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_EQ(fetcher_ptr_->NumCalls(), std::size_t(2));

  SetResponse(
      std::nullopt,
      TryGetProbabilisticRevealTokensResult{
          TryGetProbabilisticRevealTokensStatus::kNetNotOk,
          net::ERR_OUT_OF_MEMORY, base::Time::Now() + base::Seconds(51)});
  task_environment_.FastForwardBy(base::Seconds(42));
  EXPECT_EQ(fetcher_ptr_->NumCalls(), std::size_t(3));
}

// If next_epoch_start is before base::Time::Now(), manager should
// schedule next fetch in 3 hours.
TEST_F(IpProtectionProbabilisticRevealTokenManagerTest, PassedNextEpochStart) {
  const base::Time expiration = base::Time::Now() + base::Hours(8);
  // next_start is already passed.
  const base::Time next_start = base::Time::Now() - base::Seconds(1);
  SetIssuer(/*private_key=*/137,
            /*num_tokens=*/12, expiration, next_start,
            /*num_tokens_with_signal=*/5);

  // Create manager, posts token fetch task in constructor.
  manager_ = std::make_unique<IpProtectionProbabilisticRevealTokenManager>(
      std::move(fetcher_));

  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_EQ(fetcher_ptr_->NumCalls(), std::size_t(1));

  task_environment_.FastForwardBy(base::Hours(3) - base::Seconds(1));
  EXPECT_EQ(fetcher_ptr_->NumCalls(), std::size_t(1));

  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_EQ(fetcher_ptr_->NumCalls(), std::size_t(2));
}

// Creating crypter should fail if tokens in the server response is invalid.
// IsTokenAvailable() and GetToken() should behave as expected.
TEST_F(IpProtectionProbabilisticRevealTokenManagerTest,
       InvalidServerResponseCreatingCrypterFails) {
  std::vector<ProbabilisticRevealToken> tokens =
      fetcher_ptr_->Issuer()->Tokens();
  // mess up tokens[0] to make it invalid
  tokens[0].e[0] = 'A';
  tokens[0].e[28] = 'A';
  tokens[0].u[0] = 'A';
  tokens[0].u[28] = 'A';

  TryGetProbabilisticRevealTokensOutcome outcome;
  outcome.tokens = tokens;
  outcome.public_key = fetcher_ptr_->Issuer()->GetSerializedPublicKey();
  outcome.expiration_time_seconds =
      (base::Time::Now() + base::Hours(8)).InSecondsFSinceUnixEpoch();
  outcome.next_epoch_start_time_seconds =
      (base::Time::Now() + base::Hours(4)).InSecondsFSinceUnixEpoch();
  outcome.num_tokens_with_signal = 1;
  SetResponse({std::move(outcome)},
              TryGetProbabilisticRevealTokensResult{
                  TryGetProbabilisticRevealTokensStatus::kSuccess, net::OK,
                  std::nullopt});

  // Create manager, posts token fetch task in constructor.
  manager_ = std::make_unique<IpProtectionProbabilisticRevealTokenManager>(
      std::move(fetcher_));
  task_environment_.FastForwardBy(base::TimeDelta());

  EXPECT_FALSE(manager_->IsTokenAvailable());
  EXPECT_FALSE(manager_->GetToken("a", "b"));
}

// Test behavior when PRT issuer is misconfigured and the second response from
// server is invalid. Tokens from the first response should be retained until
// expiration.
TEST_F(IpProtectionProbabilisticRevealTokenManagerTest,
       InvalidServerResponseParsingResponseFails) {
  // First response is valid.
  const base::Time expiration = base::Time::Now() + base::Hours(8);
  const base::Time next_start = base::Time::Now() + base::Hours(4);
  SetIssuer(
      /*private_key=*/4455,
      /*num_tokens=*/10, expiration, next_start,
      /*num_tokens_with_signal=*/3);
  const std::vector<ProbabilisticRevealToken> first_batch_tokens =
      fetcher_ptr_->Issuer()->Tokens();

  // Constructor fetches first batch and schedules next fetch at `next_start`.
  manager_ = std::make_unique<IpProtectionProbabilisticRevealTokenManager>(
      std::move(fetcher_));
  task_environment_.FastForwardBy(base::TimeDelta());

  EXPECT_EQ(fetcher_ptr_->NumCalls(), std::size_t(1));
  EXPECT_TRUE(manager_->IsTokenAvailable());

  // set fetcher to return parsing error.
  SetResponse(
      {},
      {TryGetProbabilisticRevealTokensStatus::kResponseParsingFailed, net::OK,
       /*try_again_after=*/std::nullopt});

  // Check that fetching triggered at next start.
  task_environment_.FastForwardBy(next_start - base::Time::Now());
  EXPECT_EQ(fetcher_ptr_->NumCalls(), std::size_t(2));

  // Check that manager re-tried fetching in an hour.
  task_environment_.FastForwardBy(base::Hours(1) - base::Seconds(1));
  EXPECT_EQ(fetcher_ptr_->NumCalls(), std::size_t(2));
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_EQ(fetcher_ptr_->NumCalls(), std::size_t(3));

  // First batch is not expired yet. Check that GetToken() returns a
  // token that is in the first batch by decrypting token returned by
  // `GetToken()` and checking whether it is in decrypted
  // `first_batch_tokens`.
  EXPECT_TRUE(manager_->IsTokenAvailable());
  auto first_batch_points = DecryptSerializeEncode(first_batch_tokens);
  auto maybe_token = manager_->GetToken("a", "b");
  ASSERT_TRUE(maybe_token.has_value());
  auto point = DecryptSerializeEncode(maybe_token.value());
  EXPECT_THAT(first_batch_points, testing::Contains(point))
      << "GetToken() returned a token that is not in the current batch.";

  // Tokens expire as expected
  task_environment_.FastForwardBy(expiration - base::Time::Now());
  EXPECT_FALSE(manager_->IsTokenAvailable());
  EXPECT_FALSE(manager_->GetToken("ip", "protection"));
}

}  // namespace ip_protection
