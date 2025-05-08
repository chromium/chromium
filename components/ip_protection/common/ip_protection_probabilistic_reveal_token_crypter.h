// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_PROBABILISTIC_REVEAL_TOKEN_CRYPTER_H_
#define COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_PROBABILISTIC_REVEAL_TOKEN_CRYPTER_H_

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/types/expected.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "third_party/abseil-cpp/absl/status/status.h"
#include "third_party/private-join-and-compute/src/crypto/context.h"
#include "third_party/private-join-and-compute/src/crypto/ec_group.h"
#include "third_party/private-join-and-compute/src/crypto/elgamal.h"

namespace ip_protection {

// IpProtectionProbabilisticRevealTokenCrypter stores crypto context and
// ciphertexts for probabilistic reveal tokens. Provides a method to randomize a
// specified token. This class sits between probabilistic reveal token manager
// and private-join-and-compute third party library. private-join-and-compute
// dependency is private, i.e., rest of the ip protection code does not depend
// on private-join-and-compute and all necessary functionality is provided by
// this class.
class IpProtectionProbabilisticRevealTokenCrypter {
 public:
  // Returns a unique pointer to crypter if successful.
  static base::expected<
      std::unique_ptr<IpProtectionProbabilisticRevealTokenCrypter>,
      absl::Status>
  Create(const std::string& serialized_public_key,
         const std::vector<ProbabilisticRevealToken>& tokens);
  ~IpProtectionProbabilisticRevealTokenCrypter();
  bool IsTokenAvailable() const;
  // Clears ciphertexts stored. This method can be used when tokens are expired
  // but no new tokens are available.
  void ClearTokens();
  // Returns number of ciphertexts stored.
  size_t NumTokens() const;
  // Updates internals for a new batch of public key and tokens if
  // successful. State is not changed if not successful.
  absl::Status SetNewPublicKeyAndTokens(
      const std::string& serialized_public_key,
      const std::vector<ProbabilisticRevealToken>& tokens);
  // Randomizes ciphertext corresponding to token i, creates and returns an
  // ProbabilisticRevealToken by serializing it, if successful. This method
  // fails if there is no ciphertext for index `i` or `encrypter_.ReRandomize()`
  // fails.
  base::expected<ProbabilisticRevealToken, absl::Status> Randomize(
      size_t i) const;

 private:
  // Private since it assumes all arguments are valid. Static `Create()`
  // calls this after validating its arguments and creating arguments
  // for this constructor.
  IpProtectionProbabilisticRevealTokenCrypter(
      std::unique_ptr<::private_join_and_compute::Context> context,
      std::unique_ptr<::private_join_and_compute::ECGroup> group,
      std::unique_ptr<::private_join_and_compute::ElGamalEncrypter> encrypter,
      std::vector<::private_join_and_compute::elgamal::Ciphertext> ciphertext);

  // Context is not thread safe, should be accessed only from the right
  // sequence. This means `context_`, `group_` and `encrypter_` should be
  // guarded.
  std::unique_ptr<::private_join_and_compute::Context> context_
      GUARDED_BY_CONTEXT(sequence_checker_);
  // `context_` should be defined before `group_` and deleted after.
  std::unique_ptr<::private_join_and_compute::ECGroup> group_
      GUARDED_BY_CONTEXT(sequence_checker_);
  // `group_` should be defined before `encrypter_` and deleted after.
  std::unique_ptr<::private_join_and_compute::ElGamalEncrypter> encrypter_
      GUARDED_BY_CONTEXT(sequence_checker_);
  // ciphertext_ is obtained by de-serializing ProbabilisticRevealTokens to
  // ECPoints. `group_` should be defined before `ciphertext_` and deleted
  // after.
  std::vector<::private_join_and_compute::elgamal::Ciphertext> ciphertext_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_PROBABILISTIC_REVEAL_TOKEN_CRYPTER_H_
