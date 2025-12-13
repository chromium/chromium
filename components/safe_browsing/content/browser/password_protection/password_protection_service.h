// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_PASSWORD_PROTECTION_PASSWORD_PROTECTION_SERVICE_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_PASSWORD_PROTECTION_PASSWORD_PROTECTION_SERVICE_H_

#include "build/build_config.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_reuse_detector.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/browser/password_protection/metrics_util.h"
#include "components/safe_browsing/core/browser/password_protection/password_protection_request.h"
#include "components/safe_browsing/core/browser/password_protection/password_protection_service_base.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/browser/commit_deferring_condition.h"

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
  PasswordProtectionService(
      const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      history::HistoryService* history_service,
      PrefService* pref_service,
      std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher,
      bool is_off_the_record,
      signin::IdentityManager* identity_manager,
      bool try_token_fetch,
      SafeBrowsingMetricsCollector* metrics_collector);
  ~PasswordProtectionService() override;

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
      bool password_field_exists,
      std::optional<PasswordProtectionRequest::OtpPhishingVerdictCallback>
          otp_phishing_verdict_callback = std::nullopt);

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
      bool password_field_exists,
      std::optional<PasswordProtectionRequest::OtpPhishingVerdictCallback>
          otp_phishing_verdict_callback = std::nullopt);

#if defined(ON_FOCUS_PING_ENABLED)
  virtual void MaybeStartPasswordFieldOnFocusRequest(
      content::WebContents* web_contents,
      const GURL& main_frame_url,
      const GURL& password_form_action,
      const GURL& password_form_frame_url);
#endif

  virtual void MaybeStartProtectedPasswordEntryRequest(
      content::WebContents* web_contents,
      const GURL& main_frame_url,
      const std::string& username,
      PasswordType password_type,
      const std::vector<password_manager::MatchingReusedCredential>&
          matching_reused_credentials,
      bool password_field_exists);

  // Starts a request to check if the current page is a potential phishing site
  // for one time password filling.
  void MaybeStartOtpPhishingRequest(
      content::WebContents* web_contents,
      const GURL& main_frame_url,
      PasswordProtectionRequest::OtpPhishingVerdictCallback callback);

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
  virtual ReferringAppInfo GetReferringAppInfo(
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

  void OnOtpHighConfidenceAllowlistCheckCompleted(
      content::WebContents* web_contents,
      const GURL& main_frame_url,
      PasswordProtectionRequest::OtpPhishingVerdictCallback callback,
      bool did_match_allowlist,
      std::optional<SafeBrowsingDatabaseManager::
                        HighConfidenceAllowlistCheckLoggingDetails>
          logging_details);

  base::WeakPtrFactory<PasswordProtectionService> weak_ptr_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_PASSWORD_PROTECTION_PASSWORD_PROTECTION_SERVICE_H_
