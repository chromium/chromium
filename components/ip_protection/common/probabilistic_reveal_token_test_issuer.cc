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

base::expected<std::unique_ptr<ProbabilisticRevealTokenTestIssuer>,
               absl::Status>
ProbabilisticRevealTokenTestIssuer::Create(uint64_t private_key,
                                           size_t num_tokens) {
  absl::StatusOr<std::unique_ptr<ProbabilisticRevealTokenTestIssuer>>
      maybe_issuer = CreateInternal(private_key, num_tokens);
  if (!maybe_issuer.ok()) {
    return base::unexpected(maybe_issuer.status());
  }
  return base::ok(std::move(maybe_issuer.value()));
}

absl::StatusOr<std::unique_ptr<ProbabilisticRevealTokenTestIssuer>>
ProbabilisticRevealTokenTestIssuer::CreateInternal(uint64_t private_key,
                                                   size_t num_tokens) {
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

  std::vector<ProbabilisticRevealToken> tokens;
  std::vector<std::string> plaintexts;
  tokens.reserve(num_tokens);
  for (std::size_t i = 0; i < num_tokens; ++i) {
    ASSIGN_OR_RETURN(
        ECPoint plaintext_point,
        group->GetPointByHashingToCurveSha256(
            "awesome-probabilistic-reveal-token-" + base::NumberToString(i)));
    ASSIGN_OR_RETURN(std::string serialized_plaintext_point,
                     plaintext_point.ToBytesCompressed());
    plaintexts.push_back(base::Base64Encode(serialized_plaintext_point));
    ASSIGN_OR_RETURN(Ciphertext ciphertext,
                     encrypter->Encrypt(plaintext_point));
    ASSIGN_OR_RETURN(std::string u_compressed,
                     ciphertext.u.ToBytesCompressed());
    ASSIGN_OR_RETURN(std::string e_compressed,
                     ciphertext.e.ToBytesCompressed());
    tokens.emplace_back(1, std::move(u_compressed), std::move(e_compressed));
  }
  return base::WrapUnique<ProbabilisticRevealTokenTestIssuer>(
      new ProbabilisticRevealTokenTestIssuer(
          std::move(context), std::move(group), std::move(encrypter),
          std::move(decrypter), std::move(serialized_public_key),
          std::move(plaintexts), std::move(tokens)));
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
  ASSIGN_OR_RETURN(ECPoint u, group_->CreateECPoint(token.u));
  ASSIGN_OR_RETURN(ECPoint e, group_->CreateECPoint(token.e));
  Ciphertext ciphertext{std::move(u), std::move(e)};
  ASSIGN_OR_RETURN(ECPoint point, decrypter_->Decrypt(ciphertext));
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

ProbabilisticRevealTokenTestIssuer::ProbabilisticRevealTokenTestIssuer(
    std::unique_ptr<Context> context,
    std::unique_ptr<ECGroup> group,
    std::unique_ptr<ElGamalEncrypter> encrypter,
    std::unique_ptr<ElGamalDecrypter> decrypter,
    std::string serialized_public_key,
    std::vector<std::string> plaintexts,
    std::vector<ProbabilisticRevealToken> tokens)
    : context_(std::move(context)),
      group_(std::move(group)),
      encrypter_(std::move(encrypter)),
      decrypter_(std::move(decrypter)),
      serialized_public_key_(std::move(serialized_public_key)),
      plaintexts_(std::move(plaintexts)),
      tokens_(std::move(tokens)) {}

ProbabilisticRevealTokenTestIssuer::~ProbabilisticRevealTokenTestIssuer() =
    default;

}  // namespace ip_protection
