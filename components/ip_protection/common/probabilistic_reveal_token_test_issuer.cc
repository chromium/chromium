// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/probabilistic_reveal_token_test_issuer.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "third_party/abseil-cpp/absl/status/status.h"
#include "third_party/abseil-cpp/absl/status/statusor.h"
#include "third_party/private-join-and-compute/src/crypto/context.h"
#include "third_party/private-join-and-compute/src/crypto/ec_group.h"
#include "third_party/private-join-and-compute/src/crypto/ec_point.h"
#include "third_party/private-join-and-compute/src/crypto/elgamal.h"

namespace ip_protection {

using ::private_join_and_compute::BigNum;
using ::private_join_and_compute::Context;
using ::private_join_and_compute::ECGroup;
using ::private_join_and_compute::ECPoint;
using ::private_join_and_compute::ElGamalDecrypter;
using ::private_join_and_compute::ElGamalEncrypter;
using ::private_join_and_compute::elgamal::Ciphertext;
using ::private_join_and_compute::elgamal::PrivateKey;
using ::private_join_and_compute::elgamal::PublicKey;

namespace {
constexpr size_t kBitsPerByte = 8;
constexpr size_t kPlaintextSize = 29;
constexpr size_t kPaddingSize = 3;
}  // namespace

base::expected<std::unique_ptr<ProbabilisticRevealTokenTestIssuer>,
               absl::Status>
ProbabilisticRevealTokenTestIssuer::Create(uint64_t private_key) {
  auto context = std::make_unique<Context>();
  std::unique_ptr<ECGroup> group;
  {
    absl::StatusOr<ECGroup> maybe_group =
        ECGroup::Create(NID_X9_62_prime256v1, context.get());
    if (!maybe_group.ok()) {
      return base::unexpected(maybe_group.status());
    }
    group = std::make_unique<ECGroup>(std::move(maybe_group).value());
  }

  std::unique_ptr<ElGamalEncrypter> encrypter;
  std::string serialized_public_key;
  {
    absl::StatusOr<ECPoint> maybe_g = group->GetFixedGenerator();
    if (!maybe_g.ok()) {
      return base::unexpected(maybe_g.status());
    }
    ECPoint g = std::move(maybe_g).value();

    absl::StatusOr<ECPoint> maybe_y = g.Mul(context->CreateBigNum(private_key));
    if (!maybe_y.ok()) {
      return base::unexpected(maybe_y.status());
    }
    ECPoint y = std::move(maybe_y).value();

    absl::StatusOr<std::string> maybe_serialized_public_key =
        y.ToBytesCompressed();
    if (!maybe_serialized_public_key.ok()) {
      return base::unexpected(maybe_serialized_public_key.status());
    }

    serialized_public_key = std::move(maybe_serialized_public_key).value();
    encrypter = std::make_unique<ElGamalEncrypter>(
        group.get(), std::make_unique<PublicKey>(std::move(g), std::move(y)));
  }

  auto decrypter = std::make_unique<ElGamalDecrypter>(
      std::make_unique<PrivateKey>(context->CreateBigNum(private_key)));
  return base::WrapUnique<ProbabilisticRevealTokenTestIssuer>(
      new ProbabilisticRevealTokenTestIssuer(
          std::move(context), std::move(group), std::move(encrypter),
          std::move(decrypter), std::move(serialized_public_key)));
}

base::expected<GetProbabilisticRevealTokenResponse, absl::Status>
ProbabilisticRevealTokenTestIssuer::Issue(std::vector<std::string> plaintexts,
                                          base::Time expiration,
                                          base::Time next_epoch_start,
                                          int32_t num_tokens_with_signal,
                                          std::string epoch_id) {
  tokens_.clear();
  std::vector<ECPoint> plaintext_points;
  plaintext_points.reserve(plaintexts.size());
  for (const auto& pi : plaintexts) {
    base::expected<ECPoint, absl::Status> maybe_plaintext_point =
        GetPointByPadding(pi);
    if (!maybe_plaintext_point.has_value()) {
      return base::unexpected(maybe_plaintext_point.error());
    }
    plaintext_points.push_back(std::move(maybe_plaintext_point).value());
  }
  return IssueFromPoints(std::move(plaintext_points), expiration,
                         next_epoch_start, num_tokens_with_signal, epoch_id);
}

base::expected<GetProbabilisticRevealTokenResponse, absl::Status>
ProbabilisticRevealTokenTestIssuer::IssueByHashingToPoint(
    std::vector<std::string> plaintexts,
    base::Time expiration,
    base::Time next_epoch_start,
    int32_t num_tokens_with_signal,
    std::string epoch_id) {
  tokens_.clear();
  std::vector<ECPoint> plaintext_points;
  plaintext_points.reserve(plaintexts.size());
  for (const auto& pi : plaintexts) {
    base::expected<ECPoint, absl::Status> maybe_plaintext_point =
        GetPointByHashing(pi);
    if (!maybe_plaintext_point.has_value()) {
      return base::unexpected(maybe_plaintext_point.error());
    }
    plaintext_points.push_back(std::move(maybe_plaintext_point).value());
  }
  return IssueFromPoints(std::move(plaintext_points), expiration,
                         next_epoch_start, num_tokens_with_signal, epoch_id);
}

base::expected<GetProbabilisticRevealTokenResponse, absl::Status>
ProbabilisticRevealTokenTestIssuer::IssueFromPoints(
    std::vector<private_join_and_compute::ECPoint> plaintext_points,
    base::Time expiration,
    base::Time next_epoch_start,
    int32_t num_tokens_with_signal,
    std::string epoch_id) {
  GetProbabilisticRevealTokenResponse response_proto;
  for (const auto& pi : plaintext_points) {
    GetProbabilisticRevealTokenResponse_ProbabilisticRevealToken* token =
        response_proto.add_tokens();
    base::expected<ProbabilisticRevealToken, absl::Status> maybe_token =
        Encrypt(pi);
    if (!maybe_token.has_value()) {
      return base::unexpected(maybe_token.error());
    }
    tokens_.push_back(maybe_token.value());
    token->set_version(maybe_token.value().version);
    token->set_u(std::move(maybe_token.value().u));
    token->set_e(std::move(maybe_token.value().e));
  }
  response_proto.mutable_public_key()->set_y(serialized_public_key_);
  response_proto.mutable_expiration_time()->set_seconds(
      expiration.InSecondsFSinceUnixEpoch());
  response_proto.mutable_next_epoch_start_time()->set_seconds(
      next_epoch_start.InSecondsFSinceUnixEpoch());
  response_proto.set_num_tokens_with_signal(num_tokens_with_signal);
  response_proto.set_epoch_id(epoch_id);
  return base::ok(std::move(response_proto));
}

base::expected<std::string, absl::Status>
ProbabilisticRevealTokenTestIssuer::RevealToken(
    const ProbabilisticRevealToken& token) const {
  base::expected<ECPoint, absl::Status> maybe_point = Decrypt(token);
  if (!maybe_point.has_value()) {
    return base::unexpected(maybe_point.error());
  }

  absl::StatusOr<BigNum> big_num = group_->RecoverXFromPaddedPoint(
      maybe_point.value(), kPaddingSize * kBitsPerByte);
  if (!big_num.ok()) {
    return base::unexpected(big_num.status());
  }
  return std::move(big_num)->ToBytes();
}

base::expected<ProbabilisticRevealToken, absl::Status>
ProbabilisticRevealTokenTestIssuer::Encrypt(const ECPoint& point) const {
  absl::StatusOr<Ciphertext> maybe_ciphertext = encrypter_->Encrypt(point);
  if (!maybe_ciphertext.ok()) {
    return base::unexpected(maybe_ciphertext.status());
  }
  const auto& ciphertext = maybe_ciphertext.value();

  absl::StatusOr<std::string> maybe_u_compressed =
      ciphertext.u.ToBytesCompressed();
  if (!maybe_u_compressed.ok()) {
    return base::unexpected(maybe_u_compressed.status());
  }

  absl::StatusOr<std::string> maybe_e_compressed =
      ciphertext.e.ToBytesCompressed();
  if (!maybe_e_compressed.ok()) {
    return base::unexpected(maybe_e_compressed.status());
  }

  return ProbabilisticRevealToken(1, std::move(maybe_u_compressed).value(),
                                  std::move(maybe_e_compressed).value());
}

base::expected<ECPoint, absl::Status>
ProbabilisticRevealTokenTestIssuer::GetPointByPadding(
    std::string plaintext) const {
  if (plaintext.size() != kPlaintextSize) {
    return base::unexpected(
        absl::InvalidArgumentError("plaintext size must be kPlaintextSize"));
  }
  absl::StatusOr<ECPoint> maybe_plaintext_point = group_->GetPointByPaddingX(
      context_->CreateBigNum(plaintext),
      /*padding_bit_count=*/kPaddingSize * kBitsPerByte);
  if (!maybe_plaintext_point.ok()) {
    return base::unexpected(maybe_plaintext_point.status());
  }
  return std::move(maybe_plaintext_point).value();
}

base::expected<ECPoint, absl::Status>
ProbabilisticRevealTokenTestIssuer::GetPointByHashing(
    std::string message) const {
  absl::StatusOr<ECPoint> maybe_plaintext_point =
      group_->GetPointByHashingToCurveSha256(message);
  if (!maybe_plaintext_point.ok()) {
    return base::unexpected(maybe_plaintext_point.status());
  }
  return std::move(maybe_plaintext_point).value();
}

base::expected<std::string, absl::Status>
ProbabilisticRevealTokenTestIssuer::DecryptSerializeEncode(
    const ProbabilisticRevealToken& token) {
  base::expected<ECPoint, absl::Status> maybe_point = Decrypt(token);
  if (!maybe_point.has_value()) {
    return base::unexpected(maybe_point.error());
  }
  absl::StatusOr<std::string> maybe_serialized_point =
      maybe_point.value().ToBytesCompressed();
  if (!maybe_serialized_point.ok()) {
    return base::unexpected(maybe_serialized_point.status());
  }
  return base::Base64Encode(maybe_serialized_point.value());
}

base::expected<std::vector<std::string>, absl::Status>
ProbabilisticRevealTokenTestIssuer::DecryptSerializeEncode(
    const std::vector<ProbabilisticRevealToken>& tokens) {
  std::vector<std::string> encoded;
  for (const auto& t : tokens) {
    base::expected<std::string, absl::Status> maybe_sp =
        DecryptSerializeEncode(t);
    if (!maybe_sp.has_value()) {
      return base::unexpected(maybe_sp.error());
    }
    encoded.push_back(std::move(maybe_sp).value());
  }
  return encoded;
}

base::expected<ECPoint, absl::Status>
ProbabilisticRevealTokenTestIssuer::Decrypt(
    const ProbabilisticRevealToken& token) const {
  absl::StatusOr<ECPoint> maybe_u = group_->CreateECPoint(token.u);
  if (!maybe_u.ok()) {
    return base::unexpected(maybe_u.status());
  }
  absl::StatusOr<ECPoint> maybe_e = group_->CreateECPoint(token.e);
  if (!maybe_e.ok()) {
    return base::unexpected(maybe_e.status());
  }
  Ciphertext ciphertext{std::move(maybe_u).value(), std::move(maybe_e).value()};
  absl::StatusOr<ECPoint> decrypted_point = decrypter_->Decrypt(ciphertext);
  if (!decrypted_point.ok()) {
    return base::unexpected(decrypted_point.status());
  }
  return std::move(decrypted_point).value();
}

ProbabilisticRevealTokenTestIssuer::ProbabilisticRevealTokenTestIssuer(
    std::unique_ptr<Context> context,
    std::unique_ptr<ECGroup> group,
    std::unique_ptr<ElGamalEncrypter> encrypter,
    std::unique_ptr<ElGamalDecrypter> decrypter,
    std::string serialized_public_key)
    : context_(std::move(context)),
      group_(std::move(group)),
      encrypter_(std::move(encrypter)),
      decrypter_(std::move(decrypter)),
      serialized_public_key_(std::move(serialized_public_key)) {}

ProbabilisticRevealTokenTestIssuer::~ProbabilisticRevealTokenTestIssuer() =
    default;

}  // namespace ip_protection
