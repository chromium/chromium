// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_ISSUER_CONFIG_H_
#define COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_ISSUER_CONFIG_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/values.h"
#include "components/private_verification_tokens/common/private_verification_tokens_public_key.h"

namespace private_verification_tokens {

inline constexpr char kIssuersKey[] = "issuers";
inline constexpr char kDomainKey[] = "domain";
inline constexpr char kVersionKey[] = "version";
inline constexpr char kPublicKeyKey[] = "public_key";
inline constexpr char kKeyIdKey[] = "key_id";
inline constexpr char kBatchSizeKey[] = "batch_size";
inline constexpr char kExpirationKey[] = "expiration";

// Struct for holding config for a single issuer.
struct IssuerConfig {
  IssuerConfig(int32_t batch_size,
               PrivateVerificationTokensPublicKey public_key);
  IssuerConfig(const IssuerConfig&);
  IssuerConfig& operator=(const IssuerConfig&);
  IssuerConfig(IssuerConfig&&);
  IssuerConfig& operator=(IssuerConfig&&);
  ~IssuerConfig();

  int32_t batch_size;
  PrivateVerificationTokensPublicKey public_key;
};

// Parses and holds the config for all issuers served by the component updater.
class PrivateVerificationTokensIssuerConfig {
 public:
  // Creates config from given dictionary. The config is served by the component
  // updater (trusted).  Component updater will have its own checks and tests to
  // serve configs in the right form. Chrome does verify the config as well. For
  // duplicated issuers in the dictionary, Chrome uses the first one in the
  // issuers list.
  static std::unique_ptr<PrivateVerificationTokensIssuerConfig> Create(
      base::DictValue config);

  PrivateVerificationTokensIssuerConfig(
      const PrivateVerificationTokensIssuerConfig&) = delete;
  PrivateVerificationTokensIssuerConfig(
      PrivateVerificationTokensIssuerConfig&&) = delete;
  PrivateVerificationTokensIssuerConfig& operator=(
      const PrivateVerificationTokensIssuerConfig&) = delete;
  PrivateVerificationTokensIssuerConfig& operator=(
      PrivateVerificationTokensIssuerConfig&&) = delete;

  ~PrivateVerificationTokensIssuerConfig();

  const std::map<std::string, IssuerConfig>& config() const;

 private:
  explicit PrivateVerificationTokensIssuerConfig(
      std::map<std::string, IssuerConfig> config);
  const std::map<std::string, IssuerConfig> config_;
};

}  // namespace private_verification_tokens

#endif  // COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_ISSUER_CONFIG_H_
