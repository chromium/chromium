// Copyright 2020 The Chromium Authors. All rights reserved.
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
// ListSecurityDomainsResponse.
class DownloadKeysResponseHandler {
 public:
  struct ProcessedResponse {
    explicit ProcessedResponse(TrustedVaultRequestStatus status);
    ProcessedResponse(TrustedVaultRequestStatus status,
                      std::vector<std::vector<uint8_t>> keys,
                      int last_key_version);
    ProcessedResponse(const ProcessedResponse& other);
    ProcessedResponse& operator=(const ProcessedResponse& other);
    ~ProcessedResponse();

    // kSuccess is reported if extraction was successful and there are new
    // trusted vault keys.
    // kLocalDataObsolete is reported if it's impossible to extract keys due to
    // data corruption or absence of SecurityDomain/Member or if there is no new
    // keys.
    // kOtherError is reported in case of http/network errors or if the response
    // isn't valid serialized ListSecurityDomainsResponse proto.
    TrustedVaultRequestStatus status;

    // Contains new keys and potentially previously known trusted vault key if
    // it wasn't removed server-side. Doesn't contain keys that predate
    // last known trusted vault key, because it's impossible to validate them
    // and the client should be aware of them already.
    std::vector<std::vector<uint8_t>> keys;
    int last_key_version;
  };

  // |device_key_pair| must not be null. This class doesn't make an assumption
  // of |last_trusted_vault_key_and_version.key| being non-empty or
  // |last_trusted_vault_key_version_and_version.version| being non-negative.
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
