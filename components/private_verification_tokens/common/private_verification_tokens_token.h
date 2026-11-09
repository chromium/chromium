// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_TOKEN_H_
#define COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_TOKEN_H_

#include <cstdint>
#include <vector>

#include "base/time/time.h"
#include "url/origin.h"

namespace private_verification_tokens {

using SerializedToken = std::vector<uint8_t>;

// Type to store a single token.
class PrivateVerificationTokensToken {
 public:
  PrivateVerificationTokensToken(url::Origin issuer,
                                 SerializedToken token,
                                 uint32_t key_id,
                                 base::Time expiration,
                                 uint32_t version,
                                 base::Time creation_time = base::Time::Now(),
                                 bool is_proxied = false);
  PrivateVerificationTokensToken(const PrivateVerificationTokensToken&);
  PrivateVerificationTokensToken& operator=(
      const PrivateVerificationTokensToken&);
  PrivateVerificationTokensToken(PrivateVerificationTokensToken&&);
  PrivateVerificationTokensToken& operator=(PrivateVerificationTokensToken&&);

  ~PrivateVerificationTokensToken();

  // Returns the issuer Origin this token is for.
  const url::Origin& issuer() const;
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
  // Creation time of the token.
  base::Time creation_time() const;
  // Whether this token was obtained via the proxy server.
  bool is_proxied() const;

  bool operator==(const PrivateVerificationTokensToken&) const = default;

 private:
  url::Origin issuer_;
  SerializedToken token_;
  uint32_t key_id_;
  base::Time expiration_;
  uint32_t version_;
  base::Time creation_time_;
  bool is_proxied_ = false;
};

}  // namespace private_verification_tokens

#endif  // COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_TOKEN_H_
