// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSKEY_CREDENTIAL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSKEY_CREDENTIAL_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "base/containers/span.h"

namespace sync_pb {
class WebauthnCredentialSpecifics;
}

namespace password_manager {

// Represents a Web Authentication passkey credential to be displayed in
// autofill and password manager selection contexts.
class PasskeyCredential {
 public:
  enum class Source {
    kAndroidPhone,
    kTouchId,
    kWindowsHello,
    kOther,
  };

  static std::vector<PasskeyCredential> FromCredentialSpecifics(
      base::span<const sync_pb::WebauthnCredentialSpecifics> passkeys);

  PasskeyCredential(Source source,
                    std::string rp_id,
                    std::vector<uint8_t> credential_id,
                    std::vector<uint8_t> user_id,
                    std::string username = "",
                    std::string display_name = "");
  ~PasskeyCredential();

  PasskeyCredential(const PasskeyCredential&);
  PasskeyCredential& operator=(const PasskeyCredential&);

  PasskeyCredential(PasskeyCredential&&);
  PasskeyCredential& operator=(PasskeyCredential&&);

  // Returns the l10n ID for the name of the authenticator this credential
  // belongs to.
  int GetAuthenticatorLabel() const;

  Source source() const { return source_; }
  const std::string& rp_id() const { return rp_id_; }
  const std::vector<uint8_t>& credential_id() const { return credential_id_; }
  const std::vector<uint8_t>& user_id() const { return user_id_; }
  const std::string& username() const { return username_; }
  const std::string& display_name() const { return display_name_; }

 private:
  friend bool operator==(const PasskeyCredential& lhs,
                         const PasskeyCredential& rhs);

  // Authenticator type this passkey belongs to.
  Source source_;

  // The relying party identifier.
  // https://w3c.github.io/webauthn/#relying-party-identifier
  std::string rp_id_;

  // The credential identifier.
  // https://w3c.github.io/webauthn/#credential-id
  std::vector<uint8_t> credential_id_;

  // The user's identifier handle.
  // https://w3c.github.io/webauthn/#user-handle
  std::vector<uint8_t> user_id_;

  // The user's name.
  // https://w3c.github.io/webauthn/#dom-publickeycredentialentity-name
  std::string username_;

  // The user's display name.
  // https://w3c.github.io/webauthn/#dom-publickeycredentialuserentity-displayname
  std::string display_name_;
};

bool operator==(const PasskeyCredential& lhs, const PasskeyCredential& rhs);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSKEY_CREDENTIAL_H_
