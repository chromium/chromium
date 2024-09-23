// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_FAKE_RECOVERY_KEY_STORE_H_
#define CHROME_BROWSER_WEBAUTHN_FAKE_RECOVERY_KEY_STORE_H_

#include <memory>
#include <optional>
#include <string>

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

  // Cause the specified file to return an HTTP error.
  virtual void break_cert_xml_file() = 0;
  virtual void break_sig_xml_file() = 0;

  // Returns the cohort private scalar as 32 bytes in big-endian order.
  virtual std::array<uint8_t, 32> endpoint_private_key_bytes() const = 0;
};

#endif  // CHROME_BROWSER_WEBAUTHN_FAKE_RECOVERY_KEY_STORE_H_
