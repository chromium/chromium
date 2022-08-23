// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_LOCAL_CREDENTIAL_MANAGEMENT_MAC_H_
#define CHROME_BROWSER_WEBAUTHN_LOCAL_CREDENTIAL_MANAGEMENT_MAC_H_

#include <vector>

#include "chrome/browser/webauthn/local_credential_management.h"

class Profile;

// LocalCredentialManagementMac is the MacOS implementation of
// LocalCredentialManagement.
class LocalCredentialManagementMac : public LocalCredentialManagement {
 public:
  LocalCredentialManagementMac();

  // LocalCredentialManagement:
  void HasCredentials(Profile* profile,
                      base::OnceCallback<void(bool)> callback) override;
  void Enumerate(
      Profile* profile,
      base::OnceCallback<void(
          absl::optional<std::vector<device::DiscoverableCredentialMetadata>>)>
          callback) override;
  void Delete(Profile* profile,
              base::span<const uint8_t> credential_id,
              base::OnceCallback<void(bool)> callback) override;
};

#endif  // CHROME_BROWSER_WEBAUTHN_LOCAL_CREDENTIAL_MANAGEMENT_MAC_H_