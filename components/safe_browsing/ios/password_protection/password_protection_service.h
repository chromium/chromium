// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_IOS_PASSWORD_PROTECTION_PASSWORD_PROTECTION_SERVICE_H_
#define COMPONENTS_SAFE_BROWSING_IOS_PASSWORD_PROTECTION_PASSWORD_PROTECTION_SERVICE_H_

#include "components/safe_browsing/core/password_protection/password_protection_service_base.h"

namespace web {
class WebState;
}

namespace safe_browsing {

class PasswordProtectionService : public PasswordProtectionServiceBase {
  using PasswordProtectionServiceBase::PasswordProtectionServiceBase;

 public:
  // Records a Chrome Sync event for the result of the URL reputation lookup
  // if the user enters their sync password on a website.
  virtual void MaybeLogPasswordReuseLookupEvent(
      web::WebState* web_state,
      RequestOutcome outcome,
      PasswordType password_type,
      const LoginReputationClientResponse* response) = 0;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_IOS_PASSWORD_PROTECTION_PASSWORD_PROTECTION_SERVICE_H_
