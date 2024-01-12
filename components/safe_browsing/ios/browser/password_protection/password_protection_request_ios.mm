// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/safe_browsing/ios/browser/password_protection/password_protection_request_ios.h"

#import "components/safe_browsing/core/browser/password_protection/request_canceler.h"
#import "components/safe_browsing/ios/browser/password_protection/password_protection_service.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#import "ios/web/public/web_state.h"
#include "url/gurl.h"

using password_manager::metrics_util::PasswordType;

namespace safe_browsing {

PasswordProtectionRequestIOS::~PasswordProtectionRequestIOS() = default;

PasswordProtectionRequestIOS::PasswordProtectionRequestIOS(
    web::WebState* web_state,
    const GURL& main_frame_url,
    const std::string& mime_type,
    const std::string& username,
    PasswordType password_type,
    const std::vector<password_manager::MatchingReusedCredential>&
        matching_reused_credentials,
    LoginReputationClientRequest::TriggerType type,
    bool password_field_exists,
    PasswordProtectionServiceBase* pps,
    int request_timeout_in_ms)
    : PasswordProtectionRequest(web::GetUIThreadTaskRunner({}),
                                web::GetIOThreadTaskRunner({}),
                                main_frame_url,
                                /*password_form_action=*/GURL(),
                                /*password_frame_url=*/GURL(),
                                mime_type,
                                username,
                                password_type,
                                matching_reused_credentials,
                                type,
                                password_field_exists,
                                pps,
                                request_timeout_in_ms),
      web_state_(web_state) {
  request_canceler_ = RequestCanceler::CreateRequestCanceler(
      weak_factory_.GetWeakPtr(), web_state);
}

base::WeakPtr<PasswordProtectionRequest>
PasswordProtectionRequestIOS::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void PasswordProtectionRequestIOS::MaybeLogPasswordReuseLookupEvent(
    RequestOutcome outcome,
    const LoginReputationClientResponse* response) {
  PasswordProtectionService* service =
      static_cast<PasswordProtectionService*>(password_protection_service());
  service->MaybeLogPasswordReuseLookupEvent(web_state_, outcome,
                                            password_type(), response);
}

}  // namespace safe_browsing
