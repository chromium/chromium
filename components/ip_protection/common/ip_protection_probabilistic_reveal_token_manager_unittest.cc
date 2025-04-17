// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_probabilistic_reveal_token_manager.h"

#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/ip_protection/common/ip_protection_probabilistic_reveal_token_crypter.h"
#include "components/ip_protection/common/ip_protection_probabilistic_reveal_token_fetcher.h"
#include "components/ip_protection/common/probabilistic_reveal_token_test_consumer.h"
#include "components/ip_protection/common/probabilistic_reveal_token_test_issuer.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/network_switches.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/test/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ip_protection {

namespace {

constexpr char kGetTokensResultHistogram[] =
    "NetworkService.IpProtection.GetProbabilisticRevealTokensResult";
constexpr char kGetTokensRequestTimeHistogram[] =
    "NetworkService.IpProtection.ProbabilisticRevealTokensRequestTime";
constexpr char kInitialTokenAvailableHistogram[] =
    "NetworkService.IpProtection."
    "IsProbabilisticRevealTokenAvailableOnInitialRequest";
constexpr char kSubsequentTokenAvailableHistogram[] =
    "NetworkService.IpProtection."
    "IsProbabilisticRevealTokenAvailableOnSubsequentRequest";
constexpr char kRandomizationTimeHistogram[] =
    "NetworkService.IpProtection.ProbabilisticRevealTokenRandomizationTime";

constexpr size_t kPlaintextSize = 29;


// Mocks a PRT fetcher. Uses ProbabilisticRevealTokenTestIssuer for successful
// fetches with valid tokens and SetResponse to mock error results.
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
                         int32_t num_tokens_with_signal,
                         std::string epoch_id) {
    {
      auto maybe_issuer =
          ProbabilisticRevealTokenTestIssuer::Create(private_key);
      if (!maybe_issuer.has_value()) {
        return maybe_issuer.error();
      }
      issuer_ = std::move(maybe_issuer.value());
      // Issue and store tokens in issuer_.
      std::vector<std::string> plaintexts(num_tokens, "");
      for (std::size_t i = 0; i < num_tokens; ++i) {
        std::string p = "awesome-prt-" + base::NumberToString(i);
        plaintexts[i] = p + std::string(kPlaintextSize - p.size(), '-');
      }
      base::expected<GetProbabilisticRevealTokenResponse, absl::Status>
          maybe_response = issuer_->Issue(plaintexts, expiration, next_start,
                                          num_tokens_with_signal, epoch_id);
      if (!maybe_response.has_value()) {
        return maybe_response.error();
      }
    }
    TryGetProbabilisticRevealTokensOutcome outcome;
    outcome.tokens = issuer_->Tokens();
    outcome.public_key = issuer_->GetSerializedPublicKey();
    outcome.expiration_time_seconds = expiration.InSecondsFSinceUnixEpoch();
    outcome.next_epoch_start_time_seconds =
        next_start.InSecondsFSinceUnixEpoch();
    outcome.num_tokens_with_signal = num_tokens_with_signal;
    outcome.epoch_id = std::move(epoch_id);
    SetResponse({std::move(outcome)},
                TryGetProbabilisticRevealTokensResult{
                    TryGetProbabilisticRevealTokensStatus::kSuccess, net::OK,
                    std::nullopt});
    return absl::OkStatus();
  }

  size_t NumCalls() const { return num_calls_; }

  ProbabilisticRevealTokenTestIssuer* Issuer() { return issuer_.get(); }

 private:
  std::optional<TryGetProbabilisticRevealTokensOutcome> outcome_;
  TryGetProbabilisticRevealTokensResult result_;
  size_t num_calls_ = 0;
  std::unique_ptr<ProbabilisticRevealTokenTestIssuer> issuer_;
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
                            /*num_tokens_with_signal=*/7,
                            /*epoch_id=*/"epoch_id");
    ASSERT_TRUE(status.ok());
    fetcher_ptr_ = fetcher_.get();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
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
                 size_t num_tokens_with_signal,
                 std::string epoch_id) {
    auto status =
        fetcher_ptr_->SetIssuer(private_key, num_tokens, expiration, next_start,
                                num_tokens_with_signal, std::move(epoch_id));
    ASSERT_TRUE(status.ok());
  }

  // Deserialize a given prt serialized using
  // `IpProtectionProbabilisticRevealTokenManager::SerializePrt()`.
  void Deserialize(const std::string& serialized_prt,
                   ProbabilisticRevealToken& token_out,
                   std::string& epoch_id_out) {
    std::optional<ProbabilisticRevealTokenTestConsumer> consumer =
        ProbabilisticRevealTokenTestConsumer::MaybeCreate(serialized_prt);
    ASSERT_TRUE(consumer) << "Deserializing PRT failed";
    token_out = consumer->Token();
    epoch_id_out = consumer->EpochId();
  }

  // Decrypt given token, serialize returned point, and base64 encode.
  std::string DecryptSerializeEncode(const ProbabilisticRevealToken& token) {
    auto maybe_serialized_point =
        fetcher_ptr_->Issuer()->DecryptSerializeEncode(token);
    EXPECT_TRUE(maybe_serialized_point.has_value());
    return std::move(maybe_serialized_point.value());
  }

  std::vector<std::string> DecryptSerializeEncode(
      const std::vector<ProbabilisticRevealToken>& tokens) {
    auto maybe_serialized_points =
        fetcher_ptr_->Issuer()->DecryptSerializeEncode(tokens);
    EXPECT_TRUE(maybe_serialized_points.has_value());
    return std::move(maybe_serialized_points.value());
  }

  base::FilePath DbPath() const {
    return DataDirectory().Append(
        FILE_PATH_LITERAL("ProbabilisticRevealTokens"));
  }

  base::FilePath DataDirectory() const {
    return temp_dir_.GetPath().AppendASCII("DataDirectory");
  }

  size_t CountTokenEntries(sql::Database& db) {
    static const char kCountSQL[] = "SELECT COUNT(*) FROM tokens";
    sql::Statement s(db.GetUniqueStatement(kCountSQL));
    EXPECT_TRUE(s.Step());
    return s.ColumnInt(0);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<MockFetcher> fetcher_;
  std::unique_ptr<IpProtectionProbabilisticRevealTokenManager> manager_;
  // fetcher_ is moved to create manager. `fetcher_ptr_` is a pointer
  // to fetcher to modify its behavior after it is moved.
  raw_ptr<MockFetcher> fetcher_ptr_;
  base::HistogramTester histogram_tester_;
  base::ScopedTempDir temp_dir_;
};

