// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/chrome_authenticator_request_delegate_mac.h"

#import <Foundation/Foundation.h>

#include <memory>

#include "base/check.h"
#include "base/feature_list.h"
#include "device/fido/mac/icloud_keychain.h"
#include "device/fido/public/features.h"

static bool g_override = false;
static bool g_override_value = false;

bool IsICloudKeychainConfiguredForPasskeys() {
  if (g_override) {
    return g_override_value;
  }
  if (base::FeatureList::IsEnabled(
          device::kWebAuthnICloudKeychainUseDeviceConfiguredForPasskeys)) {
    if (@available(macOS 26.2, *)) {
      return device::fido::icloud_keychain::IsConfiguredForPasskeys();
    }
  }
  return [NSFileManager defaultManager].ubiquityIdentityToken != nil;
}

ScopedICloudKeychainOverride::~ScopedICloudKeychainOverride() = default;

struct Override : public ScopedICloudKeychainOverride {
  explicit Override(bool configured) {
    CHECK(!g_override);
    g_override = true;
    g_override_value = configured;
  }

  ~Override() override {
    CHECK(g_override);
    g_override = false;
  }
};

std::unique_ptr<ScopedICloudKeychainOverride>
OverrideICloudKeychainConfiguredForPasskeys(bool configured) {
  return std::make_unique<Override>(configured);
}
