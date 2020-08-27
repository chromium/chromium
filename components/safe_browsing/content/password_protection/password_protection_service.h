// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_PASSWORD_PROTECTION_PASSWORD_PROTECTION_SERVICE_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_PASSWORD_PROTECTION_PASSWORD_PROTECTION_SERVICE_H_

#include <set>
#include <unordered_map>

#include "base/callback.h"
#include "base/feature_list.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_reuse_detector.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom.h"
#include "components/safe_browsing/content/password_protection/metrics_util.h"
#include "components/safe_browsing/core/browser/referrer_chain_provider.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/proto/csd.pb.h"
#include "components/sessions/core/session_id.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"

namespace content {
class WebContents;
class NavigationHandle;
}

namespace policy {
class BrowserPolicyConnector;
}

class GURL;
class HostContentSettingsMap;

namespace safe_browsing {

class PasswordProtectionNavigationThrottle;
class PasswordProtectionRequest;
class SafeBrowsingDatabaseManager;

using ReusedPasswordAccountType =
    LoginReputationClientRequest::PasswordReuseEvent::ReusedPasswordAccountType;
using ReusedPasswordType =
    LoginReputationClientRequest::PasswordReuseEvent::ReusedPasswordType;
using password_manager::metrics_util::PasswordType;

// Manage password protection pings and verdicts. There is one instance of this
// class per profile. Therefore, every PasswordProtectionService instance is
// associated with a unique HistoryService instance and a unique
// HostContentSettingsMap instance.
class PasswordProtectionService : public history::HistoryServiceObserver {
 public:
  PasswordProtectionService(
      const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      history::HistoryService* history_service);

  ~PasswordProtectionService() override;

  base::WeakPtr<PasswordProtectionService> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Looks up |settings| to find the cached verdict response. If verdict is not
  // available or is expired, return VERDICT_TYPE_UNSPECIFIED. Can be called on
  // any thread.
  virtual LoginReputationClientResponse::VerdictType GetCachedVerdict(
      const GURL& url,
      LoginReputationClientRequest::TriggerType trigger_type,
      ReusedPasswordAccountType password_type,
      LoginReputationClientResponse* out_response);

  // Stores |verdict| in |settings| based on its |trigger_type|, |url|,
  // reused |password_type|, |verdict| and |receive_time|.
  virtual void CacheVerdict(
      const GURL& url,
      LoginReputationClientRequest::TriggerType trigger_type,
      ReusedPasswordAccountType password_type,
      const LoginReputationClientResponse& verdict,
      const base::Time& receive_time);

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

#if defined(ON_FOCUS_PING_ENABLED)
  virtual void MaybeStartPasswordFieldOnFocusRequest(
      content::WebContents* web_contents,
      const GURL& main_frame_url,
      const GURL& password_form_action,
      const GURL& password_form_frame_url,
      const std::string& hosted_domain);
#endif

#if defined(PASSWORD_REUSE_DETECTION_ENABLED)
  virtual void MaybeStartProtectedPasswordEntryRequest(
      content::WebContents* web_contents,
      const GURL& main_frame_url,
      const std::string& username,
      PasswordType password_type,
      const std::vector<password_manager::MatchingReusedCredential>&
          matching_reused_credentials,
      bool password_field_exists);
#endif

#if defined(PASSWORD_REUSE_WARNING_ENABLED)
  // Records a Chrome Sync event that sync password reuse was detected.
  virtual void MaybeLogPasswordReuseDetectedEvent(
      content::WebContents* web_contents) = 0;

  // If we want to show password reuse modal warning.
  bool ShouldShowModalWarning(
      LoginReputationClientRequest::TriggerType trigger_type,
      ReusedPasswordAccountType password_type,
      LoginReputationClientResponse::VerdictType verdict_type);