// Test whether IsTokenAvailable() returns false and GetToken() returns null,
// when no response from the PRT issuer server is returned, i.e., crypter is
// null.
TEST_F(IpProtectionProbabilisticRevealTokenManagerTest, NotRequestedTokensYet) {
  manager_ = std::make_unique<IpProtectionProbabilisticRevealTokenManager>(
      std::move(fetcher_), DataDirectory());
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_FALSE(manager_->IsTokenAvailable());
  EXPECT_FALSE(manager_->GetToken("A", "b42"));

  histogram_tester_.ExpectUniqueSample(
      kGetTokensResultHistogram,
      TryGetProbabilisticRevealTokensStatus::kNullResponse, 0);
  histogram_tester_.ExpectTotalCount(kGetTokensRequestTimeHistogram, 0);
  histogram_tester_.ExpectUniqueSample(kInitialTokenAvailableHistogram, false,
                                       1);
}

TEST_F(IpProtectionProbabilisticRevealTokenManagerTest,
       TokensExpireAsExpected) {
  const base::Time expiration = base::Time::Now() + base::Hours(8);
  const base::Time next_start = base::Time::Now() + base::Hours(4);
  SetIssuer(/*private_key=*/12345,
            /*num_tokens=*/10, expiration, next_start,
            /*num_tokens_with_signal=*/3, "epoch_id");

  manager_ = std::make_unique<IpProtectionProbabilisticRevealTokenManager>(
      std::move(fetcher_), DataDirectory());
  // Request tokens, schedules next fetch at `next_start`.
  manager_->RequestTokens();
  task_environment_.FastForwardBy(base::TimeDelta());

  histogram_tester_.ExpectUniqueSample(
      kGetTokensResultHistogram,
      TryGetProbabilisticRevealTokensStatus::kSuccess, 1);
  histogram_tester_.ExpectTotalCount(kGetTokensRequestTimeHistogram, 1);

  EXPECT_TRUE(manager_->IsTokenAvailable());
  EXPECT_TRUE(manager_->GetToken("fp.ex", "tp.ex"));

  histogram_tester_.ExpectUniqueSample(kInitialTokenAvailableHistogram, true,
                                       1);
  histogram_tester_.ExpectUniqueSample(kSubsequentTokenAvailableHistogram, true,
                                       0);

  // Advance time to 5 seconds before tokens expire.
  task_environment_.AdvanceClock(expiration - base::Time::Now() -
                                 base::Seconds(5));
  EXPECT_TRUE(manager_->IsTokenAvailable());
  EXPECT_TRUE(manager_->GetToken("fp.ex", "tp.ex"));

  histogram_tester_.ExpectUniqueSample(kInitialTokenAvailableHistogram, true,
                                       1);
  histogram_tester_.ExpectUniqueSample(kSubsequentTokenAvailableHistogram, true,
                                       1);

  // Advance time to 1 second before tokens expire.
  task_environment_.AdvanceClock(expiration - base::Time::Now() -
                                 base::Seconds(1));
  EXPECT_TRUE(manager_->IsTokenAvailable());
  EXPECT_TRUE(manager_->GetToken("fp.ex", "tp.ex"));

  histogram_tester_.ExpectUniqueSample(kInitialTokenAvailableHistogram, true,
                                       1);
  histogram_tester_.ExpectUniqueSample(kSubsequentTokenAvailableHistogram, true,
                                       2);

  // Advance time to expiration of tokens.
  task_environment_.AdvanceClock(expiration - base::Time::Now());
  EXPECT_FALSE(manager_->IsTokenAvailable());
  EXPECT_FALSE(manager_->GetToken("a.ex", "b.ex"));

  histogram_tester_.ExpectUniqueSample(kInitialTokenAvailableHistogram, true,
                                       1);
  histogram_tester_.ExpectBucketCount(kSubsequentTokenAvailableHistogram, true,
                                      2);
  histogram_tester_.ExpectBucketCount(kSubsequentTokenAvailableHistogram, false,
                                      1);
}

