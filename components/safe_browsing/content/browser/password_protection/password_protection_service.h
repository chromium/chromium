// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_PASSWORD_PROTECTION_PASSWORD_PROTECTION_SERVICE_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_PASSWORD_PROTECTION_PASSWORD_PROTECTION_SERVICE_H_

#include "build/build_config.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_reuse_detector.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom.h"
#include "components/safe_browsing/core/browser/password_protection/metrics_util.h"
#include "components/safe_browsing/core/browser/password_protection/password_protection_service_base.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/browser/commit_deferring_condition.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

class GURL;

namespace safe_browsing {

class PasswordProtectionCommitDeferringCondition;
class PasswordProtectionRequest;

using ReusedPasswordAccountType =
    LoginReputationClientRequest::PasswordReuseEvent::ReusedPasswordAccountType;
using password_manager::metrics_util::PasswordType;

using PasswordReuseEvent =
    safe_browsing::LoginReputationClientRequest::PasswordReuseEvent;
using ReusedPasswordType = safe_browsing::LoginReputationClientRequest::
    PasswordReuseEvent::ReusedPasswordType;
using SyncAccountType =
    LoginReputationClientRequest::PasswordReuseEvent::SyncAccountType;

struct PasswordReuseInfo {
  PasswordReuseInfo();
  PasswordReuseInfo(const PasswordReuseInfo& other);
  ~PasswordReuseInfo();
  bool matches_signin_password;
  ReusedPasswordAccountType reused_password_account_type;
  std::vector<std::string> matching_domains;
  uint64_t reused_password_hash;
  int count{0};
};

class PasswordProtectionService : public PasswordProtectionServiceBase {
  using PasswordProtectionServiceBase::PasswordProtectionServiceBase;

 public:
  // Creates an instance of PasswordProtectionRequest and call Start() on that
  // instance. This function also insert this request object in |requests_| for
  // record keeping.
  void StartRequest(
      content::WebContents* web_contents,
      const GURL& main_frame_url,
      const GURL& password_form_action,
      const GURL& password_form_frame_url,
      const std::string& username,
      PasswordType password_type,
      const std::vector<password_manager::MatchingReusedCredential>&
          matching_reused_credentials,
      LoginReputationClientRequest::TriggerType trigger_type,
      bool password_field_exists);

  // Same as above but uses a PasswordProtectionRequest that avoids sending
  // real requests that can be used for testing.
  void StartRequestForTesting(
      content::WebContents* web_contents,
      const GURL& main_frame_url,
      const GURL& password_form_action,
      const GURL& password_form_frame_url,
      const std::string& username,
      PasswordType password_type,
      const std::vector<password_manager::MatchingReusedCredential>&
          matching_reused_credentials,
      LoginReputationClientRequest::TriggerType trigger_type,
      bool password_field_exists);

#if defined(ON_FOCUS_PING_ENABLED)
  virtual void MaybeStartPasswordFieldOnFocusRequest(
      content::WebContents* web_contents,
      const GURL& main_frame_url,
      const GURL& password_form_action,
      const GURL& password_form_frame_url,
      const std::string& hosted_domain);
#endif

  virtual void MaybeStartProtectedPasswordEntryRequest(
      content::WebContents* web_contents,
      const GURL& main_frame_url,
      const std::string& username,
      PasswordType password_type,
      const std::vector<password_manager::MatchingReusedCredential>&
          matching_reused_credentials,
      bool password_field_exists);

  // Records a Chrome Sync event that sync password reuse was detected.
  virtual void MaybeLogPasswordReuseDetectedEvent(
      content::WebContents* web_contents) = 0;

  // Records a Chrome Sync event for the result of the URL reputation lookup
  // if the user enters their sync password on a website.
  virtual void MaybeLogPasswordReuseLookupEvent(
      content::WebContents* web_contents,
      RequestOutcome outcome,
      PasswordType password_type,
      const LoginReputationClientResponse* response) = 0;

  // Shows chrome://reset-password interstitial.
  virtual void ShowInterstitial(content::WebContents* web_contents,
                                ReusedPasswordAccountType password_type) = 0;

  virtual void UpdateSecurityState(safe_browsing::SBThreatType threat_type,
                                   ReusedPasswordAccountType password_type,
                                   content::WebContents* web_contents) = 0;

#if BUILDFLAG(IS_ANDROID)
  // Returns the referring app info that starts the activity.
  virtual LoginReputationClientRequest::ReferringAppInfo GetReferringAppInfo(
      content::WebContents* web_contents) = 0;
#endif

  // Called when a new navigation is starting to create a deferring condition
  // if there is a pending sync password reuse ping or if there is a modal
  // warning dialog showing in the corresponding web contents.
  std::unique_ptr<PasswordProtectionCommitDeferringCondition>
  MaybeCreateCommitDeferringCondition(
      content::NavigationHandle& navigation_handle);

 protected:
  void StartRequestInternal(scoped_refptr<PasswordProtectionRequest> request);

  void RemoveWarningRequestsByWebContents(content::WebContents* web_contents);

  bool IsModalWarningShowingInWebContents(content::WebContents* web_contents);

  void ResumeDeferredNavigationsIfNeeded(
      PasswordProtectionRequest* request) override;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_PASSWORD_PROTECTION_PASSWORD_PROTECTION_SERVICE_H_