  // Shows modal warning dialog on the current |web_contents| and pass the
  // |verdict_token| to callback of this dialog.
  virtual void ShowModalWarning(
      content::WebContents* web_contents,
      RequestOutcome outcome,
      LoginReputationClientResponse::VerdictType verdict_type,
      const std::string& verdict_token,
      ReusedPasswordAccountType password_type) = 0;

  // Shows chrome://reset-password interstitial.
  virtual void ShowInterstitial(content::WebContents* web_contens,
                                ReusedPasswordAccountType password_type) = 0;
#endif

// The following functions are disabled on Android, because enterprise reporting
// extension is not supported.
#if !defined(OS_ANDROID)
  // Triggers the safeBrowsingPrivate.OnPolicySpecifiedPasswordReuseDetected.
  virtual void MaybeReportPasswordReuseDetected(
      content::WebContents* web_contents,
      const std::string& username,
      PasswordType password_type,
      bool is_phishing_url) = 0;

  // Called when a protected password change is detected. Must be called on
  // UI thread.
  virtual void ReportPasswordChanged() = 0;
#endif

#if defined(PASSWORD_REUSE_WARNING_ENABLED)
  virtual void UpdateSecurityState(safe_browsing::SBThreatType threat_type,
                                   ReusedPasswordAccountType password_type,
                                   content::WebContents* web_contents) = 0;
#endif

  scoped_refptr<SafeBrowsingDatabaseManager> database_manager();

  // Safe Browsing backend cannot get a reliable reputation of a URL if
  // (1) URL is not valid
  // (2) URL doesn't have http or https scheme
  // (3) It maps to a local host.
  // (4) Its hostname is an IP Address in an IANA-reserved range.
  // (5) Its hostname is a not-yet-assigned by ICANN gTLD.
  // (6) Its hostname is a dotless domain.
  static bool CanGetReputationOfURL(const GURL& url);

  // If user has clicked through any Safe Browsing interstitial on this given
  // |web_contents|.
  virtual bool UserClickedThroughSBInterstitial(
      content::WebContents* web_contents) = 0;

  // Called when a new navigation is starting. Create throttle if there is a
  // pending sync password reuse ping or if there is a modal warning dialog
  // showing in the corresponding web contents.
  std::unique_ptr<PasswordProtectionNavigationThrottle>
  MaybeCreateNavigationThrottle(content::NavigationHandle* navigation_handle);

  // Returns if the warning UI is enabled.
  bool IsWarningEnabled(ReusedPasswordAccountType password_type);

  // Returns the pref value of password protection warning trigger.
  virtual PasswordProtectionTrigger GetPasswordProtectionWarningTriggerPref(
      ReusedPasswordAccountType password_type) const = 0;

  // If |url| matches Safe Browsing whitelist domains, password protection
  // change password URL, or password protection login URLs in the enterprise
  // policy.
  virtual bool IsURLWhitelistedForPasswordEntry(const GURL& url) const = 0;

  // Persist the phished saved password credential in the "compromised
  // credentials" table. Calls the password store to add a row for each
  // MatchingReusedCredential where the phished saved password is used on.
  virtual void PersistPhishedSavedPasswordCredential(
      const std::vector<password_manager::MatchingReusedCredential>&
          matching_reused_credentials) = 0;

  // Remove all rows of the phished saved password credential in the
  // "compromised credentials" table. Calls the password store to remove a row
  // for each
  virtual void RemovePhishedSavedPasswordCredential(
      const std::vector<password_manager::MatchingReusedCredential>&
          matching_reused_credentials) = 0;

  // Converts from password::metrics_util::PasswordType to
  // LoginReputationClientRequest::PasswordReuseEvent::ReusedPasswordType.
  static ReusedPasswordType GetPasswordProtectionReusedPasswordType(
      PasswordType password_type);

  // Converts from password_manager::metrics_util::PasswordType
  // to PasswordReuseEvent::ReusedPasswordAccountType. |username| is only
  // used if |password_type| is OTHER_GAIA_PASSWORD because it needs to be
  // compared to the list of signed in accounts.
  ReusedPasswordAccountType GetPasswordProtectionReusedPasswordAccountType(
      PasswordType password_type,
      const std::string& username) const;