// Test whether GetToken() returns the same token for the same
// first and third party during an epoch.
TEST_F(IpProtectionProbabilisticRevealTokenManagerTest,
       GetTokenSameFirstAndThirdParty) {
  manager_ = std::make_unique<IpProtectionProbabilisticRevealTokenManager>(
      std::move(fetcher_), DataDirectory());
  manager_->RequestTokens();

  task_environment_.FastForwardBy(base::TimeDelta());

  const std::string top_level = "awe-page.ex";
  const std::string third_party = "tp.ex";

  std::optional<std::string> serialized_token =
      manager_->GetToken(top_level, third_party);
  ASSERT_TRUE(serialized_token.has_value());
  const std::string token1 = serialized_token.value();

  for (int i = 0; i < 5; ++i) {
    serialized_token = manager_->GetToken(top_level, third_party);
    ASSERT_TRUE(serialized_token.has_value());
    const std::string token2 = serialized_token.value();
    EXPECT_EQ(token1, token2);
  }

  // The token will only be randomized once for the same first/third party pair.
  histogram_tester_.ExpectTotalCount(kRandomizationTimeHistogram, 1);
}

// Test whether GetToken() returns the randomized versions of the same
// token for the same first party.
TEST_F(IpProtectionProbabilisticRevealTokenManagerTest,
       GetTokenSameFirstParty) {
  manager_ = std::make_unique<IpProtectionProbabilisticRevealTokenManager>(
      std::move(fetcher_), DataDirectory());
  manager_->RequestTokens();

  task_environment_.FastForwardBy(base::TimeDelta());

  const std::string top_level = "awe-page.ex";

  std::optional<std::string> serialized_token_ex =
      manager_->GetToken(top_level, "tp.ex");
  ASSERT_TRUE(serialized_token_ex.has_value());

  std::optional<std::string> serialized_token_com =
      manager_->GetToken(top_level, "tp.com");
  ASSERT_TRUE(serialized_token_com.has_value());

  EXPECT_NE(serialized_token_ex.value(), serialized_token_com.value());

  // The token will be randomized for each distinct first/third party pair.
  histogram_tester_.ExpectTotalCount(kRandomizationTimeHistogram, 2);

  ProbabilisticRevealToken token_ex;
  std::string epoch_id_ex;
  Deserialize(serialized_token_ex.value(), token_ex, epoch_id_ex);

  ProbabilisticRevealToken token_com;
  std::string epoch_id_com;
  Deserialize(serialized_token_com.value(), token_com, epoch_id_com);

  EXPECT_EQ(epoch_id_ex, epoch_id_com);

  // When decrypted, both tokens should return the same point.
  auto serialized_point_ex = DecryptSerializeEncode(token_ex);
  auto serialized_point_com = DecryptSerializeEncode(token_com);
  EXPECT_EQ(serialized_point_ex, serialized_point_com);
}

