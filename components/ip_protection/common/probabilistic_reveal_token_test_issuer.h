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
#include "components/ip_protection/get_probabilistic_reveal_token.pb.h"
#include "third_party/abseil-cpp/absl/status/status.h"
#include "third_party/abseil-cpp/absl/status/statusor.h"
#include "third_party/private-join-and-compute/src/crypto/context.h"
#include "third_party/private-join-and-compute/src/crypto/ec_group.h"
#include "third_party/private-join-and-compute/src/crypto/ec_point.h"
#include "third_party/private-join-and-compute/src/crypto/elgamal.h"

namespace ip_protection {

// Implements a PRT issuer server capabilities, used to create/decrypt tokens
// for tests.
class ProbabilisticRevealTokenTestIssuer {
 public:
  static base::expected<std::unique_ptr<ProbabilisticRevealTokenTestIssuer>,
                        absl::Status>
  Create(uint64_t private_key);

  ~ProbabilisticRevealTokenTestIssuer();

  // Create a response proto type for a given set of arguments. Tokens in
  // response will contain encrypted `plaintexts`. Returns error if
  // `plaintexts[i].size()` is not 29. Updates `tokens_` to new ones. `tokens_`
  // is set to empty in case of failure, already existing ones (if any) are
  // cleared.
  base::expected<GetProbabilisticRevealTokenResponse, absl::Status> Issue(
      std::vector<std::string> plaintexts,
      base::Time expiration,
      base::Time next_epoch_start,
      int32_t num_tokens_with_signal,
      std::string epoch_id);

  // Create a response proto type for a given set of arguments. Tokens in
  // response will contain ECPoints obtained by hashing `plaintexts` to group.
  // Updates `tokens_` to new ones. `tokens_` is set to empty in case of
  // failure, already existing ones (if any) are cleared.
  base::expected<GetProbabilisticRevealTokenResponse, absl::Status>
  IssueByHashingToPoint(std::vector<std::string> plaintexts,
                        base::Time expiration,
                        base::Time next_epoch_start,
                        int32_t num_tokens_with_signal,
                        std::string epoch_id);

  // Decrypt a given `token` and return resulting string. `RevealToken()` will
  // return `plaintexts[i]` corresponding to the given `token`. See `Issue()`.
  base::expected<std::string, absl::Status> RevealToken(
      const ProbabilisticRevealToken& token) const;

  // PRTs produced by the `Issue()` call are encrypted plaintexts mapped to
  // points using `ECGroup::GetPointByPaddingX(plaintexts[i])`.
  // `RevealToken(Tokens()[i])` should yield `plaintexts[i]`.
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
  ProbabilisticRevealTokenTestIssuer(
      std::unique_ptr<private_join_and_compute::Context> context,
      std::unique_ptr<private_join_and_compute::ECGroup> group,
      std::unique_ptr<private_join_and_compute::ElGamalEncrypter> encrypter,
      std::unique_ptr<private_join_and_compute::ElGamalDecrypter> decrypter,
      std::string serialized_public_key);

  base::expected<GetProbabilisticRevealTokenResponse, absl::Status>
  IssueFromPoints(
      std::vector<private_join_and_compute::ECPoint> plaintext_points,
      base::Time expiration,
      base::Time next_epoch_start,
      int32_t num_tokens_with_signal,
      std::string epoch_id);

  base::expected<private_join_and_compute::ECPoint, absl::Status>
  GetPointByPadding(std::string plaintext) const;
  base::expected<private_join_and_compute::ECPoint, absl::Status>
  GetPointByHashing(std::string message) const;
  base::expected<ProbabilisticRevealToken, absl::Status> Encrypt(
      const private_join_and_compute::ECPoint& point) const;

  base::expected<private_join_and_compute::ECPoint, absl::Status> Decrypt(
      const ProbabilisticRevealToken& token) const;

  std::unique_ptr<private_join_and_compute::Context> context_;
  std::unique_ptr<const private_join_and_compute::ECGroup> group_;
  std::unique_ptr<const private_join_and_compute::ElGamalEncrypter> encrypter_;
  std::unique_ptr<const private_join_and_compute::ElGamalDecrypter> decrypter_;
  const std::string serialized_public_key_;
  std::vector<ProbabilisticRevealToken> tokens_;
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_PROBABILISTIC_REVEAL_TOKEN_TEST_ISSUER_H_
