// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_probabilistic_reveal_token_crypter.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/probabilistic_reveal_token_test_issuer.h"
#include "components/ip_protection/get_probabilistic_reveal_token.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/status/status.h"

namespace ip_protection {

class IpProtectionProbabilisticRevealTokenCrypterTest : public testing::Test {
 public:
  // Encrypt given plaintexts with the given private key. Store resulting tokens
  // in `issuer_->Tokens()`.
  void CreateTokens(uint64_t private_key, std::vector<std::string> plaintexts) {
    base::expected<
        std::unique_ptr<ip_protection::ProbabilisticRevealTokenTestIssuer>,
        absl::Status>
        maybe_issuer =
            ip_protection::ProbabilisticRevealTokenTestIssuer::Create(
                private_key);
    ASSERT_TRUE(maybe_issuer.has_value())
        << "creating test issuer failed with error: " << maybe_issuer.error();
    issuer_ = std::move(maybe_issuer).value();
    base::expected<ip_protection::GetProbabilisticRevealTokenResponse,
                   absl::Status>
        maybe_response = issuer_->Issue(plaintexts,
                                        /*expiration=*/base::Time::Now(),
                                        /*next_epoch_start=*/base::Time::Now(),
                                        /*num_tokens_with_signal=*/0,
                                        /*epoch_id=*/"epoch-id");
    ASSERT_TRUE(maybe_response.has_value())
        << "creating tokens failed with error: " << maybe_response.error();
    ASSERT_THAT(issuer_->Tokens(), testing::SizeIs(plaintexts.size()));
  }

  std::string GetPublicKey() const { return issuer_->GetSerializedPublicKey(); }

  std::vector<ProbabilisticRevealToken> GetTokens() const {
    return issuer_->Tokens();
  }

