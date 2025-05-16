// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/probabilistic_reveal_token_test_issuer.h"

#include <memory>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/get_probabilistic_reveal_token.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/status/status.h"

namespace ip_protection {

namespace {

constexpr size_t kEpochIdSize = 8;

TEST(ProbabilisticRevealTokenTestIssuerTest, CreateSuccess) {
  base::expected<std::unique_ptr<ProbabilisticRevealTokenTestIssuer>,
                 absl::Status>
      maybe_issuer =
          ProbabilisticRevealTokenTestIssuer::Create(/*private_key=*/9876);
  ASSERT_TRUE(maybe_issuer.has_value());
  auto issuer = std::move(maybe_issuer.value());
  EXPECT_THAT(issuer->Tokens(), testing::SizeIs(0));
}

TEST(ProbabilisticRevealTokenTestIssuerTest, IssueNoPlaintexts) {
  base::expected<std::unique_ptr<ProbabilisticRevealTokenTestIssuer>,
                 absl::Status>
      maybe_issuer =
          ProbabilisticRevealTokenTestIssuer::Create(/*private_key=*/22);
  ASSERT_TRUE(maybe_issuer.has_value());
  auto issuer = std::move(maybe_issuer.value());
  EXPECT_THAT(issuer->Tokens(), testing::SizeIs(0));
  base::expected<GetProbabilisticRevealTokenResponse, absl::Status> response =
      issuer->Issue(/*plaintexts=*/{},
                    /*expiration=*/base::Time::Now(),
                    /*next_epoch_start=*/base::Time::Now(),
                    /*num_tokens_with_signal=*/0,
                    /*epoch_id=*/std::string(kEpochIdSize, 'a'));
  EXPECT_THAT(issuer->Tokens(), testing::SizeIs(0));
}

TEST(ProbabilisticRevealTokenTestIssuerTest, IssueSuccess) {
  base::expected<std::unique_ptr<ProbabilisticRevealTokenTestIssuer>,
                 absl::Status>
      maybe_issuer =
          ProbabilisticRevealTokenTestIssuer::Create(/*private_key=*/34);
  ASSERT_TRUE(maybe_issuer.has_value());
  auto issuer = std::move(maybe_issuer.value());
  EXPECT_THAT(issuer->Tokens(), testing::SizeIs(0));
  const std::vector<std::string> plaintexts = {
      "An algorithm must be seen to ", "be believed.-----------------",
      "Computers are useless. They  ", "can only give you answers.---",
      "------------Code never lies, ", "comments sometimes do.-------",
  };
  const auto expiration_time = base::Time::Now() + base::Hours(10);
  const auto next_epoch_start_time = base::Time::Now() + base::Hours(12);
  const int32_t num_tokens_with_signal = 2;
  const std::string epoch_id = "epoch-id";
  base::expected<GetProbabilisticRevealTokenResponse, absl::Status>
      maybe_response =
          issuer->Issue(plaintexts, expiration_time, next_epoch_start_time,
                        num_tokens_with_signal, epoch_id);
  ASSERT_TRUE(maybe_response.has_value())
      << "Issue() returned error " << maybe_response.error();
  auto const& response = maybe_response.value();
  ASSERT_THAT(response.tokens(), testing::SizeIs(plaintexts.size()));
  ASSERT_THAT(issuer->Tokens(), testing::SizeIs(plaintexts.size()));
  const ProbabilisticRevealToken token2(response.tokens()[2].version(),
                                        response.tokens()[2].u(),
                                        response.tokens()[2].e());
  base::expected<std::string, absl::Status> maybe_plaintext2 =
      issuer->RevealToken(token2);
  ASSERT_TRUE(maybe_plaintext2.has_value());
  const auto& plaintext2 = maybe_plaintext2.value();
  EXPECT_EQ(plaintext2, plaintexts[2]);

  EXPECT_EQ(response.public_key().y(), issuer->GetSerializedPublicKey());
  int64_t expiration_time_seconds = expiration_time.InSecondsFSinceUnixEpoch();
  EXPECT_EQ(response.expiration_time().seconds(), expiration_time_seconds);
  int64_t next_epoch_start_time_seconds =
      next_epoch_start_time.InSecondsFSinceUnixEpoch();
  EXPECT_EQ(response.next_epoch_start_time().seconds(),
            next_epoch_start_time_seconds);
  EXPECT_EQ(response.num_tokens_with_signal(), num_tokens_with_signal);
  EXPECT_EQ(response.epoch_id(), epoch_id);
}

TEST(ProbabilisticRevealTokenTestIssuerTest, IssueByHashingSuccess) {
  base::expected<std::unique_ptr<ProbabilisticRevealTokenTestIssuer>,
                 absl::Status>
      maybe_issuer =
          ProbabilisticRevealTokenTestIssuer::Create(/*private_key=*/34);
  ASSERT_TRUE(maybe_issuer.has_value());
  auto issuer = std::move(maybe_issuer.value());
  EXPECT_THAT(issuer->Tokens(), testing::SizeIs(0));
  std::vector<std::string> plaintexts = {
      "arbitrary-string-with-arbitrary-size---",
      "",
      "a-bit-longer-for-testing-----------------------------------------------"
      "-long-long-long--------",
  };
  std::string another_str;
  base::Base64Decode("/////52dlv///38HB5YAAAAAAAJ/EJaWlpaWlpY=", &another_str);
  plaintexts.push_back(another_str);
  const auto expiration_time = base::Time::Now() + base::Hours(10);
  const auto next_epoch_start_time = base::Time::Now() + base::Hours(12);
  const int32_t num_tokens_with_signal = 2;
  const std::string epoch_id = "epoch-id";
  base::expected<GetProbabilisticRevealTokenResponse, absl::Status>
      maybe_response = issuer->IssueByHashingToPoint(
          plaintexts, expiration_time, next_epoch_start_time,
          num_tokens_with_signal, epoch_id);
  ASSERT_TRUE(maybe_response.has_value())
      << "Issue() returned error " << maybe_response.error();
  auto const& response = maybe_response.value();
  ASSERT_THAT(response.tokens(), testing::SizeIs(plaintexts.size()));
  ASSERT_THAT(issuer->Tokens(), testing::SizeIs(plaintexts.size()));

  EXPECT_EQ(response.public_key().y(), issuer->GetSerializedPublicKey());
  int64_t expiration_time_seconds = expiration_time.InSecondsFSinceUnixEpoch();
  EXPECT_EQ(response.expiration_time().seconds(), expiration_time_seconds);
  int64_t next_epoch_start_time_seconds =
      next_epoch_start_time.InSecondsFSinceUnixEpoch();
  EXPECT_EQ(response.next_epoch_start_time().seconds(),
            next_epoch_start_time_seconds);
  EXPECT_EQ(response.num_tokens_with_signal(), num_tokens_with_signal);
  EXPECT_EQ(response.epoch_id(), epoch_id);
}

}  // namespace

}  // namespace ip_protection
