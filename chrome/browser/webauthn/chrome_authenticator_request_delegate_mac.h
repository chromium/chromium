// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_CHROME_AUTHENTICATOR_REQUEST_DELEGATE_MAC_H_
#define CHROME_BROWSER_WEBAUTHN_CHROME_AUTHENTICATOR_REQUEST_DELEGATE_MAC_H_

#include <memory>

// Returns true if iCloud Keychain is configured for passkeys on this device.
// When available, this queries Apple's
// `isDeviceConfiguredForPasskeys` API. Otherwise, it falls back to checking
// whether iCloud Drive is available as an approximation.
bool IsICloudKeychainConfiguredForPasskeys();

struct ScopedICloudKeychainOverride {
  virtual ~ScopedICloudKeychainOverride() = 0;
};

// Override `IsICloudKeychainConfiguredForPasskeys` for testing purposes.
std::unique_ptr<ScopedICloudKeychainOverride>
OverrideICloudKeychainConfiguredForPasskeys(bool configured);

#endif  // CHROME_BROWSER_WEBAUTHN_CHROME_AUTHENTICATOR_REQUEST_DELEGATE_MAC_H_
