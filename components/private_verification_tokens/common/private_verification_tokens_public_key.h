// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_PUBLIC_KEY_H_
#define COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_PUBLIC_KEY_H_

#include <cstdint>
#include <vector>

#include "base/time/time.h"
#include "url/origin.h"

namespace private_verification_tokens {

// Holds a PVT key.
//
// Serialized public_key is the serialization of the PublicKey struct given in
// the following TLS presentation form.
//
// struct {
//   uint8 Z_enc[Ne];
//   uint8 C_x_enc[Ne];
//   uint8 C_y_enc[Ne];
// } PublicKey;
//
// Serialized public_key_proof is the serialization of the PublicKeyProof struct
// given in the following TLS presentation form.
//
// struct {
//   uint8 e_enc[Ns];
//   uint8 a_z_enc[Ns];
// } PublicKeyProof;
//
// key_id <- SHA-256(Serialize(PublicKey))
// truncated_key_id <- least significant byte of key_id
//
class PrivateVerificationTokensPublicKey {
 public:
  PrivateVerificationTokensPublicKey(url::Origin issuer,
                                     std::vector<uint8_t> public_key,
                                     std::vector<uint8_t> public_key_proof,
                                     base::Time expiration,
                                     uint32_t version);
  PrivateVerificationTokensPublicKey(const PrivateVerificationTokensPublicKey&);
  PrivateVerificationTokensPublicKey& operator=(
      const PrivateVerificationTokensPublicKey&);
  PrivateVerificationTokensPublicKey(PrivateVerificationTokensPublicKey&&);
  PrivateVerificationTokensPublicKey& operator=(
      PrivateVerificationTokensPublicKey&&);

  ~PrivateVerificationTokensPublicKey();

  const url::Origin& issuer() const;
  const std::vector<uint8_t>& public_key() const;
  const std::vector<uint8_t>& public_key_proof() const;
  uint8_t key_id() const;
  base::Time expiration() const;
  uint32_t version() const;

  bool operator==(const PrivateVerificationTokensPublicKey&) const = default;

 private:
  url::Origin issuer_;
  // Serialized public key.
  std::vector<uint8_t> public_key_;
  // Serialized public key proof.
  std::vector<uint8_t> public_key_proof_;
  // Stores truncated key id.
  uint8_t key_id_;
  base::Time expiration_;
  uint32_t version_;
};

}  // namespace private_verification_tokens

#endif  // COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_PUBLIC_KEY_H_