  // Converts from ReusedPasswordAccountType to
  // password_manager::metrics_util::PasswordType.
  static PasswordType ConvertReusedPasswordAccountTypeToPasswordType(
      ReusedPasswordAccountType password_type);

  // If we can send ping for this type of reused password.
  bool IsSupportedPasswordTypeForPinging(PasswordType password_type) const;

  // If we can show modal warning for this type of reused password.
  bool IsSupportedPasswordTypeForModalWarning(
      ReusedPasswordAccountType password_type) const;

  const ReusedPasswordAccountType&
  reused_password_account_type_for_last_shown_warning() const {
    return reused_password_account_type_for_last_shown_warning_;
  }
#if defined(UNIT_TEST)
  void set_reused_password_account_type_for_last_shown_warning(
      ReusedPasswordAccountType
          reused_password_account_type_for_last_shown_warning) {
    reused_password_account_type_for_last_shown_warning_ =
        reused_password_account_type_for_last_shown_warning;
  }
#endif

  const std::string& username_for_last_shown_warning() const {
    return username_for_last_shown_warning_;
  }
#if defined(UNIT_TEST)
  void set_username_for_last_shown_warning(const std::string& username) {
    username_for_last_shown_warning_ = username;
  }
#endif

  const std::vector<password_manager::MatchingReusedCredential>&
  saved_passwords_matching_reused_credentials() const {
    return saved_passwords_matching_reused_credentials_;
  }
#if defined(UNIT_TEST)
  void set_saved_passwords_matching_reused_credentials(
      const std::vector<password_manager::MatchingReusedCredential>&
          credentials) {
    saved_passwords_matching_reused_credentials_ = credentials;
  }
#endif
  const std::vector<std::string>& saved_passwords_matching_domains() const {
    return saved_passwords_matching_domains_;
  }
#if defined(UNIT_TEST)
  void set_saved_passwords_matching_domains(
      const std::vector<std::string>& matching_domains) {
    saved_passwords_matching_domains_ = matching_domains;
  }
#endif

  virtual AccountInfo GetAccountInfo() const = 0;

 protected:
  friend class PasswordProtectionRequest;

  // Chrome can send password protection ping if it is allowed by for the
  // |trigger_type| and |password_type| and if Safe Browsing can compute
  // reputation of |main_frame_url| (e.g. Safe Browsing is not able to compute
  // reputation of a private IP or a local host).
  bool CanSendPing(LoginReputationClientRequest::TriggerType trigger_type,
                   const GURL& main_frame_url,
                   ReusedPasswordAccountType password_type);

  // Called by a PasswordProtectionRequest instance when it finishes to remove
  // itself from |requests_|.
  virtual void RequestFinished(
      PasswordProtectionRequest* request,
      RequestOutcome outcome,
      std::unique_ptr<LoginReputationClientResponse> response);

  // Called by a PasswordProtectionRequest instance to check if a sample ping
  // can be sent to Safe Browsing.
  virtual bool CanSendSamplePing() = 0;

  // Sanitize referrer chain by only keeping origin information of all URLs.
  virtual void SanitizeReferrerChain(ReferrerChain* referrer_chain) = 0;

  // Cancels all requests in |requests_|, empties it, and releases references to
  // the requests.
  void CancelPendingRequests();

  // Gets the total number of verdicts of the specified |trigger_type| we cached
  // for this profile. This counts both expired and active verdicts.
  virtual int GetStoredVerdictCount(
      LoginReputationClientRequest::TriggerType trigger_type);

  // Gets an unowned |BrowserPolicyConnector| for the current platform.
  virtual const policy::BrowserPolicyConnector* GetBrowserPolicyConnector()
      const = 0;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory() {
    return url_loader_factory_;
  }

