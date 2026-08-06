// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_ISSUER_CONFIG_H_
#define COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_ISSUER_CONFIG_H_

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "components/private_verification_tokens/common/private_verification_tokens_public_key.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace private_verification_tokens {

inline constexpr char kIssuersKey[] = "issuers";
inline constexpr char kIssuerRequestUrlKey[] = "issuerRequestUrl";
inline constexpr char kVersionKey[] = "version";
inline constexpr char kPublicKeyKey[] = "publicKey";
inline constexpr char kPublicKeyProofKey[] = "publicKeyProof";
inline constexpr char kBatchSizeKey[] = "batchSize";
inline constexpr char kExpirationKey[] = "expiration";
inline constexpr char kRedeemersKey[] = "redeemers";
inline constexpr char kDeploymentIdKey[] = "deploymentId";
inline constexpr char kConfigVersionKey[] = "1";

// Struct for holding config for a single issuer.
struct IssuerConfig {
  IssuerConfig(GURL issuer_request_url,
               int32_t batch_size,
               PrivateVerificationTokensPublicKey public_key,
               std::vector<url::Origin> redeemers,
               std::string deployment_id);
  IssuerConfig(const IssuerConfig&);
  IssuerConfig& operator=(const IssuerConfig&);
  IssuerConfig(IssuerConfig&&);
  IssuerConfig& operator=(IssuerConfig&&);
  ~IssuerConfig();

  GURL issuer_request_url;
  int32_t batch_size;
  PrivateVerificationTokensPublicKey public_key;
  std::vector<url::Origin> redeemers;
  std::string deployment_id;
};

// Parses and holds the config for all issuers served by the component updater.
//
// Inherits from `base::RefCountedThreadSafe` to enable safe, shared ownership
// of the immutable configuration across multiple profile services and across
// thread boundaries (e.g., loading on a background sequence and passing to the
// UI thread).
class PrivateVerificationTokensIssuerConfig
    : public base::RefCountedThreadSafe<PrivateVerificationTokensIssuerConfig> {
 public:
  // Creates config from given dictionary. The config is served by the component
  // updater (trusted).  Component updater will have its own checks and tests to
  // serve configs in the right form. Chrome does verify the config as well. For
  // duplicated issuers in the dictionary, Chrome uses the first one in the
  // issuers list.
  static scoped_refptr<PrivateVerificationTokensIssuerConfig> Create(
      base::DictValue config);

  // Loads and parses the config from the config file at `path`. The config file
  // is obtained through component updater. Returns nullptr if file reading or
  // parsing fails which should not happen in theory since the component
  // updater will have its own check on the server side to verify the file
  // before serving.
  //
  // NOTE: This function performs blocking I/O and must be called on a
  // sequence that allows blocking (e.g., using base::ThreadPool with
  // base::MayBlock()).
  static scoped_refptr<PrivateVerificationTokensIssuerConfig> LoadFromFile(
      const base::FilePath& path);

  // Creates a new config containing all entries from `base_config` (if
  // non-null) plus the single custom issuer entry parsed from
  // `custom_issuer_dict`.
  static scoped_refptr<const PrivateVerificationTokensIssuerConfig>
  CreateWithCustomIssuer(
      scoped_refptr<const PrivateVerificationTokensIssuerConfig> base_config,
      base::DictValue custom_issuer_dict);

  PrivateVerificationTokensIssuerConfig(
      const PrivateVerificationTokensIssuerConfig&) = delete;
  PrivateVerificationTokensIssuerConfig(
      PrivateVerificationTokensIssuerConfig&&) = delete;
  PrivateVerificationTokensIssuerConfig& operator=(
      const PrivateVerificationTokensIssuerConfig&) = delete;
  PrivateVerificationTokensIssuerConfig& operator=(
      PrivateVerificationTokensIssuerConfig&&) = delete;

  const std::map<url::Origin, IssuerConfig>& config() const;

 private:
  friend class base::RefCountedThreadSafe<
      PrivateVerificationTokensIssuerConfig>;
  explicit PrivateVerificationTokensIssuerConfig(
      std::map<url::Origin, IssuerConfig> config);
  ~PrivateVerificationTokensIssuerConfig();

  const std::map<url::Origin, IssuerConfig> config_;
};

}  // namespace private_verification_tokens

#endif  // COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_ISSUER_CONFIG_H_
