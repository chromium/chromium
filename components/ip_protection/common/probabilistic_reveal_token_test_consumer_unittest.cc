// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/probabilistic_reveal_token_test_consumer.h"

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/ip_protection/common/ip_protection_probabilistic_reveal_token_fetcher.h"
#include "components/ip_protection/common/ip_protection_probabilistic_reveal_token_manager.h"
#include "components/ip_protection/common/probabilistic_reveal_token_test_issuer.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ip_protection {

namespace {

constexpr char kPRTPointSize = 33;
constexpr size_t kPlaintextSize = 29;
constexpr size_t kCiphertextUSizeStartIndex = 1;
constexpr size_t kCiphertextESizeStartIndex = 36;

class MockFetcher : public IpProtectionProbabilisticRevealTokenFetcher {
 public:
  MockFetcher() = default;
  ~MockFetcher() override = default;
  void TryGetProbabilisticRevealTokens(
      TryGetProbabilisticRevealTokensCallback callback) override {
    std::move(callback).Run(outcome_, result_);
  }

  void SetResponse(
      std::optional<TryGetProbabilisticRevealTokensOutcome> outcome,
      TryGetProbabilisticRevealTokensResult result) {
    outcome_ = std::move(outcome);
    result_ = std::move(result);
  }

 private:
  std::optional<TryGetProbabilisticRevealTokensOutcome> outcome_;
  TryGetProbabilisticRevealTokensResult result_;
};

}  // namespace

class ProbabilisticRevealTokenTestConsumerTest : public testing::Test {
 protected:
  // SetUp enables ip protection.
  void SetUp() override {
    // Advance clock to some arbitrary time.
    task_environment_.AdvanceClock(base::Time::UnixEpoch() + base::Days(9876) -
                                   base::Time::Now());
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    issuer_ = nullptr;
    fetcher_ = std::make_unique<MockFetcher>();
    fetcher_ptr_ = fetcher_.get();
    manager_ = std::make_unique<IpProtectionProbabilisticRevealTokenManager>(
        std::move(fetcher_), DataDirectory());
    // Set issuer to serve a default PRT batch.
    SetIssuer(/*private_key=*/12345,
              /*num_tokens=*/27,
              /*expiration=*/base::Time::Now() + base::Hours(8),
              /*next_start=*/base::Time::Now() + base::Hours(4),
              /*num_tokens_with_signal=*/7,
              /*epoch_id=*/"epoch_id");
    // Request tokens and store them in manager.
    manager_->RequestTokens();
  }

 public:
  // Set issuer and fetcher response, used to get a valid PRT batch.
  void SetIssuer(uint64_t private_key,
                 size_t num_tokens,
                 base::Time expiration,
                 base::Time next_start,
                 int32_t num_tokens_with_signal,
                 std::string epoch_id) {
    plaintexts_.clear();
    issuer_ = nullptr;
    epoch_id_ = "";
    {
      auto maybe_issuer =
          ProbabilisticRevealTokenTestIssuer::Create(private_key);
      ASSERT_TRUE(maybe_issuer.has_value())
          << "creating issuer failed, error: " << maybe_issuer.error();
      issuer_ = std::move(maybe_issuer.value());
    }
    // Issue and store tokens in issuer_.
    std::vector<std::string> plaintexts;
    for (std::size_t i = 0; i < num_tokens; ++i) {
      std::string p = "awesome-prt-" + base::NumberToString(i);
      plaintexts.push_back(p + std::string(kPlaintextSize - p.size(), '-'));
    }

    base::expected<GetProbabilisticRevealTokenResponse, absl::Status>
        maybe_response = issuer_->Issue(plaintexts, expiration, next_start,
                                        num_tokens_with_signal, epoch_id);
    ASSERT_TRUE(maybe_response.has_value())
        << "issuer.Issue() failed, error: " << maybe_response.error();

    plaintexts_ = std::move(plaintexts);
    epoch_id_ = epoch_id;
    TryGetProbabilisticRevealTokensOutcome outcome;
    outcome.tokens = issuer_->Tokens();
    outcome.public_key = issuer_->GetSerializedPublicKey();
    outcome.expiration_time_seconds = expiration.InSecondsFSinceUnixEpoch();
    outcome.next_epoch_start_time_seconds =
        next_start.InSecondsFSinceUnixEpoch();
    outcome.num_tokens_with_signal = num_tokens_with_signal;
    outcome.epoch_id = std::move(epoch_id);

    fetcher_ptr_->SetResponse(
        {std::move(outcome)},
        TryGetProbabilisticRevealTokensResult{
            TryGetProbabilisticRevealTokensStatus::kSuccess, net::OK,
            std::nullopt});
  }

