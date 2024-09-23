// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/password_protection/password_protection_service.h"
#include "content/public/browser/browser_thread.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/metrics/histogram_macros.h"
#include "base/strings/escape.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_reuse_detector.h"
#include "components/safe_browsing/content/browser/password_protection/password_protection_commit_deferring_condition.h"
#include "components/safe_browsing/content/browser/password_protection/password_protection_request_content.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/utils.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/google_api_keys.h"
#include "net/base/url_util.h"
#include "third_party/blink/public/common/page/page_zoom.h"

using content::WebContents;
using password_manager::metrics_util::PasswordType;

namespace safe_browsing {

PasswordReuseInfo::PasswordReuseInfo() = default;

PasswordReuseInfo::PasswordReuseInfo(const PasswordReuseInfo& other) = default;

PasswordReuseInfo::~PasswordReuseInfo() = default;

#if defined(ON_FOCUS_PING_ENABLED)
void PasswordProtectionService::MaybeStartPasswordFieldOnFocusRequest(
    WebContents* web_contents,
    const GURL& main_frame_url,
    const GURL& password_form_action,
    const GURL& password_form_frame_url,
    const std::string& hosted_domain) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  LoginReputationClientRequest::TriggerType trigger_type =
      LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE;
  ReusedPasswordAccountType reused_password_account_type =
      GetPasswordProtectionReusedPasswordAccountType(
          PasswordType::PASSWORD_TYPE_UNKNOWN,
          /*username=*/"");
  if (CanSendPing(trigger_type, main_frame_url, reused_password_account_type)) {
    StartRequest(web_contents, main_frame_url, password_form_action,
                 password_form_frame_url, /* username */ "",
                 PasswordType::PASSWORD_TYPE_UNKNOWN,
                 {}, /* matching_reused_credentials: not used for this type */
                 LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE, true);
  } else {
    RequestOutcome reason = GetPingNotSentReason(trigger_type, main_frame_url,
                                                 reused_password_account_type);
    LogNoPingingReason(trigger_type, reason, reused_password_account_type);
  }
}
#endif

void PasswordProtectionService::MaybeStartProtectedPasswordEntryRequest(
    WebContents* web_contents,
    const GURL& main_frame_url,
    const std::string& username,
    PasswordType password_type,
    const std::vector<password_manager::MatchingReusedCredential>&
        matching_reused_credentials,
    bool password_field_exists) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  LoginReputationClientRequest::TriggerType trigger_type =
      LoginReputationClientRequest::PASSWORD_REUSE_EVENT;
  ReusedPasswordAccountType reused_password_account_type =
      GetPasswordProtectionReusedPasswordAccountType(password_type, username);
  bool can_show_interstitial =
      CanShowInterstitial(reused_password_account_type, main_frame_url);
  if (IsSupportedPasswordTypeForPinging(password_type)) {
    if (CanSendPing(LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                    main_frame_url, reused_password_account_type)) {
      saved_passwords_matching_reused_credentials_ =
          matching_reused_credentials;
      StartRequest(web_contents, main_frame_url, GURL(), GURL(), username,
                   password_type, matching_reused_credentials,
                   LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                   password_field_exists);
    } else {
      RequestOutcome reason = GetPingNotSentReason(
          trigger_type, main_frame_url, reused_password_account_type);
      LogNoPingingReason(trigger_type, reason, reused_password_account_type);

// Disabled on Android, because enterprise reporting extension is not supported.
#if !BUILDFLAG(IS_ANDROID)
      if (reason == RequestOutcome::PASSWORD_ALERT_MODE) {
        MaybeReportPasswordReuseDetected(
            main_frame_url, username, password_type, /*is_phishing_url=*/false,
            can_show_interstitial);
      }
#endif
      if (reused_password_account_type.is_account_syncing())
        MaybeLogPasswordReuseLookupEvent(web_contents, reason, password_type,
                                         nullptr);
    }
  }

  if (can_show_interstitial) {
    LogPasswordAlertModeOutcome(RequestOutcome::SUCCEEDED,
                                reused_password_account_type);
    username_for_last_shown_warning_ = username;
    reused_password_account_type_for_last_shown_warning_ =
        reused_password_account_type;
    ShowInterstitial(web_contents, reused_password_account_type);
  }
}

