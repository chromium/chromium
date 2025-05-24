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
#include "base/types/expected.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/ip_protection_telemetry.h"
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
base::expected<std::unique_ptr<ElGamalEncrypter>, absl::Status> CreateEncrypter(
    ECGroup const* group,
    const std::string& serialized_public_key) {
  absl::StatusOr<ECPoint> maybe_g = group->GetFixedGenerator();
  if (!maybe_g.ok()) {
    return base::unexpected(maybe_g.status());
  }

  absl::StatusOr<ECPoint> maybe_y = group->CreateECPoint(serialized_public_key);
  if (!maybe_y.ok()) {
    return base::unexpected(maybe_y.status());
  }
  return std::make_unique<ElGamalEncrypter>(
      group, std::make_unique<PublicKey>(PublicKey{
                 std::move(maybe_g).value(), std::move(maybe_y).value()}));
}

// `group` should outlive returned ciphertexts. Used by static creator.
// In anonymous namespace to prevent misuse.
base::expected<std::vector<Ciphertext>, absl::Status> CreateCiphertext(
    ECGroup const* group,
    const std::vector<ProbabilisticRevealToken>& tokens) {
  std::vector<Ciphertext> ciphertext;
  ciphertext.reserve(tokens.size());
  for (const auto& t : tokens) {
    absl::StatusOr<ECPoint> maybe_u = group->CreateECPoint(t.u);
    if (!maybe_u.ok()) {
      return base::unexpected(maybe_u.status());
    }
    absl::StatusOr<ECPoint> maybe_e = group->CreateECPoint(t.e);
    if (!maybe_e.ok()) {
      return base::unexpected(maybe_e.status());
    }
    ciphertext.emplace_back(std::move(maybe_u).value(),
                            std::move(maybe_e).value());
  }
  return ciphertext;
}

}  // namespace

base::expected<std::unique_ptr<IpProtectionProbabilisticRevealTokenCrypter>,
               absl::Status>
IpProtectionProbabilisticRevealTokenCrypter::Create(
    const std::string& serialized_public_key,
    const std::vector<ProbabilisticRevealToken>& tokens) {
  auto context = std::make_unique<Context>();
  absl::StatusOr<ECGroup> local_group =
      ECGroup::Create(NID_X9_62_prime256v1, context.get());
  if (!local_group.ok()) {
    return base::unexpected(local_group.status());
  }
  std::unique_ptr<ECGroup> group =
      std::make_unique<ECGroup>(std::move(local_group).value());
  base::expected<std::unique_ptr<ElGamalEncrypter>, absl::Status>
      maybe_encrypter = CreateEncrypter(group.get(), serialized_public_key);
  if (!maybe_encrypter.has_value()) {
    return base::unexpected(maybe_encrypter.error());
  }

  base::expected<std::vector<Ciphertext>, absl::Status> maybe_ciphertext =
      CreateCiphertext(group.get(), tokens);
  if (!maybe_ciphertext.has_value()) {
    return base::unexpected(maybe_ciphertext.error());
  }

  // Can not use `make_unique` since constructor is private.
  // Can not use `return std::unique_ptr`, git cl upload
  // returns pre-submit error and recommends using base::WrapUnique.
  return base::WrapUnique<IpProtectionProbabilisticRevealTokenCrypter>(
      new IpProtectionProbabilisticRevealTokenCrypter(
          std::move(context), std::move(group),
          std::move(maybe_encrypter).value(),
          std::move(maybe_ciphertext).value()));
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
  base::expected<std::unique_ptr<ElGamalEncrypter>, absl::Status>
      maybe_encrypter = CreateEncrypter(group_.get(), serialized_public_key);
  if (!maybe_encrypter.has_value()) {
    return maybe_encrypter.error();
  }

  base::expected<std::vector<Ciphertext>, absl::Status> maybe_ciphertext =
      CreateCiphertext(group_.get(), tokens);
  if (!maybe_ciphertext.has_value()) {
    return maybe_ciphertext.error();
  }

  // Creating encrypter and ciphertext succeeded, set members.
  encrypter_ = std::move(maybe_encrypter).value();

  ciphertext_ = std::move(maybe_ciphertext).value();
  return absl::OkStatus();
}

bool IpProtectionProbabilisticRevealTokenCrypter::IsTokenAvailable() const {
  return ciphertext_.size();
}

void IpProtectionProbabilisticRevealTokenCrypter::ClearTokens() {
  ciphertext_.clear();
}

base::expected<ProbabilisticRevealToken, absl::Status>
IpProtectionProbabilisticRevealTokenCrypter::Randomize(size_t i) const {
  base::TimeTicks randomization_start_time = base::TimeTicks::Now();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (i >= ciphertext_.size()) {
    return base::unexpected(absl::InvalidArgumentError("invalid index"));
  }
  absl::StatusOr<Ciphertext> maybe_randomized_ciphertext =
      encrypter_->ReRandomize(ciphertext_[i]);
  if (!maybe_randomized_ciphertext.ok()) {
    return base::unexpected(maybe_randomized_ciphertext.status());
  }
  const auto& randomized_ciphertext = maybe_randomized_ciphertext.value();

  ProbabilisticRevealToken randomized_token;
  randomized_token.version = 1;
  absl::StatusOr<std::string> maybe_serialized_u =
      randomized_ciphertext.u.ToBytesCompressed();
  if (!maybe_serialized_u.ok()) {
    return base::unexpected(maybe_serialized_u.status());
  }
  randomized_token.u = std::move(maybe_serialized_u).value();

  absl::StatusOr<std::string> maybe_serialized_e =
      randomized_ciphertext.e.ToBytesCompressed();
  if (!maybe_serialized_e.ok()) {
    return base::unexpected(maybe_serialized_e.status());
  }
  randomized_token.e = std::move(maybe_serialized_e).value();

  Telemetry().ProbabilisticRevealTokenRandomizationTime(
      base::TimeTicks::Now() - randomization_start_time);
  return randomized_token;
}

size_t IpProtectionProbabilisticRevealTokenCrypter::NumTokens() const {
  return ciphertext_.size();
}

}  // namespace ip_protection
