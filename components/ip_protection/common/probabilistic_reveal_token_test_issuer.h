// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_PROBABILISTIC_REVEAL_TOKEN_TEST_ISSUER_H_
#define COMPONENTS_IP_PROTECTION_COMMON_PROBABILISTIC_REVEAL_TOKEN_TEST_ISSUER_H_

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/memory/ptr_util.h"
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

using ::private_join_and_compute::Context;
using ::private_join_and_compute::ECGroup;
using ::private_join_and_compute::ECPoint;
using ::private_join_and_compute::ElGamalDecrypter;
using ::private_join_and_compute::ElGamalEncrypter;
using ::private_join_and_compute::elgamal::Ciphertext;
using ::private_join_and_compute::elgamal::PrivateKey;
using ::private_join_and_compute::elgamal::PublicKey;

// Implements a PRT issuer server capabilities, used to create/decrypt tokens
// for tests.
class ProbabilisticRevealTokenTestIssuer {
 public:
  static base::expected<std::unique_ptr<ProbabilisticRevealTokenTestIssuer>,
                        absl::Status>
  Create(uint64_t private_key, size_t num_tokens);

  ~ProbabilisticRevealTokenTestIssuer();

  // Serialized base64 encoded ECPoints encrypted by the Issuer
  // to produce `Tokens()`. `Tokens()[i]` is encrypted `Plaintexts()[i]`.
  const std::vector<std::string>& Plaintexts() const { return plaintexts_; }

  // Ciphertexts produced by the issuer. These are encrypted
  // `Plaintexts()`. Decrypting Tokens()[i] should yield
  // Plaintexts()[i].
  const std::vector<ProbabilisticRevealToken>& Tokens() const {
    return tokens_;
  }

  std::string GetSerializedPublicKey() const { return serialized_public_key_; }

  // Decrypt given token, serialize returned point, and base64 encode.
  base::expected<std::string, absl::Status> DecryptSerializeEncode(
      const ProbabilisticRevealToken& token);

  base::expected<std::vector<std::string>, absl::Status> DecryptSerializeEncode(
      const std::vector<ProbabilisticRevealToken>& tokens);

 private:
  static absl::StatusOr<std::unique_ptr<ProbabilisticRevealTokenTestIssuer>>
  CreateInternal(uint64_t private_key, size_t num_tokens);

  ProbabilisticRevealTokenTestIssuer(
      std::unique_ptr<Context> context,
      std::unique_ptr<ECGroup> group,
      std::unique_ptr<ElGamalEncrypter> encrypter,
      std::unique_ptr<ElGamalDecrypter> decrypter,
      std::string serialized_public_key,
      std::vector<std::string> plaintexts,
      std::vector<ProbabilisticRevealToken> tokens);

  absl::StatusOr<std::string> DecryptSerializeEncodeInternal(
      const ProbabilisticRevealToken& token);

  absl::StatusOr<std::vector<std::string>> DecryptSerializeEncodeInternal(
      const std::vector<ProbabilisticRevealToken>& tokens);

  std::unique_ptr<const Context> context_;
  std::unique_ptr<const ECGroup> group_;
  std::unique_ptr<const ElGamalEncrypter> encrypter_;
  std::unique_ptr<const ElGamalDecrypter> decrypter_;
  const std::string serialized_public_key_;
  std::vector<std::string> plaintexts_;
  std::vector<ProbabilisticRevealToken> tokens_;
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_PROBABILISTIC_REVEAL_TOKEN_TEST_ISSUER_H_
