// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_IOS_PASSWORD_PROTECTION_PASSWORD_PROTECTION_SERVICE_H_
#define COMPONENTS_SAFE_BROWSING_IOS_PASSWORD_PROTECTION_PASSWORD_PROTECTION_SERVICE_H_

#include <string>
#include <vector>

#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/safe_browsing/core/password_protection/metrics_util.h"
#include "components/safe_browsing/core/password_protection/password_protection_service_base.h"

class GURL;

namespace web {
class WebState;
}

namespace safe_browsing {

class PasswordProtectionService : public PasswordProtectionServiceBase {
  using PasswordProtectionServiceBase::PasswordProtectionServiceBase;

 public:
  using ShowWarningCallback =
      base::OnceCallback<void(const base::string16& warning_text)>;

  virtual void MaybeStartProtectedPasswordEntryRequest(
      web::WebState* web_state,
      const GURL& main_frame_url,
      const std::string& username,
      PasswordType password_type,
      const std::vector<password_manager::MatchingReusedCredential>&
          matching_reused_credentials,
      bool password_field_exists,
      ShowWarningCallback show_warning_callback) = 0;

  // Records a Chrome Sync event for the result of the URL reputation lookup
  // if the user enters their sync password on a website.
  virtual void MaybeLogPasswordReuseLookupEvent(
      web::WebState* web_state,
      RequestOutcome outcome,
      PasswordType password_type,
      const LoginReputationClientResponse* response) = 0;

  // Records a Chrome Sync event that sync password reuse was detected.
  virtual void MaybeLogPasswordReuseDetectedEvent(web::WebState* web_state) = 0;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_IOS_PASSWORD_PROTECTION_PASSWORD_PROTECTION_SERVICE_H_
