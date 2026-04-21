// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_PUBLIC_KEY_H_
#define COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_PUBLIC_KEY_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/time/time.h"

namespace private_verification_tokens {

class PrivateVerificationTokensPublicKey {
 public:
  PrivateVerificationTokensPublicKey(std::string etld_plus_one,
                                     std::vector<uint8_t> public_key,
                                     uint32_t key_id,
                                     base::Time expiration,
                                     uint32_t version);
  PrivateVerificationTokensPublicKey(const PrivateVerificationTokensPublicKey&);
  PrivateVerificationTokensPublicKey& operator=(
      const PrivateVerificationTokensPublicKey&);
  PrivateVerificationTokensPublicKey(PrivateVerificationTokensPublicKey&&);
  PrivateVerificationTokensPublicKey& operator=(
      PrivateVerificationTokensPublicKey&&);

  ~PrivateVerificationTokensPublicKey();

  const std::string& etld_plus_one() const;
  const std::vector<uint8_t>& public_key() const;
  uint32_t key_id() const;
  base::Time expiration() const;
  uint32_t version() const;

 private:
  std::string etld_plus_one_;
  std::vector<uint8_t> public_key_;
  uint32_t key_id_;
  base::Time expiration_;
  uint32_t version_;
};

}  // namespace private_verification_tokens

#endif  // COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_PUBLIC_KEY_H_