  // Returns the URL where PasswordProtectionRequest instances send requests.
  static GURL GetPasswordProtectionRequestUrl();

  // Gets the request timeout in milliseconds.
  static int GetRequestTimeoutInMS();

  // Obtains referrer chain of |event_url| and |event_tab_id| and adds this
  // info into |frame|.
  virtual void FillReferrerChain(
      const GURL& event_url,
      SessionID
          event_tab_id,  // SessionID::InvalidValue() if tab not available.
      LoginReputationClientRequest::Frame* frame) = 0;

  void FillUserPopulation(
      LoginReputationClientRequest::TriggerType trigger_type,
      LoginReputationClientRequest* request_proto);

  virtual bool IsExtendedReporting() = 0;

  virtual bool IsEnhancedProtection() = 0;

  virtual bool IsIncognito() = 0;

  virtual bool IsUserMBBOptedIn() = 0;

  virtual bool IsInPasswordAlertMode(
      ReusedPasswordAccountType password_type) = 0;

  virtual bool IsPingingEnabled(
      LoginReputationClientRequest::TriggerType trigger_type,
      ReusedPasswordAccountType password_type) = 0;

  virtual bool IsHistorySyncEnabled() = 0;

  // If primary account is syncing.
  virtual bool IsPrimaryAccountSyncing() const = 0;

  // If primary account is signed in.
  virtual bool IsPrimaryAccountSignedIn() const = 0;

  // If a domain is not defined for the primary account. This means the primary
  // account is a Gmail account.
  virtual bool IsPrimaryAccountGmail() const = 0;

  // If the domain for the non sync account is equal to |kNoHostedDomainFound|,
  // this means that the account is a Gmail account.
  virtual bool IsOtherGaiaAccountGmail(const std::string& username) const = 0;

  // Gets the account based off of the username from a list of signed in
  // accounts.
  virtual AccountInfo GetSignedInNonSyncAccount(
      const std::string& username) const = 0;

#if BUILDFLAG(FULL_SAFE_BROWSING)
  virtual bool IsUnderAdvancedProtection() = 0;
#endif

  // If Safe browsing endpoint is not enabled in the country.
  virtual bool IsInExcludedCountry() = 0;

#if defined(PASSWORD_REUSE_WARNING_ENABLED)
  // Records a Chrome Sync event for the result of the URL reputation lookup
  // if the user enters their sync password on a website.
  virtual void MaybeLogPasswordReuseLookupEvent(
      content::WebContents* web_contents,
      RequestOutcome,
      PasswordType password_type,
      const LoginReputationClientResponse*) = 0;

  void RemoveWarningRequestsByWebContents(content::WebContents* web_contents);

  bool IsModalWarningShowingInWebContents(content::WebContents* web_contents);

  // Determines if we should show chrome://reset-password interstitial based on
  // the reused |password_type| and the |main_frame_url|.
  virtual bool CanShowInterstitial(ReusedPasswordAccountType password_type,
                                   const GURL& main_frame_url) = 0;
#endif

  void CheckCsdWhitelistOnIOThread(const GURL& url, bool* check_result);

  // Gets the type of sync account associated with current profile or
  // |NOT_SIGNED_IN|.
  virtual LoginReputationClientRequest::PasswordReuseEvent::SyncAccountType
  GetSyncAccountType() const = 0;

  // Get information about Delayed Warnings and Omnibox URL display experiments.
  // This information is sent in PhishGuard pings.
  virtual LoginReputationClientRequest::UrlDisplayExperiment
  GetUrlDisplayExperiment() const = 0;

  // Returns the reason why a ping is not sent based on the |trigger_type|,
  // |url| and |password_type|. Crash if |CanSendPing| is true.
  virtual RequestOutcome GetPingNotSentReason(
      LoginReputationClientRequest::TriggerType trigger_type,
      const GURL& url,
      ReusedPasswordAccountType password_type) = 0;

