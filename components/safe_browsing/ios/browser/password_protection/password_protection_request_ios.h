// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_IOS_BROWSER_PASSWORD_PROTECTION_PASSWORD_PROTECTION_REQUEST_IOS_H_
#define COMPONENTS_SAFE_BROWSING_IOS_BROWSER_PASSWORD_PROTECTION_PASSWORD_PROTECTION_REQUEST_IOS_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/safe_browsing/core/browser/password_protection/password_protection_request.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

class GURL;
class RequestCanceler;

namespace web {
class WebState;
}

namespace safe_browsing {

class PasswordProtectionServiceBase;

class PasswordProtectionRequestIOS final : public PasswordProtectionRequest {
 public:
  PasswordProtectionRequestIOS(
      web::WebState* web_state,
      const GURL& main_frame_url,
      const std::string& mime_type,
      const std::string& user_name,
      password_manager::metrics_util::PasswordType password_type,
      const std::vector<password_manager::MatchingReusedCredential>&
          matching_reused_credentials,
      LoginReputationClientRequest::TriggerType type,
      bool password_field_exists,
      PasswordProtectionServiceBase* pps,
      int request_timeout_in_ms);

  web::WebState* web_state() const { return web_state_; }
  base::WeakPtr<PasswordProtectionRequest> AsWeakPtr() override;

 private:
  ~PasswordProtectionRequestIOS() override;

  void MaybeLogPasswordReuseLookupEvent(
      RequestOutcome outcome,
      const LoginReputationClientResponse* response) override;

  // WebState corresponding to the password protection event.
  raw_ptr<web::WebState> web_state_;

  // Cancels the request when it is no longer valid.
  std::unique_ptr<RequestCanceler> request_canceler_;

  base::WeakPtrFactory<PasswordProtectionRequestIOS> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_IOS_BROWSER_PASSWORD_PROTECTION_PASSWORD_PROTECTION_REQUEST_IOS_H_