// Verify RequestTokens() posts next fetch task after next epoch start and
// before token expiration.
TEST_F(IpProtectionProbabilisticRevealTokenManagerTest, RefetchSuccess) {
  const base::Time first_batch_expiration = base::Time::Now() + base::Hours(8);
  const base::Time first_batch_next_start = base::Time::Now() + base::Hours(4);
  const std::string epoch_id_1 = std::string(8, '1');
  SetIssuer(/*private_key=*/12345,
            /*num_tokens=*/10, first_batch_expiration, first_batch_next_start,
            /*num_tokens_with_signal=*/3,
            /*epoch_id=*/epoch_id_1);
  const std::vector<ProbabilisticRevealToken> first_batch_tokens =
      fetcher_ptr_->Issuer()->Tokens();

  manager_ = std::make_unique<IpProtectionProbabilisticRevealTokenManager>(
      std::move(fetcher_), DataDirectory());
  manager_->RequestTokens();
  task_environment_.FastForwardBy(base::TimeDelta());

  EXPECT_TRUE(manager_->IsTokenAvailable());
  histogram_tester_.ExpectUniqueSample(
      kGetTokensResultHistogram,
      TryGetProbabilisticRevealTokensStatus::kSuccess, 1);
  histogram_tester_.ExpectTotalCount(kGetTokensRequestTimeHistogram, 1);

  // check that GetToken() returns a token that is in the batch
  // by decrypting token returned by `GetToken()` and checking whether
  // it is in decrypted `first_batch_tokens`.
  std::vector<std::string> first_batch_points =
      DecryptSerializeEncode(first_batch_tokens);
  std::optional<std::string> serialized_token = manager_->GetToken("a", "b");
  ASSERT_TRUE(serialized_token.has_value());

  ProbabilisticRevealToken token;
  std::string epoch_id;
  Deserialize(serialized_token.value(), token, epoch_id);
  EXPECT_EQ(epoch_id, epoch_id_1);

  std::string point = DecryptSerializeEncode(token);
  EXPECT_THAT(first_batch_points, testing::Contains(point))
      << "GetToken() returned a token that is not in the current batch.";

  // Expect no re-fetch before the next epoch start.
  task_environment_.FastForwardBy(first_batch_next_start - base::Seconds(1) -
                                  base::Time::Now());
  histogram_tester_.ExpectUniqueSample(
      kGetTokensResultHistogram,
      TryGetProbabilisticRevealTokensStatus::kSuccess, 1);
  histogram_tester_.ExpectTotalCount(kGetTokensRequestTimeHistogram, 1);

  const std::string epoch_id_2 = std::string(8, '2');
  SetIssuer(/*private_key=*/77777,
            /*num_tokens=*/27,
            /*expiration=*/base::Time::Now() + base::Hours(16),
            /*next_start==*/base::Time::Now() + base::Hours(8),
            /*num_tokens_with_signal=*/12,
            /*epoch_id=*/epoch_id_2);
  const std::vector<ProbabilisticRevealToken> second_batch_tokens =
      fetcher_ptr_->Issuer()->Tokens();

  task_environment_.FastForwardBy(first_batch_expiration - base::Time::Now());

  // check that GetToken() returns a token that is in the second batch
  // by decrypting token returned by `GetToken()` and checking whether
  // it is in decrypted `second_batch_tokens`.
  EXPECT_TRUE(manager_->IsTokenAvailable());
  std::vector<std::string> second_batch_points =
      DecryptSerializeEncode(second_batch_tokens);
  serialized_token = manager_->GetToken("a", "b");
  ASSERT_TRUE(serialized_token.has_value());
  Deserialize(serialized_token.value(), token, epoch_id);
  EXPECT_EQ(epoch_id, epoch_id_2);
  point = DecryptSerializeEncode(token);
  EXPECT_THAT(second_batch_points, testing::Contains(point))
      << "GetToken() returned a token that is not in the current batch.";
  histogram_tester_.ExpectUniqueSample(
      kGetTokensResultHistogram,
      TryGetProbabilisticRevealTokensStatus::kSuccess, 2);
  histogram_tester_.ExpectTotalCount(kGetTokensRequestTimeHistogram, 2);
}