  const std::list<std::string>& common_spoofed_domains() const {
    return common_spoofed_domains_;
  }

 private:
  friend class PasswordProtectionServiceTest;
  friend class TestPasswordProtectionService;
  friend class ChromePasswordProtectionServiceTest;
  friend class ChromePasswordProtectionServiceBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(PasswordProtectionServiceTest,
                           TestParseInvalidVerdictEntry);
  FRIEND_TEST_ALL_PREFIXES(PasswordProtectionServiceTest,
                           TestParseValidVerdictEntry);
  FRIEND_TEST_ALL_PREFIXES(PasswordProtectionServiceTest,
                           TestPathVariantsMatchCacheExpression);
  FRIEND_TEST_ALL_PREFIXES(PasswordProtectionServiceTest,
                           TestRemoveCachedVerdictOnURLsDeleted);
  FRIEND_TEST_ALL_PREFIXES(PasswordProtectionServiceTest,
                           TestCleanUpExpiredVerdict);

  // Overridden from history::HistoryServiceObserver.
  void OnURLsDeleted(history::HistoryService* history_service,
                     const history::DeletionInfo& deletion_info) override;

  void HistoryServiceBeingDeleted(
      history::HistoryService* history_service) override;

  // Posted to UI thread by OnURLsDeleted(...). This function remove the related
  // entries in kSafeBrowsingUnhandledSyncPasswordReuses.
  virtual void RemoveUnhandledSyncPasswordReuseOnURLsDeleted(
      bool all_history,
      const history::URLRows& deleted_rows) = 0;

  static bool PathVariantsMatchCacheExpression(
      const std::vector<std::string>& generated_paths,
      const std::string& cache_expression_path);

  void RecordNoPingingReason(
      LoginReputationClientRequest::TriggerType trigger_type,
      RequestOutcome reason,
      PasswordType password_type);

#if BUILDFLAG(FULL_SAFE_BROWSING)
  // Get the content area size of current browsing window.
  virtual gfx::Size GetCurrentContentAreaSize() const = 0;
#endif

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  // Binds the |phishing_detector| to the appropriate interface, as provided by
  // |provider|.
  virtual void GetPhishingDetector(
      service_manager::InterfaceProvider* provider,
      mojo::Remote<mojom::PhishingDetector>* phishing_detector);
#endif

  // The username of the account which password has been reused on. It is only
  // set once a modal warning or interstitial is verified to be shown.
  std::string username_for_last_shown_warning_ = "";

  // The last ReusedPasswordAccountType that was shown a warning or
  // interstitial.
  ReusedPasswordAccountType
      reused_password_account_type_for_last_shown_warning_;

  std::vector<password_manager::MatchingReusedCredential>
      saved_passwords_matching_reused_credentials_;

  std::vector<std::string> saved_passwords_matching_domains_;

  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;

  // The context we use to issue network requests. This request_context_getter
  // is obtained from SafeBrowsingService so that we can use the Safe Browsing
  // cookie store.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Set of pending PasswordProtectionRequests that are still waiting for
  // verdict.
  std::set<scoped_refptr<PasswordProtectionRequest>> pending_requests_;

  // Set of PasswordProtectionRequests that are triggering modal warnings.
  std::set<scoped_refptr<PasswordProtectionRequest>> warning_requests_;

  // List of most commonly spoofed domains to default to on the password warning
  // dialog.
  std::list<std::string> common_spoofed_domains_;

  ScopedObserver<history::HistoryService, history::HistoryServiceObserver>
      history_service_observer_{this};

  // Weakptr can only cancel task if it is posted to the same thread. Therefore,
  // we need CancelableTaskTracker to cancel tasks posted to IO thread.
  base::CancelableTaskTracker tracker_;

  base::WeakPtrFactory<PasswordProtectionService> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(PasswordProtectionService);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_PASSWORD_PROTECTION_PASSWORD_PROTECTION_SERVICE_H_
