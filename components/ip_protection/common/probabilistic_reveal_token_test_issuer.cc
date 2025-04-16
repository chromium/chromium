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
  absl::StatusOr<std::unique_ptr<ProbabilisticRevealTokenTestIssuer>>
      maybe_issuer = CreateInternal(private_key);
  if (!maybe_issuer.ok()) {
    return base::unexpected(maybe_issuer.status());
  }
  return base::ok(std::move(maybe_issuer.value()));
}

absl::StatusOr<std::unique_ptr<ProbabilisticRevealTokenTestIssuer>>
ProbabilisticRevealTokenTestIssuer::CreateInternal(uint64_t private_key) {
  auto context = std::make_unique<Context>();
  std::unique_ptr<ECGroup> group;
  {
    ASSIGN_OR_RETURN(ECGroup local_group,
                     ECGroup::Create(NID_X9_62_prime256v1, context.get()));
    group = std::make_unique<ECGroup>(std::move(local_group));
  }

  std::unique_ptr<ElGamalEncrypter> encrypter;
  std::string serialized_public_key;
  {
    ASSIGN_OR_RETURN(ECPoint g, group->GetFixedGenerator());
    ASSIGN_OR_RETURN(ECPoint y, g.Mul(context->CreateBigNum(private_key)));
    ASSIGN_OR_RETURN(serialized_public_key, y.ToBytesCompressed());
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
  GetProbabilisticRevealTokenResponse response_proto;
  for (const auto& pi : plaintexts) {
    GetProbabilisticRevealTokenResponse_ProbabilisticRevealToken* token =
        response_proto.add_tokens();
    absl::StatusOr<ProbabilisticRevealToken> maybe_token = IssueInternal(pi);
    if (!maybe_token.ok()) {
      return base::unexpected(maybe_token.status());
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
  absl::StatusOr<std::string> maybe_plaintext = RevealTokenInternal(token);
  if (!maybe_plaintext.ok()) {
    return base::unexpected(maybe_plaintext.status());
  }
  return base::ok(std::move(maybe_plaintext.value()));
}

absl::StatusOr<std::string>
ProbabilisticRevealTokenTestIssuer::RevealTokenInternal(
    const ProbabilisticRevealToken& token) const {
  ASSIGN_OR_RETURN(ECPoint point, Decrypt(token));
  ASSIGN_OR_RETURN(BigNum big_num, group_->RecoverXFromPaddedPoint(
                                       point, kPaddingSize * kBitsPerByte));
  return big_num.ToBytes();
}

absl::StatusOr<ProbabilisticRevealToken>
ProbabilisticRevealTokenTestIssuer::IssueInternal(
    const std::string& plaintext) const {
  if (plaintext.size() != kPlaintextSize) {
    return absl::InvalidArgumentError("plaintext size must be kPlaintextSize");
  }
  ASSIGN_OR_RETURN(ECPoint plaintext_point,
                   group_->GetPointByPaddingX(
                       context_->CreateBigNum(plaintext),
                       /*padding_bit_count=*/kPaddingSize * kBitsPerByte));
  ASSIGN_OR_RETURN(std::string serialized_plaintext_point,
                   plaintext_point.ToBytesCompressed());
  ASSIGN_OR_RETURN(Ciphertext ciphertext, encrypter_->Encrypt(plaintext_point));
  ASSIGN_OR_RETURN(std::string u_compressed, ciphertext.u.ToBytesCompressed());
  ASSIGN_OR_RETURN(std::string e_compressed, ciphertext.e.ToBytesCompressed());
  return ProbabilisticRevealToken(1, std::move(u_compressed),
                                  std::move(e_compressed));
}

base::expected<std::string, absl::Status>
ProbabilisticRevealTokenTestIssuer::DecryptSerializeEncode(
    const ProbabilisticRevealToken& token) {
  absl::StatusOr<std::string> maybe_serialized_point =
      DecryptSerializeEncodeInternal(token);
  if (!maybe_serialized_point.ok()) {
    return base::unexpected(maybe_serialized_point.status());
  }
  return base::ok(std::move(maybe_serialized_point.value()));
}

base::expected<std::vector<std::string>, absl::Status>
ProbabilisticRevealTokenTestIssuer::DecryptSerializeEncode(
    const std::vector<ProbabilisticRevealToken>& tokens) {
  absl::StatusOr<std::vector<std::string>> maybe_serialized_points =
      DecryptSerializeEncodeInternal(tokens);
  if (!maybe_serialized_points.ok()) {
    return base::unexpected(maybe_serialized_points.status());
  }
  return base::ok(std::move(maybe_serialized_points.value()));
}

absl::StatusOr<std::string>
ProbabilisticRevealTokenTestIssuer::DecryptSerializeEncodeInternal(
    const ProbabilisticRevealToken& token) {
  ASSIGN_OR_RETURN(ECPoint point, Decrypt(token));
  ASSIGN_OR_RETURN(std::string serialized_point, point.ToBytesCompressed());
  return base::Base64Encode(serialized_point);
}

absl::StatusOr<std::vector<std::string>>
ProbabilisticRevealTokenTestIssuer::DecryptSerializeEncodeInternal(
    const std::vector<ProbabilisticRevealToken>& tokens) {
  std::vector<std::string> encoded;
  for (const auto& t : tokens) {
    ASSIGN_OR_RETURN(std::string sp, DecryptSerializeEncodeInternal(t));
    encoded.push_back(std::move(sp));
  }
  return encoded;
}

absl::StatusOr<ECPoint> ProbabilisticRevealTokenTestIssuer::Decrypt(
    const ProbabilisticRevealToken& token) const {
  ASSIGN_OR_RETURN(ECPoint u, group_->CreateECPoint(token.u));
  ASSIGN_OR_RETURN(ECPoint e, group_->CreateECPoint(token.e));
  Ciphertext ciphertext{std::move(u), std::move(e)};
  return decrypter_->Decrypt(ciphertext);
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
