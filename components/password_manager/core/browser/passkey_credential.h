// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSKEY_CREDENTIAL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSKEY_CREDENTIAL_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "build/build_config.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace password_manager {

// Represents a Web Authentication passkey credential to be displayed in
// autofill and password manager selection contexts.
class PasskeyCredential {
 public:
  enum class Source {
    kAndroidPhone,
    kTouchId,
    kWindowsHello,
    kICloudKeychain,
    kGooglePasswordManager,
    kOther,
  };

  using RpId = base::StrongAlias<class RpIdTag, std::string>;
  using CredentialId =
      base::StrongAlias<class CredentialIdTag, std::vector<uint8_t>>;
  using UserId = base::StrongAlias<class UserIdTag, std::vector<uint8_t>>;
  using Username = base::StrongAlias<class UsernameTag, std::string>;
  using DisplayName = base::StrongAlias<class DisplayNameTag, std::string>;

#if !BUILDFLAG(IS_ANDROID)
  static std::vector<PasskeyCredential> FromCredentialSpecifics(
      base::span<const sync_pb::WebauthnCredentialSpecifics> passkeys);
#endif  // !BUILDFLAG(IS_ANDROID)

  PasskeyCredential(Source source,
                    RpId rp_id,
                    CredentialId credential_id,
                    UserId user_id,
                    Username username = Username(""),
                    DisplayName display_name = DisplayName(""),
                    // Must be provided for kAndroidPhone credentials.
                    std::optional<base::Time> creation_time = std::nullopt);
  ~PasskeyCredential();

  PasskeyCredential(const PasskeyCredential&);
  PasskeyCredential& operator=(const PasskeyCredential&);

  PasskeyCredential(PasskeyCredential&&);
  PasskeyCredential& operator=(PasskeyCredential&&);

  // Returns the user-friendly label for the authenticator this credential
  // belongs to.
  std::u16string GetAuthenticatorLabel() const;

  // Sets an authenticator label for this passkey. If no label is set, a generic
  // device name will be returned by GetAuthenticatorLabel().
  void set_authenticator_label(const std::u16string& authenticator_label) {
    authenticator_label_ = authenticator_label;
  }

  Source source() const { return source_; }
  const std::string& rp_id() const { return rp_id_; }
  const std::vector<uint8_t>& credential_id() const { return credential_id_; }
  const std::vector<uint8_t>& user_id() const { return user_id_; }
  const std::string& username() const { return username_; }
  const std::string& display_name() const { return display_name_; }
  const std::optional<base::Time>& creation_time() const {
    return creation_time_;
  }

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

  // An optional label for the authenticator. If this is not set, a generic
  // device name will be returned by GetAuthenticatorLabel().
  std::optional<std::u16string> authenticator_label_;

  // The time when the credential was created. Used for display in management
  // UIs. This value is not available for passkeys from some sources.
  std::optional<base::Time> creation_time_;
};

bool operator==(const PasskeyCredential& lhs, const PasskeyCredential& rhs);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSKEY_CREDENTIAL_H_