// Check whether RequestTokens() tries again in accordance with the try again
// returned by the direct fetcher.
TEST_F(IpProtectionProbabilisticRevealTokenManagerTest, NetworkErrorTryAgain) {
  SetResponse(
      std::nullopt,
      TryGetProbabilisticRevealTokensResult{
          TryGetProbabilisticRevealTokensStatus::kNetNotOk,
          net::ERR_OUT_OF_MEMORY, base::Time::Now() + base::Seconds(23)});
  manager_ = std::make_unique<IpProtectionProbabilisticRevealTokenManager>(
      std::move(fetcher_), DataDirectory());
  manager_->RequestTokens();
  task_environment_.FastForwardBy(base::TimeDelta());

  // Fetch is called 1 times, once task posted by constructor is done.
  EXPECT_EQ(fetcher_ptr_->NumCalls(), std::size_t(1));

  task_environment_.FastForwardBy(base::Seconds(22));
  EXPECT_EQ(fetcher_ptr_->NumCalls(), std::size_t(1));
  histogram_tester_.ExpectUniqueSample(
      kGetTokensResultHistogram,
      TryGetProbabilisticRevealTokensStatus::kNetNotOk, 1);
  histogram_tester_.ExpectTotalCount(kGetTokensRequestTimeHistogram, 0);

  SetResponse(
      std::nullopt,
      TryGetProbabilisticRevealTokensResult{
          TryGetProbabilisticRevealTokensStatus::kNetNotOk,
          net::ERR_OUT_OF_MEMORY, base::Time::Now() + base::Seconds(42)});
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_EQ(fetcher_ptr_->NumCalls(), std::size_t(2));
  histogram_tester_.ExpectUniqueSample(
      kGetTokensResultHistogram,
      TryGetProbabilisticRevealTokensStatus::kNetNotOk, 2);
  histogram_tester_.ExpectTotalCount(kGetTokensRequestTimeHistogram, 0);

  SetResponse(
      std::nullopt,
      TryGetProbabilisticRevealTokensResult{
          TryGetProbabilisticRevealTokensStatus::kNetNotOk,
          net::ERR_OUT_OF_MEMORY, base::Time::Now() + base::Seconds(51)});
  task_environment_.FastForwardBy(base::Seconds(42));
  EXPECT_EQ(fetcher_ptr_->NumCalls(), std::size_t(3));
  histogram_tester_.ExpectUniqueSample(
      kGetTokensResultHistogram,
      TryGetProbabilisticRevealTokensStatus::kNetNotOk, 3);
  histogram_tester_.ExpectTotalCount(kGetTokensRequestTimeHistogram, 0);
}

