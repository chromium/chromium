// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/chrome_authenticator_request_delegate_mac.h"

#import <Foundation/Foundation.h>

#include <memory>

#include "base/check.h"

static bool g_override = false;
static bool g_override_value = false;

bool IsICloudDriveEnabled() {
  if (g_override) {
    return g_override_value;
  }
  return [NSFileManager defaultManager].ubiquityIdentityToken != nil;
}

ScopedICloudDriveOverride::~ScopedICloudDriveOverride() = default;

struct Override : public ScopedICloudDriveOverride {
  explicit Override(bool enabled) {
    CHECK(!g_override);
    g_override = true;
    g_override_value = enabled;
  }

  ~Override() override {
    CHECK(g_override);
    g_override = false;
  }
};

std::unique_ptr<ScopedICloudDriveOverride> OverrideICloudDriveEnabled(
    bool enabled) {
  return std::make_unique<Override>(enabled);
}
