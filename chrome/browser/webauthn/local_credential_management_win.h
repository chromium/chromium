// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_LOCAL_CREDENTIAL_MANAGEMENT_WIN_H_
#define CHROME_BROWSER_WEBAUTHN_LOCAL_CREDENTIAL_MANAGEMENT_WIN_H_

#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/webauthn/local_credential_management.h"
#include "device/fido/win/authenticator.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace device {
class WinWebAuthnApi;
}

// LocalCredentialManagementWin is the Windows implementation
// LocalCredentialManagement.
class LocalCredentialManagementWin : public LocalCredentialManagement {
 public:
  explicit LocalCredentialManagementWin(device::WinWebAuthnApi* api,
                                        Profile* profile);

  // RegisterProfilePrefs registers preference values that are used for caching
  // whether local credentials exist.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // LocalCredentialManagement:
  void HasCredentials(base::OnceCallback<void(bool)> callback) override;
  void Enumerate(
      base::OnceCallback<void(
          std::optional<std::vector<device::DiscoverableCredentialMetadata>>)>
          callback) override;
  void Delete(base::span<const uint8_t> credential_id,
              base::OnceCallback<void(bool)> callback) override;
  void Edit(base::span<uint8_t> credential_id,
            std::string new_username,
            base::OnceCallback<void(bool)> callback) override;

 private:
  const raw_ptr<device::WinWebAuthnApi> api_;
  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_WEBAUTHN_LOCAL_CREDENTIAL_MANAGEMENT_WIN_H_
