// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_probabilistic_reveal_token_crypter.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/sequence_checker.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "third_party/abseil-cpp/absl/status/statusor.h"
#include "third_party/private-join-and-compute/src/crypto/context.h"
#include "third_party/private-join-and-compute/src/crypto/ec_group.h"
#include "third_party/private-join-and-compute/src/crypto/ec_point.h"
#include "third_party/private-join-and-compute/src/crypto/elgamal.h"

namespace ip_protection {

using ::private_join_and_compute::Context;
using ::private_join_and_compute::ECGroup;
using ::private_join_and_compute::ECPoint;
using ::private_join_and_compute::ElGamalEncrypter;
using ::private_join_and_compute::elgamal::Ciphertext;
using ::private_join_and_compute::elgamal::PublicKey;

namespace {

// `group` should outlive returned encrypter. Used by static creator.
// In anonymous namespace to prevent misuse.
absl::StatusOr<std::unique_ptr<ElGamalEncrypter>> CreateEncrypter(
    ECGroup const* group,
    const std::string& serialized_public_key) {
  ASSIGN_OR_RETURN(ECPoint g, group->GetFixedGenerator());
  ASSIGN_OR_RETURN(ECPoint y, group->CreateECPoint(serialized_public_key));
  return std::make_unique<ElGamalEncrypter>(
      group,
      std::make_unique<PublicKey>(PublicKey{std::move(g), std::move(y)}));
}

// `group` should outlive returned ciphertexts. Used by static creator.
// In anonymous namespace to prevent misuse.
absl::StatusOr<std::vector<Ciphertext>> CreateCiphertext(
    ECGroup const* group,
    const std::vector<ProbabilisticRevealToken>& tokens) {
  std::vector<Ciphertext> ciphertext;
  ciphertext.reserve(tokens.size());
  for (const auto& t : tokens) {
    ASSIGN_OR_RETURN(ECPoint u, group->CreateECPoint(t.u));
    ASSIGN_OR_RETURN(ECPoint e, group->CreateECPoint(t.e));
    ciphertext.emplace_back(std::move(u), std::move(e));
  }
  return ciphertext;
}

}  // namespace

absl::StatusOr<std::unique_ptr<IpProtectionProbabilisticRevealTokenCrypter>>
IpProtectionProbabilisticRevealTokenCrypter::Create(
    const std::string& serialized_public_key,
    const std::vector<ProbabilisticRevealToken>& tokens) {
  auto context = std::make_unique<Context>();
  std::unique_ptr<ECGroup> group;
  {
    ASSIGN_OR_RETURN(ECGroup local_group,
                     ECGroup::Create(NID_secp224r1, context.get()));
    group = std::make_unique<ECGroup>(std::move(local_group));
  }
  ASSIGN_OR_RETURN(std::unique_ptr<ElGamalEncrypter> encrypter,
                   CreateEncrypter(group.get(), serialized_public_key));
  ASSIGN_OR_RETURN(std::vector<Ciphertext> ciphertext,
                   CreateCiphertext(group.get(), tokens));
  // Can not use `make_unique` since constructor is private.
  // Can not use `return std::unique_ptr`, git cl upload
  // returns pre-submit error and recommends using base::WrapUnique.
  return base::WrapUnique<IpProtectionProbabilisticRevealTokenCrypter>(
      new IpProtectionProbabilisticRevealTokenCrypter(
          std::move(context), std::move(group), std::move(encrypter),
          std::move(ciphertext)));
}

IpProtectionProbabilisticRevealTokenCrypter::
    ~IpProtectionProbabilisticRevealTokenCrypter() = default;

IpProtectionProbabilisticRevealTokenCrypter::
    IpProtectionProbabilisticRevealTokenCrypter(
        std::unique_ptr<Context> context,
        std::unique_ptr<ECGroup> group,
        std::unique_ptr<ElGamalEncrypter> encrypter,
        std::vector<Ciphertext> ciphertext)
    : context_(std::move(context)),
      group_(std::move(group)),
      encrypter_(std::move(encrypter)),
      ciphertext_(std::move(ciphertext)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

absl::Status
IpProtectionProbabilisticRevealTokenCrypter::SetNewPublicKeyAndTokens(
    const std::string& serialized_public_key,
    const std::vector<ProbabilisticRevealToken>& tokens) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ASSIGN_OR_RETURN(std::unique_ptr<ElGamalEncrypter> encrypter,
                   CreateEncrypter(group_.get(), serialized_public_key));
  ASSIGN_OR_RETURN(std::vector<Ciphertext> ciphertext,
                   CreateCiphertext(group_.get(), tokens));
  // Creating encrypter and ciphertext succeeded, set members.
  encrypter_.reset(encrypter.release());
  ciphertext_ = std::move(ciphertext);
  return absl::OkStatus();
}

bool IpProtectionProbabilisticRevealTokenCrypter::IsTokenAvailable() const {
  return ciphertext_.size();
}

void IpProtectionProbabilisticRevealTokenCrypter::ClearTokens() {
  ciphertext_.clear();
}

absl::StatusOr<ProbabilisticRevealToken>
IpProtectionProbabilisticRevealTokenCrypter::Randomize(size_t i) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (i >= ciphertext_.size()) {
    return absl::InvalidArgumentError("invalid index");
  }
  ASSIGN_OR_RETURN(Ciphertext randomized_ciphertext,
                   encrypter_->ReRandomize(ciphertext_[i]));
  ProbabilisticRevealToken randomized_token;
  randomized_token.version = 1;
  ASSIGN_OR_RETURN(randomized_token.u,
                   randomized_ciphertext.u.ToBytesCompressed());
  ASSIGN_OR_RETURN(randomized_token.e,
                   randomized_ciphertext.e.ToBytesCompressed());
  return randomized_token;
}

size_t IpProtectionProbabilisticRevealTokenCrypter::NumTokens() const {
  return ciphertext_.size();
}

}  // namespace ip_protection
