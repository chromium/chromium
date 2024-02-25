// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_DOWNLOAD_KEYS_RESPONSE_HANDLER_H_
#define COMPONENTS_TRUSTED_VAULT_DOWNLOAD_KEYS_RESPONSE_HANDLER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "components/trusted_vault/trusted_vault_connection.h"
#include "components/trusted_vault/trusted_vault_request.h"

namespace trusted_vault {

enum class SecurityDomainId;
class SecureBoxKeyPair;

// Helper class to extract and validate trusted vault keys for a specific
// security domain from a GetSecurityDomainMember response.
class DownloadKeysResponseHandler {
 public:
  struct ProcessedResponse {
    explicit ProcessedResponse(TrustedVaultDownloadKeysStatus status);
    ProcessedResponse(TrustedVaultDownloadKeysStatus status,
                      std::vector<std::vector<uint8_t>> new_keys,
                      int last_key_version);
    ProcessedResponse(const ProcessedResponse& other);
    ProcessedResponse& operator=(const ProcessedResponse& other);
    ~ProcessedResponse();

    TrustedVaultDownloadKeysStatus status;

    // Contains all keys returned by the server.
    std::vector<std::vector<uint8_t>> downloaded_keys;
    int last_key_version;
  };

  // Returns error cases that can be directly determined from the HTTP status,
  // or nullopt if successful. Exposed publicly for membership-verification
  // purposes.
  static std::optional<TrustedVaultDownloadKeysStatus> GetErrorFromHttpStatus(
      TrustedVaultRequest::HttpStatus http_status);

  // |device_key_pair| must not be null. It will be verified that the new keys
  // are result of rotating |last_trusted_vault_key_and_version|.
  DownloadKeysResponseHandler(
      SecurityDomainId security_domain,
      const TrustedVaultKeyAndVersion& last_trusted_vault_key_and_version,
      std::unique_ptr<SecureBoxKeyPair> device_key_pair);
  DownloadKeysResponseHandler(const DownloadKeysResponseHandler& other) =
      delete;
  DownloadKeysResponseHandler& operator=(
      const DownloadKeysResponseHandler& other) = delete;
  ~DownloadKeysResponseHandler();

  ProcessedResponse ProcessResponse(TrustedVaultRequest::HttpStatus http_status,
                                    const std::string& response_body) const;

 private:
  const SecurityDomainId security_domain_;
  const TrustedVaultKeyAndVersion last_trusted_vault_key_and_version_;
  const std::unique_ptr<SecureBoxKeyPair> device_key_pair_;
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_DOWNLOAD_KEYS_RESPONSE_HANDLER_H_