  std::unique_ptr<ProbabilisticRevealTokenTestIssuer> issuer_ = nullptr;
};

TEST_F(IpProtectionProbabilisticRevealTokenCrypterTest, CreateSuccess) {
  const std::vector<std::string> plaintexts = {
      "An algorithm must be seen to ", "be believed.-----------------",
      "Computers are useless. They  ", "can only give you answers.---",
      "------------Code never lies, ", "comments sometimes do.-------",
  };
  CreateTokens(/*private_key=*/12345, plaintexts);

  base::expected<std::unique_ptr<IpProtectionProbabilisticRevealTokenCrypter>,
                 absl::Status>
      maybe_crypter = IpProtectionProbabilisticRevealTokenCrypter::Create(
          GetPublicKey(), GetTokens());
  EXPECT_TRUE(maybe_crypter.has_value());
  const auto& crypter = maybe_crypter.value();
  EXPECT_TRUE(crypter->IsTokenAvailable());
  EXPECT_EQ(crypter->NumTokens(), plaintexts.size());
}

TEST_F(IpProtectionProbabilisticRevealTokenCrypterTest, CreateEmptyTokens) {
  const std::vector<std::string> plaintexts = {
      "An algorithm must be seen to ", "be believed.-----------------",
      "Computers are useless. They  ", "can only give you answers.---",
      "------------Code never lies, ", "comments sometimes do.-------",
  };
  CreateTokens(/*private_key=*/12345, plaintexts);

  base::expected<std::unique_ptr<IpProtectionProbabilisticRevealTokenCrypter>,
                 absl::Status>
      maybe_crypter = IpProtectionProbabilisticRevealTokenCrypter::Create(
          GetPublicKey(), {});
  EXPECT_TRUE(maybe_crypter.has_value());
  const auto& crypter = maybe_crypter.value();
  EXPECT_FALSE(crypter->IsTokenAvailable());
  EXPECT_EQ(crypter->NumTokens(), std::size_t(0));
}

TEST_F(IpProtectionProbabilisticRevealTokenCrypterTest,
       CreateInvalidPublicKey) {
  const auto& serialized_public_key = "invalid-public-key";
  base::expected<std::unique_ptr<IpProtectionProbabilisticRevealTokenCrypter>,
                 absl::Status>
      maybe_crypter = IpProtectionProbabilisticRevealTokenCrypter::Create(
          serialized_public_key, {});
  EXPECT_EQ(maybe_crypter.error().code(), absl::StatusCode::kInvalidArgument);
}

TEST_F(IpProtectionProbabilisticRevealTokenCrypterTest, CreateInvalidTokenU) {
  const std::vector<std::string> plaintexts = {
      "An algorithm must be seen to ", "be believed.-----------------",
      "Computers are useless. They  ", "can only give you answers.---",
      "------------Code never lies, ", "comments sometimes do.-------",
  };
  CreateTokens(/*private_key=*/12345, plaintexts);
  std::vector<ProbabilisticRevealToken> tokens = GetTokens();
  tokens.back().u = "invalid-u";

  base::expected<std::unique_ptr<IpProtectionProbabilisticRevealTokenCrypter>,
                 absl::Status>
      maybe_crypter = IpProtectionProbabilisticRevealTokenCrypter::Create(
          GetPublicKey(), tokens);
  EXPECT_EQ(maybe_crypter.error().code(), absl::StatusCode::kInvalidArgument);
}

TEST_F(IpProtectionProbabilisticRevealTokenCrypterTest, CreateInvalidTokenE) {
  const std::vector<std::string> plaintexts = {
      "An algorithm must be seen to ", "be believed.-----------------",
      "Computers are useless. They  ", "can only give you answers.---",
      "------------Code never lies, ", "comments sometimes do.-------",
  };
  CreateTokens(/*private_key=*/12345, plaintexts);
  std::vector<ProbabilisticRevealToken> tokens = GetTokens();
  tokens.back().e = "invalid-e";

  base::expected<std::unique_ptr<IpProtectionProbabilisticRevealTokenCrypter>,
                 absl::Status>
      maybe_crypter = IpProtectionProbabilisticRevealTokenCrypter::Create(
          GetPublicKey(), tokens);
  EXPECT_EQ(maybe_crypter.error().code(), absl::StatusCode::kInvalidArgument);
}

TEST_F(IpProtectionProbabilisticRevealTokenCrypterTest,
       SetNewPublicKeyAndTokensSuccess) {
  const std::vector<std::string> plaintexts = {
      "An algorithm must be seen to ", "be believed.-----------------",
      "Computers are useless. They  ", "can only give you answers.---",
      "------------Code never lies, ", "comments sometimes do.-------",
  };
  CreateTokens(/*private_key=*/11111, plaintexts);

  // Create a crypter with no tokens.
  base::expected<std::unique_ptr<IpProtectionProbabilisticRevealTokenCrypter>,
                 absl::Status>
      maybe_crypter = IpProtectionProbabilisticRevealTokenCrypter::Create(
          GetPublicKey(), {});
  EXPECT_TRUE(maybe_crypter.has_value());
  auto& crypter = maybe_crypter.value();
  EXPECT_FALSE(crypter->IsTokenAvailable());
  EXPECT_EQ(crypter->NumTokens(), std::size_t(0));

  // Create new tokens with a new key.
  CreateTokens(/*private_key=*/22222, plaintexts);

  auto status = crypter->SetNewPublicKeyAndTokens(GetPublicKey(), GetTokens());

  EXPECT_TRUE(status.ok());
  EXPECT_TRUE(crypter->IsTokenAvailable());
  EXPECT_EQ(crypter->NumTokens(), plaintexts.size());
}

TEST_F(IpProtectionProbabilisticRevealTokenCrypterTest,
       SetNewPublicKeyAndTokensEmptyTokens) {
  const std::vector<std::string> plaintexts = {
      "An algorithm must be seen to ", "be believed.-----------------",
      "Computers are useless. They  ", "can only give you answers.---",
      "------------Code never lies, ", "comments sometimes do.-------",
  };
  CreateTokens(/*private_key=*/11111, plaintexts);
  base::expected<std::unique_ptr<IpProtectionProbabilisticRevealTokenCrypter>,
                 absl::Status>
      maybe_crypter = IpProtectionProbabilisticRevealTokenCrypter::Create(
          GetPublicKey(), {});
  EXPECT_TRUE(maybe_crypter.has_value());
  auto& crypter = maybe_crypter.value();
  EXPECT_FALSE(crypter->IsTokenAvailable());
  EXPECT_EQ(crypter->NumTokens(), std::size_t(0));

  // Create new tokens with a new key.
  CreateTokens(/*private_key=*/22222, plaintexts);

  auto status = crypter->SetNewPublicKeyAndTokens(GetPublicKey(), {});
  EXPECT_TRUE(status.ok());
  EXPECT_FALSE(crypter->IsTokenAvailable());
  EXPECT_EQ(crypter->NumTokens(), std::size_t(0));
}

TEST_F(IpProtectionProbabilisticRevealTokenCrypterTest,
       SetNewPublicKeyAndTokensInvalidPublicKey) {
  const std::vector<std::string> plaintexts = {
      "------------Code never lies, ",
      "comments sometimes do.-------",
  };
  CreateTokens(/*private_key=*/42, plaintexts);

  base::expected<std::unique_ptr<IpProtectionProbabilisticRevealTokenCrypter>,
                 absl::Status>
      maybe_crypter = IpProtectionProbabilisticRevealTokenCrypter::Create(
          GetPublicKey(), GetTokens());
  EXPECT_TRUE(maybe_crypter.has_value());
  auto& crypter = maybe_crypter.value();
  EXPECT_TRUE(crypter->IsTokenAvailable());
  EXPECT_EQ(crypter->NumTokens(), plaintexts.size());

  // Should fail and crypter should keep its state.
  auto status = crypter->SetNewPublicKeyAndTokens("invalid-pk", {});
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_TRUE(crypter->IsTokenAvailable());
  EXPECT_EQ(crypter->NumTokens(), plaintexts.size());
}

TEST_F(IpProtectionProbabilisticRevealTokenCrypterTest,
       SetNewPublicKeyAndTokensInvalidTokenU) {
  const std::vector<std::string> plaintexts = {
      "An algorithm must be seen to ", "be believed.-----------------",
      "Computers are useless. They  ", "can only give you answers.---",
      "------------Code never lies, ", "comments sometimes do.-------",
  };
  CreateTokens(/*private_key=*/42, plaintexts);

  base::expected<std::unique_ptr<IpProtectionProbabilisticRevealTokenCrypter>,
                 absl::Status>
      maybe_crypter = IpProtectionProbabilisticRevealTokenCrypter::Create(
          GetPublicKey(), GetTokens());
  EXPECT_TRUE(maybe_crypter.has_value());
  auto& crypter = maybe_crypter.value();
  EXPECT_TRUE(crypter->IsTokenAvailable());
  EXPECT_EQ(crypter->NumTokens(), plaintexts.size());

  // Create new tokens with a new key.
  CreateTokens(
      /*private_key=*/23,
      std::vector<std::string>(plaintexts.begin(), plaintexts.begin() + 2));
  std::vector<ProbabilisticRevealToken> new_tokens = GetTokens();
  new_tokens[1].u = "not-a-valid-token-u";

  // Should fail and crypter should keep its state.
  auto status = crypter->SetNewPublicKeyAndTokens(GetPublicKey(), new_tokens);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_TRUE(crypter->IsTokenAvailable());
  // NumTokens() should remain as plaintexts.size() not 2.
  EXPECT_EQ(crypter->NumTokens(), plaintexts.size());
}

TEST_F(IpProtectionProbabilisticRevealTokenCrypterTest,
       SetNewPublicKeyAndTokensInvalidTokenE) {
  const std::vector<std::string> plaintexts = {
      "An algorithm must be seen to ", "be believed.-----------------",
      "Computers are useless. They  ", "can only give you answers.---",
      "------------Code never lies, ", "comments sometimes do.-------",
  };
  CreateTokens(/*private_key=*/42, plaintexts);

  base::expected<std::unique_ptr<IpProtectionProbabilisticRevealTokenCrypter>,
                 absl::Status>
      maybe_crypter = IpProtectionProbabilisticRevealTokenCrypter::Create(
          GetPublicKey(), GetTokens());
  EXPECT_TRUE(maybe_crypter.has_value());
  auto& crypter = maybe_crypter.value();
  EXPECT_TRUE(crypter->IsTokenAvailable());
  EXPECT_EQ(crypter->NumTokens(), plaintexts.size());

  // Create new tokens with a new key.
  CreateTokens(
      /*private_key=*/23,
      std::vector<std::string>(plaintexts.begin(), plaintexts.begin() + 2));
  std::vector<ProbabilisticRevealToken> new_tokens = GetTokens();
  new_tokens[1].e = "not-a-valid-token-e";

  // Should fail and crypter should keep its state.
  auto status = crypter->SetNewPublicKeyAndTokens(GetPublicKey(), new_tokens);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_TRUE(crypter->IsTokenAvailable());
  // NumTokens() should remain as plaintexts.size() not 2.
  EXPECT_EQ(crypter->NumTokens(), plaintexts.size());
}

TEST_F(IpProtectionProbabilisticRevealTokenCrypterTest, ClearTokens) {
  const std::vector<std::string> plaintexts = {
      "An algorithm must be seen to ", "be believed.-----------------",
      "Computers are useless. They  ", "can only give you answers.---",
      "------------Code never lies, ", "comments sometimes do.-------",
  };
  CreateTokens(/*private_key=*/7654, plaintexts);
  base::expected<std::unique_ptr<IpProtectionProbabilisticRevealTokenCrypter>,
                 absl::Status>
      maybe_crypter = IpProtectionProbabilisticRevealTokenCrypter::Create(
          GetPublicKey(), GetTokens());
  EXPECT_TRUE(maybe_crypter.has_value());
  auto& crypter = maybe_crypter.value();
  EXPECT_TRUE(crypter->IsTokenAvailable());
  EXPECT_EQ(crypter->NumTokens(), plaintexts.size());

  crypter->ClearTokens();
  EXPECT_FALSE(crypter->IsTokenAvailable());
  EXPECT_EQ(crypter->NumTokens(), std::size_t(0));
}

TEST_F(IpProtectionProbabilisticRevealTokenCrypterTest, Randomize) {
  const std::vector<std::string> plaintexts = {
      "An algorithm must be seen to ", "be believed.-----------------",
      "Computers are useless. They  ", "can only give you answers.---",
      "------------Code never lies, ", "comments sometimes do.-------",
      "----random-string----------, ", "--another-random-str---------",
      "some-prob-reveal-token-----, ", "--another-random-pr-token----",
  };
  CreateTokens(/*private_key=*/2025, plaintexts);

  base::expected<std::unique_ptr<IpProtectionProbabilisticRevealTokenCrypter>,
                 absl::Status>
      maybe_crypter = IpProtectionProbabilisticRevealTokenCrypter::Create(
          GetPublicKey(), GetTokens());
  EXPECT_TRUE(maybe_crypter.has_value());
  const auto& crypter = maybe_crypter.value();
  EXPECT_TRUE(crypter->IsTokenAvailable());
  EXPECT_EQ(crypter->NumTokens(), plaintexts.size());

  // Decrypting randomized tokens should yield starting plaintexts.
  for (std::size_t i = 0; i < plaintexts.size(); ++i) {
    const base::expected<ProbabilisticRevealToken, absl::Status>
        maybe_randomized_token = crypter->Randomize(i);
    ASSERT_TRUE(maybe_randomized_token.has_value());
    const auto& randomized_token = maybe_randomized_token.value();

    EXPECT_EQ(randomized_token.version, 1);
    base::expected<std::string, absl::Status> maybe_revealed_token =
        issuer_->RevealToken(randomized_token);
    ASSERT_TRUE(maybe_revealed_token.has_value())
        << "decrypting randomized token failed, error: "
        << maybe_revealed_token.error();
    EXPECT_EQ(maybe_revealed_token.value(), plaintexts[i]);
  }

  // Try to randomize a token that does not exist.
  const base::expected<ProbabilisticRevealToken, absl::Status> maybe_token =
      crypter->Randomize(plaintexts.size());
  ASSERT_FALSE(maybe_token.has_value());
  EXPECT_EQ(maybe_token.error().code(), absl::StatusCode::kInvalidArgument);
}

}  // namespace ip_protection