  base::FilePath DataDirectory() const {
    return temp_dir_.GetPath().AppendASCII("DataDirectory");
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<ProbabilisticRevealTokenTestIssuer> issuer_;
  std::unique_ptr<MockFetcher> fetcher_;
  std::unique_ptr<IpProtectionProbabilisticRevealTokenManager> manager_;
  // fetcher_ is moved to create manager. `fetcher_ptr_` is a pointer
  // to fetcher to modify its behavior after it is moved.
  raw_ptr<MockFetcher> fetcher_ptr_;
  std::vector<std::string> plaintexts_;
  std::string epoch_id_ = "";
};

TEST_F(ProbabilisticRevealTokenTestConsumerTest, CreateFailureInvalidPRT) {
  std::optional<ProbabilisticRevealTokenTestConsumer> consumer =
      ProbabilisticRevealTokenTestConsumer::MaybeCreate(
          "invalid-serialized-prt");
  EXPECT_FALSE(consumer);
}

TEST_F(ProbabilisticRevealTokenTestConsumerTest, CreateFailureTooLong) {
  std::optional<std::string> maybe_serialized_prt =
      manager_->GetToken("a.com", "b.ex");
  ASSERT_TRUE(maybe_serialized_prt);
  std::string serialized_prt = std::move(maybe_serialized_prt).value();
  EXPECT_TRUE(
      ProbabilisticRevealTokenTestConsumer::MaybeCreate(serialized_prt));
  EXPECT_FALSE(
      ProbabilisticRevealTokenTestConsumer::MaybeCreate(serialized_prt + '\0'));
}

TEST_F(ProbabilisticRevealTokenTestConsumerTest, CreateFailureTooShort) {
  std::optional<std::string> maybe_serialized_prt =
      manager_->GetToken("a.com", "b.ex");
  ASSERT_TRUE(maybe_serialized_prt);
  std::string serialized_prt = std::move(maybe_serialized_prt).value();
  EXPECT_TRUE(
      ProbabilisticRevealTokenTestConsumer::MaybeCreate(serialized_prt));
  EXPECT_FALSE(ProbabilisticRevealTokenTestConsumer::MaybeCreate(
      serialized_prt.substr(0, serialized_prt.size() - 1)));
}

TEST_F(ProbabilisticRevealTokenTestConsumerTest, CreateFailureWrongUSize) {
  std::optional<std::string> maybe_serialized_prt =
      manager_->GetToken("a.com", "b.ex");
  ASSERT_TRUE(maybe_serialized_prt);
  std::string serialized_prt = std::move(maybe_serialized_prt).value();
  EXPECT_TRUE(
      ProbabilisticRevealTokenTestConsumer::MaybeCreate(serialized_prt));
  EXPECT_EQ(serialized_prt[kCiphertextUSizeStartIndex], char(0));
  EXPECT_EQ(serialized_prt[kCiphertextUSizeStartIndex + 1], kPRTPointSize);
  // Set size of u to an invalid value.
  serialized_prt[kCiphertextUSizeStartIndex + 1] = kPRTPointSize - 1;
  EXPECT_FALSE(
      ProbabilisticRevealTokenTestConsumer::MaybeCreate(serialized_prt));
  serialized_prt[kCiphertextUSizeStartIndex + 1] = kPRTPointSize + 1;
  EXPECT_FALSE(
      ProbabilisticRevealTokenTestConsumer::MaybeCreate(serialized_prt));
}

TEST_F(ProbabilisticRevealTokenTestConsumerTest, CreateFailureWrongESize) {
  std::optional<std::string> maybe_serialized_prt =
      manager_->GetToken("d.com", "e.ex");
  ASSERT_TRUE(maybe_serialized_prt);
  std::string serialized_prt = std::move(maybe_serialized_prt).value();
  EXPECT_TRUE(
      ProbabilisticRevealTokenTestConsumer::MaybeCreate(serialized_prt));
  EXPECT_EQ(serialized_prt[kCiphertextESizeStartIndex], char(0));
  EXPECT_EQ(serialized_prt[kCiphertextESizeStartIndex + 1], kPRTPointSize);
  // Set size of e to an invalid value.
  serialized_prt[kCiphertextESizeStartIndex + 1] = kPRTPointSize - 1;
  EXPECT_FALSE(
      ProbabilisticRevealTokenTestConsumer::MaybeCreate(serialized_prt));
  serialized_prt[kCiphertextESizeStartIndex + 1] = kPRTPointSize + 1;
  EXPECT_FALSE(
      ProbabilisticRevealTokenTestConsumer::MaybeCreate(serialized_prt));
}

TEST_F(ProbabilisticRevealTokenTestConsumerTest, CreateSuccess) {
  std::optional<std::string> maybe_serialized_prt =
      manager_->GetToken("a.com", "b.ex");
  ASSERT_TRUE(maybe_serialized_prt);
  std::optional<ProbabilisticRevealTokenTestConsumer> consumer =
      ProbabilisticRevealTokenTestConsumer::MaybeCreate(*maybe_serialized_prt);
  ASSERT_TRUE(consumer);
  EXPECT_EQ(consumer->EpochId(), epoch_id_);
  base::expected<std::string, absl::Status> plaintext =
      issuer_->RevealToken(consumer->Token());
  ASSERT_TRUE(plaintext.has_value());
  EXPECT_THAT(plaintexts_, testing::Contains(plaintext));
}

}  // namespace ip_protection