void PasswordProtectionService::StartRequest(
    WebContents* web_contents,
    const GURL& main_frame_url,
    const GURL& password_form_action,
    const GURL& password_form_frame_url,
    const std::string& username,
    PasswordType password_type,
    const std::vector<password_manager::MatchingReusedCredential>&
        matching_reused_credentials,
    LoginReputationClientRequest::TriggerType trigger_type,
    bool password_field_exists) {
  scoped_refptr<PasswordProtectionRequest> request(
      new PasswordProtectionRequestContent(
          web_contents, main_frame_url, password_form_action,
          password_form_frame_url, web_contents->GetContentsMimeType(),
          username, password_type, matching_reused_credentials, trigger_type,
          password_field_exists, this, GetRequestTimeoutInMS()));
  StartRequestInternal(std::move(request));
}

void PasswordProtectionService::StartRequestForTesting(
    WebContents* web_contents,
    const GURL& main_frame_url,
    const GURL& password_form_action,
    const GURL& password_form_frame_url,
    const std::string& username,
    PasswordType password_type,
    const std::vector<password_manager::MatchingReusedCredential>&
        matching_reused_credentials,
    LoginReputationClientRequest::TriggerType trigger_type,
    bool password_field_exists) {
  scoped_refptr<PasswordProtectionRequest> request =
      PasswordProtectionRequestContent::CreateForTesting(
          web_contents, main_frame_url, password_form_action,
          password_form_frame_url, web_contents->GetContentsMimeType(),
          username, password_type, matching_reused_credentials, trigger_type,
          password_field_exists, this, GetRequestTimeoutInMS());

  StartRequestInternal(std::move(request));
}

void PasswordProtectionService::StartRequestInternal(
    scoped_refptr<PasswordProtectionRequest> request) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  request->Start();
  pending_requests_.insert(std::move(request));
}

std::unique_ptr<PasswordProtectionCommitDeferringCondition>
PasswordProtectionService::MaybeCreateCommitDeferringCondition(
    content::NavigationHandle& navigation_handle) {
  // Don't defer navigations that the user started themselves. This won't
  // matter if the dialog is showing, since it's modal, but if a password reuse
  // ping is in progress we shouldn't block the user from navigating.
  if (!navigation_handle.IsRendererInitiated())
    return nullptr;

  // The goal of the deferral is to ensure the correct URL is in the omnibox
  // while the warning modal is showing. Only the primary main frame affects
  // the omnibox.
  if (!navigation_handle.IsInPrimaryMainFrame())
    return nullptr;

  content::WebContents* web_contents = navigation_handle.GetWebContents();
  for (scoped_refptr<PasswordProtectionRequest> request : pending_requests_) {
    PasswordProtectionRequestContent* request_content =
        static_cast<PasswordProtectionRequestContent*>(request.get());
    if (request_content->web_contents() == web_contents &&
        request->trigger_type() ==
            safe_browsing::LoginReputationClientRequest::PASSWORD_REUSE_EVENT &&
        IsSupportedPasswordTypeForModalWarning(
            GetPasswordProtectionReusedPasswordAccountType(
                request->password_type(), username_for_last_shown_warning()))) {
      return std::make_unique<PasswordProtectionCommitDeferringCondition>(
          navigation_handle, *request_content);
    }
  }

  for (scoped_refptr<PasswordProtectionRequest> request : warning_requests_) {
    PasswordProtectionRequestContent* request_content =
        static_cast<PasswordProtectionRequestContent*>(request.get());
    if (request_content->web_contents() == web_contents) {
      return std::make_unique<PasswordProtectionCommitDeferringCondition>(
          navigation_handle, *request_content);
    }
  }
  return nullptr;
}

void PasswordProtectionService::RemoveWarningRequestsByWebContents(
    content::WebContents* web_contents) {
  for (auto it = warning_requests_.begin(); it != warning_requests_.end();) {
    PasswordProtectionRequestContent* request_content =
        static_cast<PasswordProtectionRequestContent*>(it->get());
    if (request_content->web_contents() == web_contents) {
      request_content->ResumeDeferredNavigations();
      it = warning_requests_.erase(it);
    } else {
      ++it;
    }
  }
}

bool PasswordProtectionService::IsModalWarningShowingInWebContents(
    content::WebContents* web_contents) {
  for (const auto& request : warning_requests_) {
    PasswordProtectionRequestContent* request_content =
        static_cast<PasswordProtectionRequestContent*>(request.get());
    if (request_content->web_contents() == web_contents)
      return true;
  }
  return false;
}

void PasswordProtectionService::ResumeDeferredNavigationsIfNeeded(
    PasswordProtectionRequest* request) {
  if (request->is_modal_warning_showing())
    return;

  PasswordProtectionRequestContent* request_content =
      static_cast<PasswordProtectionRequestContent*>(request);
  request_content->ResumeDeferredNavigations();
}

}  // namespace safe_browsing
