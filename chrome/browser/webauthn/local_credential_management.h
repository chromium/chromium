// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_LOCAL_CREDENTIAL_MANAGEMENT_H_
#define CHROME_BROWSER_WEBAUTHN_LOCAL_CREDENTIAL_MANAGEMENT_H_

#include <vector>

#include "base/functional/callback.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "third_party/icu/source/i18n/unicode/coll.h"

class Profile;

// CredentialComparator compares two credentials based on their RP ID's eTLD +
// 1, then on the label-reversed RP ID, then on user.name, and finally on
// credential ID if the previous values are equal.
class CredentialComparator {
 public:
  CredentialComparator();

  ~CredentialComparator();

  bool operator()(const device::DiscoverableCredentialMetadata& a,
                  const device::DiscoverableCredentialMetadata& b);

 private:
  static std::u16string ETLDPlus1(const std::string& rp_id);

  static std::u16string LabelReverse(const std::string& rp_id);

  std::unique_ptr<icu::Collator> collator_;
};

// LocalCredentialManagement provides functions for managing local WebAuthn
// credentials, i.e. those kept in a platform authenticator like Windows Hello
// or Chrome's TouchId authenticator. This is in contrast to the classes in
// //device/fido/credential_management.h that aid in managing credentials on
// security keys.
class LocalCredentialManagement {
 public:
  virtual ~LocalCredentialManagement() = default;

  // Returns an instance of LocalCredentialManagement depending on the
  // underlying operating system. It is incorrect to call this on an unsupported
  // OS.
  static std::unique_ptr<LocalCredentialManagement> Create(Profile* profile);

  // HasCredentials resolves whether a non-zero number of credentials exists on
  // the platform authenticator. This may be significantly more efficient than
  // calling `Enumerate`. The callback will never be invoked before the
  // function returns.
  virtual void HasCredentials(base::OnceCallback<void(bool)> callback) = 0;

  // Enumerate returns the metadata for all credentials on the platform. The
  // callback will never be invoked before the function returns.
  //
  // If enumeration isn't supported on this version of Windows the callback will
  // be run with `std::nullopt`.
  virtual void Enumerate(
      base::OnceCallback<void(
          std::optional<std::vector<device::DiscoverableCredentialMetadata>>)>
          callback) = 0;

  // Delete removes a credentail from the platform authenticator. The
  // callback will never be invoked before the function returns. It is run with
  // the value `true` if the deletion was successful.
  virtual void Delete(base::span<const uint8_t> credential_id,
                      base::OnceCallback<void(bool)> callback) = 0;

  // Edit credential metadata's username field. The callback returns false if
  // the credential was not updated to |new_username| in the mac keychain. The
  // callback will never be invoked before the function returns.
  virtual void Edit(base::span<uint8_t> credential_id,
                    std::string new_username,
                    base::OnceCallback<void(bool)> callback) = 0;
};

#endif  // CHROME_BROWSER_WEBAUTHN_LOCAL_CREDENTIAL_MANAGEMENT_H_