// If next_epoch_start is before base::Time::Now(), RequestTokens() should
// schedule next fetch in 3 hours.
TEST_F(IpProtectionProbabilisticRevealTokenManagerTest, PassedNextEpochStart) {
  const base::Time expiration = base::Time::Now() + base::Hours(8);
  // next_start is already passed.
  const base::Time next_start = base::Time::Now() - base::Seconds(1);
  SetIssuer(/*private_key=*/137,
            /*num_tokens=*/12, expiration, next_start,
            /*num_tokens_with_signal=*/5, "epoch_id");

  manager_ = std::make_unique<IpProtectionProbabilisticRevealTokenManager>(
      std::move(fetcher_), DataDirectory());
  manager_->RequestTokens();

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
  outcome.epoch_id = std::string(8, '1');
  SetResponse({std::move(outcome)},
              TryGetProbabilisticRevealTokensResult{
                  TryGetProbabilisticRevealTokensStatus::kSuccess, net::OK,
                  std::nullopt});

  manager_ = std::make_unique<IpProtectionProbabilisticRevealTokenManager>(
      std::move(fetcher_), DataDirectory());
  manager_->RequestTokens();
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
  const std::string epoch_id_1 = std::string(8, '1');
  SetIssuer(
      /*private_key=*/4455,
      /*num_tokens=*/10, expiration, next_start,
      /*num_tokens_with_signal=*/3, epoch_id_1);
  const std::vector<ProbabilisticRevealToken> first_batch_tokens =
      fetcher_ptr_->Issuer()->Tokens();

  // Constructor fetches first batch and schedules next fetch at `next_start`.
  manager_ = std::make_unique<IpProtectionProbabilisticRevealTokenManager>(
      std::move(fetcher_), DataDirectory());
  manager_->RequestTokens();
  task_environment_.FastForwardBy(base::TimeDelta());

  EXPECT_EQ(fetcher_ptr_->NumCalls(), std::size_t(1));
  EXPECT_TRUE(manager_->IsTokenAvailable());
  histogram_tester_.ExpectUniqueSample(
      kGetTokensResultHistogram,
      TryGetProbabilisticRevealTokensStatus::kSuccess, 1);
  histogram_tester_.ExpectTotalCount(kGetTokensRequestTimeHistogram, 1);

  // set fetcher to return parsing error.
  SetResponse(
      {},
      {TryGetProbabilisticRevealTokensStatus::kResponseParsingFailed, net::OK,
       /*try_again_after=*/std::nullopt});

  // Check that fetching triggered before expiration.
  task_environment_.FastForwardBy(expiration - base::Minutes(10) -
                                  base::Time::Now());

  size_t num_calls_at_expiration = fetcher_ptr_->NumCalls();
  int32_t parsing_error_count_at_expiration = histogram_tester_.GetBucketCount(
      kGetTokensResultHistogram,
      TryGetProbabilisticRevealTokensStatus::kResponseParsingFailed);

  // Fetch should have been called at least one more time (could be more
  // depending on the random time chosen between next epoch start and
  // expiration).
  EXPECT_GE(num_calls_at_expiration, std::size_t(2));
  EXPECT_GE(parsing_error_count_at_expiration, 1);
  histogram_tester_.ExpectBucketCount(
      kGetTokensResultHistogram,
      TryGetProbabilisticRevealTokensStatus::kSuccess, 1);
  histogram_tester_.ExpectTotalCount(kGetTokensRequestTimeHistogram, 1);

  // First batch is not expired yet. Check that GetToken() returns a
  // token that is in the first batch by decrypting token returned by
  // `GetToken()` and checking whether it is in decrypted
  // `first_batch_tokens`.
  EXPECT_TRUE(manager_->IsTokenAvailable());
  auto first_batch_points = DecryptSerializeEncode(first_batch_tokens);
  auto serialized_token = manager_->GetToken("a", "b");
  ASSERT_TRUE(serialized_token.has_value());

  ProbabilisticRevealToken token;
  std::string epoch_id;
  Deserialize(serialized_token.value(), token, epoch_id);
  EXPECT_EQ(epoch_id, epoch_id_1);

  auto point = DecryptSerializeEncode(token);
  EXPECT_THAT(first_batch_points, testing::Contains(point))
      << "GetToken() returned a token that is not in the current batch.";

  // Check that manager re-tried fetching once more after an hour.
  task_environment_.FastForwardBy(base::Hours(1));
  EXPECT_EQ(fetcher_ptr_->NumCalls(), num_calls_at_expiration + 1);
  EXPECT_EQ(histogram_tester_.GetBucketCount(
                kGetTokensResultHistogram,
                TryGetProbabilisticRevealTokensStatus::kResponseParsingFailed),
            parsing_error_count_at_expiration + 1);
  histogram_tester_.ExpectBucketCount(
      kGetTokensResultHistogram,
      TryGetProbabilisticRevealTokensStatus::kSuccess, 1);
  histogram_tester_.ExpectTotalCount(kGetTokensRequestTimeHistogram, 1);

  // First batch of tokens is now expired, and no re-fetch has succeeded.
  EXPECT_FALSE(manager_->IsTokenAvailable());
  EXPECT_FALSE(manager_->GetToken("ip", "protection"));
}

