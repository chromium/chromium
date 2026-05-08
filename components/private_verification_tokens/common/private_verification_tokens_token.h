// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_TOKEN_H_
#define COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_TOKEN_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/time/time.h"

namespace private_verification_tokens {

using SerializedToken = std::vector<uint8_t>;

// Type to store a single token.
class PrivateVerificationTokensToken {
 public:
  PrivateVerificationTokensToken(std::string etld_plus_one,
                                 SerializedToken token,
                                 uint32_t key_id,
                                 base::Time expiration,
                                 uint32_t version);
  PrivateVerificationTokensToken(const PrivateVerificationTokensToken&);
  PrivateVerificationTokensToken& operator=(
      const PrivateVerificationTokensToken&);
  PrivateVerificationTokensToken(PrivateVerificationTokensToken&&);
  PrivateVerificationTokensToken& operator=(PrivateVerificationTokensToken&&);

  ~PrivateVerificationTokensToken();

  // Returns the eTLD+1 this token is for.
  const std::string& etld_plus_one() const;
  // Returns the serialized token. This is a single serialized Token
  // from
  // https://www.ietf.org/archive/id/draft-yun-cfrg-athm-00.html#section-5.4.1
  // Token in TLS Presentation language is as follows.
  // struct {
  //   uint8 t_enc[Ns];
  //   uint8 P_enc[Ne];
  //   uint8 Q_enc[Ne];
  // } Token;
  const SerializedToken& token() const;
  // ID of the associated public key used. This lets the issuer server
  // determine the right key to use.
  uint32_t key_id() const;
  // Expiration of the token. The token should not be used past this.
  base::Time expiration() const;
  // PVT version. This is for backward compatibility when PVT details change.
  // The version determines the ATHM crypto parameters, serialization of group
  // elements (compressed vs not). Version is used when retrieving tokens from
  // the database as well.
  uint32_t version() const;

 private:
  std::string etld_plus_one_;
  SerializedToken token_;
  uint32_t key_id_;
  base::Time expiration_;
  uint32_t version_;
};

}  // namespace private_verification_tokens

#endif  // COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_TOKEN_H_
