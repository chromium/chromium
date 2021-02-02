// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_IOS_PASSWORD_PROTECTION_PASSWORD_PROTECTION_REQUEST_IOS_H_
#define COMPONENTS_SAFE_BROWSING_IOS_PASSWORD_PROTECTION_PASSWORD_PROTECTION_REQUEST_IOS_H_

#include "components/safe_browsing/core/password_protection/password_protection_request.h"

namespace safe_browsing {

class PasswordProtectionRequestIOS : public PasswordProtectionRequest {
 private:
  ~PasswordProtectionRequestIOS() override;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_IOS_PASSWORD_PROTECTION_PASSWORD_PROTECTION_REQUEST_IOS_H_
