// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TRUSTED_VAULT_DOWNLOAD_KEYS_RESPONSE_HANDLER_H_
#define COMPONENTS_SYNC_TRUSTED_VAULT_DOWNLOAD_KEYS_RESPONSE_HANDLER_H_

#include <memory>
#include <string>
#include <vector>

#include "components/sync/trusted_vault/trusted_vault_connection.h"
#include "components/sync/trusted_vault/trusted_vault_request.h"

namespace syncer {

class SecureBoxKeyPair;

// Helper class to extract and validate trusted vault keys from
// GetSecurityDomainMember response.
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

    // Contains new keys (e.g. keys are stored by the server, excluding last
    // known key and keys that predate it).  Excludes first key if it's a
    // constant key.
    // TODO(crbug.com/1267391): return all keys obtained from the server and
    // update StandaloneTrustedVaultBackend to store them.
    std::vector<std::vector<uint8_t>> new_keys;
    int last_key_version;
  };

  // |device_key_pair| must not be null. It will be verified that the new keys
  // are result of rotating |last_trusted_vault_key_and_version|.
  DownloadKeysResponseHandler(
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
  const TrustedVaultKeyAndVersion last_trusted_vault_key_and_version_;
  const std::unique_ptr<SecureBoxKeyPair> device_key_pair_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TRUSTED_VAULT_DOWNLOAD_KEYS_RESPONSE_HANDLER_H_
