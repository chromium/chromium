// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_FAKE_RECOVERY_KEY_STORE_H_
#define CHROME_BROWSER_WEBAUTHN_FAKE_RECOVERY_KEY_STORE_H_

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "net/http/http_status_code.h"

namespace network {
struct ResourceRequest;
}

namespace trusted_vault_pb {
class Vault;
}

// A FakeRecoveryKeyStore handles network requests that would otherwise go to
// the recovery key store (or the gstatic.com resources that it uses). It
// records uploaded vaults like the real service.
class FakeRecoveryKeyStore {
 public:
  // If present, values of this type contain an HTTP status code (e.g. 200) and
  // the body of the response.
  using MaybeResponse =
      std::optional<std::pair<net::HttpStatusCode, std::string>>;

  static std::unique_ptr<FakeRecoveryKeyStore> New();

  virtual ~FakeRecoveryKeyStore() = 0;

  // Get a callback that processes network requests and, if they are for the
  // security domain service, returns a response.
  virtual base::RepeatingCallback<
      MaybeResponse(const network::ResourceRequest&)>
  GetCallback() = 0;

  // Return the set of uploaded vaults.
  virtual base::span<const trusted_vault_pb::Vault> vaults() const = 0;

  // Override the URL for the specified files.
  virtual void set_cert_xml_url(std::string url) = 0;
  virtual void set_sig_xml_url(std::string url) = 0;

  // Cause the specified file to return an HTTP error.
  virtual void break_cert_xml_file() = 0;
  virtual void break_sig_xml_file() = 0;

  // These methods update recovery key store serial number, then regenerate keys
  // and the cert files.
  virtual void DowngradeCohort() = 0;
  virtual void UpgradeCohort() = 0;
  virtual int recovery_key_store_serial_number() = 0;

  // Returns the cohort private scalar as 32 bytes in big-endian order.
  // `public_key_bytes` must correspond to an endpoint key that was current at
  // some point during the lifetime of the fake recovery key store.
  virtual std::array<uint8_t, 32> EndpointPrivateKeyFor(
      base::span<const uint8_t> public_key_bytes) const = 0;

  // Returns the current cohort public key bytes.
  virtual std::vector<uint8_t> CurrentEndpointPublicKeyBytes() const = 0;
};

#endif  // CHROME_BROWSER_WEBAUTHN_FAKE_RECOVERY_KEY_STORE_H_
