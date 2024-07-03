// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include "chrome/browser/webauthn/chrome_authenticator_request_delegate_mac.h"

bool IsICloudDriveEnabled() {
  return [NSFileManager defaultManager].ubiquityIdentityToken != nil;
}