TEST_F(IpProtectionProbabilisticRevealTokenManagerTest,
       StoreTokensWhenFeatureIsEnabled) {
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitch(
      network::switches::kStoreProbabilisticRevealTokens);

  const base::Time expiration = base::Time::Now() + base::Hours(8);
  const base::Time next_start = base::Time::Now() + base::Hours(4);
  SetIssuer(/*private_key=*/12345,
            /*num_tokens=*/10, expiration, next_start,
            /*num_tokens_with_signal=*/3, "epoch_id");

  manager_ = std::make_unique<IpProtectionProbabilisticRevealTokenManager>(
      std::move(fetcher_), DataDirectory());
  manager_->RequestTokens();
  task_environment_.RunUntilIdle();

  // Expect that tokens are available.
  EXPECT_TRUE(manager_->IsTokenAvailable());
  EXPECT_TRUE(manager_->GetToken("fp.ex", "tp.ex"));

  // Destroy the manager to trigger the database write.
  fetcher_ptr_ = nullptr;
  manager_.reset();
  task_environment_.RunUntilIdle();

  // Expect that the database has 10 tokens.
  sql::Database db(sql::test::kTestTag);
  EXPECT_TRUE(db.Open(DbPath()));
  EXPECT_EQ(CountTokenEntries(db), 10u);
  db.Close();
}

TEST_F(IpProtectionProbabilisticRevealTokenManagerTest,
       DoNotStoreTokensWhenFeatureIsDisabled) {
  const base::Time expiration = base::Time::Now() + base::Hours(8);
  const base::Time next_start = base::Time::Now() + base::Hours(4);
  SetIssuer(/*private_key=*/12345,
            /*num_tokens=*/10, expiration, next_start,
            /*num_tokens_with_signal=*/3, "epoch_id");

  manager_ = std::make_unique<IpProtectionProbabilisticRevealTokenManager>(
      std::move(fetcher_), DataDirectory());
  manager_->RequestTokens();
  task_environment_.RunUntilIdle();

  // Expect that tokens are available.
  EXPECT_TRUE(manager_->IsTokenAvailable());
  EXPECT_TRUE(manager_->GetToken("fp.ex", "tp.ex"));

  // Destroy the manager to trigger the database write.
  fetcher_ptr_ = nullptr;
  manager_.reset();
  task_environment_.RunUntilIdle();

  // Expect that the database does not exist.
  sql::Database db(sql::test::kTestTag);
  EXPECT_FALSE(db.Open(DbPath()));
}

TEST_F(IpProtectionProbabilisticRevealTokenManagerTest,
       SerializationFailsWhenWrongEpochIdSize) {
  SetIssuer(/*private_key=*/12345,
            /*num_tokens=*/27,
            /*expiration=*/base::Time::Now() + base::Hours(8),
            /*next_start=*/base::Time::Now() + base::Hours(4),
            /*num_tokens_with_signal=*/7,
            /*epoch_id=*/"epoch_id_wrong_size");

  manager_ = std::make_unique<IpProtectionProbabilisticRevealTokenManager>(
      std::move(fetcher_), DataDirectory());
  manager_->RequestTokens();
  task_environment_.FastForwardBy(base::TimeDelta());

  // Expect that tokens are available, but they fail to serialize.
  EXPECT_TRUE(manager_->IsTokenAvailable());
  EXPECT_FALSE(manager_->GetToken("a", "b"));
}

}  // namespace ip_protection
