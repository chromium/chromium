// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_CHROME_AUTHENTICATOR_REQUEST_DELEGATE_MAC_H_
#define CHROME_BROWSER_WEBAUTHN_CHROME_AUTHENTICATOR_REQUEST_DELEGATE_MAC_H_

#include <memory>

// IsICloudDriveEnabled returns true if iCloud Drive is available. This is used
// as an approximation to "has iCloud Keychain enabled", which is what we would
// like to know but cannot easily learn.
bool IsICloudDriveEnabled();

struct ScopedICloudDriveOverride {
  virtual ~ScopedICloudDriveOverride() = 0;
};

// Override `IsICloudDriveEnabled` for testing purposes.
std::unique_ptr<ScopedICloudDriveOverride> OverrideICloudDriveEnabled(
    bool enabled);

#endif  // CHROME_BROWSER_WEBAUTHN_CHROME_AUTHENTICATOR_REQUEST_DELEGATE_MAC_H_
