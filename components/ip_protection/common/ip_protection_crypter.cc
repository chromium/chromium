// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_crypter.h"

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

absl::StatusOr<PublicKey> DeserializePublicKey(
    const std::string& serialized_public_key) {
  auto context = std::make_unique<Context>();
  ASSIGN_OR_RETURN(ECGroup group,
                   ECGroup::Create(NID_secp224r1, context.get()));
  ASSIGN_OR_RETURN(ECPoint y, group.CreateECPoint(serialized_public_key));
  ASSIGN_OR_RETURN(ECPoint g, group.GetFixedGenerator());
  return PublicKey{std::move(g), std::move(y)};
}

// Serialize |key.y| in compressed form and return to it.
absl::StatusOr<std::string> SerializePublicKey(const PublicKey& key) {
  return key.y.ToBytesCompressed();
}

absl::StatusOr<Ciphertext> Randomize(const std::string& serialized_public_key_y,
                                     const Ciphertext& ciphertext) {
  auto context = std::make_unique<Context>();
  ASSIGN_OR_RETURN(ECGroup group,
                   ECGroup::Create(NID_secp224r1, context.get()));
  ASSIGN_OR_RETURN(PublicKey public_key,
                   DeserializePublicKey(serialized_public_key_y));
  auto encrypter = ElGamalEncrypter(
      &group, std::make_unique<PublicKey>(std::move(public_key)));
  return encrypter.ReRandomize(ciphertext);
}

}  // namespace ip_protection
