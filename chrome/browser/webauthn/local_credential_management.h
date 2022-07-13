// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_LOCAL_CREDENTIAL_MANAGEMENT_H_
#define CHROME_BROWSER_WEBAUTHN_LOCAL_CREDENTIAL_MANAGEMENT_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/containers/span.h"
#include "build/build_config.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// This class is only supported on Windows so far.
#if BUILDFLAG(IS_WIN)

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace device {
class WinWebAuthnApi;
}

// LocalCredentialManagement provides functions for managing local WebAuthn
// credentials, i.e. those kept in a platform authenticator like Windows Hello
// or Chrome's TouchId authenticator. This is in contrast to the classes in
// //device/fido/credential_management.h that aid in managing credentials on
// security keys.
class LocalCredentialManagement {
 public:
  LocalCredentialManagement(device::WinWebAuthnApi* api);

  // HasCredentials resolves whether a non-zero number of credentials exists on
  // the platform authenticator. This may be significantly more efficient than
  // calling `Enumerate`. The callback will never be invoked before the
  // function returns.
  void HasCredentials(Profile* profile,
                      base::OnceCallback<void(bool)> callback);

  // Enumerate returns the metadata for all credentials on the platform. The
  // callback will never be invoked before the function returns.
  //
  // If enumeration isn't supported on this version of Windows the callback will
  // be run with `absl::nullopt`.
  void Enumerate(
      Profile* profile,
      base::OnceCallback<void(
          absl::optional<std::vector<device::DiscoverableCredentialMetadata>>)>
          callback);

  // Delete removes a credentail from the platform authenticator. The
  // callback will never be invoked before the function returns. It is run with
  // the value `true` if the deletion was successful.
  void Delete(Profile* profile,
              base::span<const uint8_t> credential_id,
              base::OnceCallback<void(bool)> callback);

  // RegisterProfilePrefs registers preference values that are used for caching
  // whether local credentials exist.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  device::WinWebAuthnApi* const api_;
};

#endif

#endif  // CHROME_BROWSER_WEBAUTHN_LOCAL_CREDENTIAL_MANAGEMENT_H_
