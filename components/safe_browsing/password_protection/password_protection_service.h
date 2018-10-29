// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_PASSWORD_PROTECTION_PASSWORD_PROTECTION_SERVICE_H_
#define COMPONENTS_SAFE_BROWSING_PASSWORD_PROTECTION_PASSWORD_PROTECTION_SERVICE_H_

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
#include "components/history/core/browser/history_service_observer.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/safe_browsing/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/password_protection/metrics_util.h"
#include "components/safe_browsing/proto/csd.pb.h"
#include "components/sessions/core/session_id.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"

namespace content {
class WebContents;
class NavigationHandle;
}

namespace history {
class HistoryService;
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

using ReusedPasswordType =
    LoginReputationClientRequest::PasswordReuseEvent::ReusedPasswordType;

// Manage password protection pings and verdicts. There is one instance of this
// class per profile. Therefore, every PasswordProtectionService instance is
// associated with a unique HistoryService instance and a unique
// HostContentSettingsMap instance.
class PasswordProtectionService : public history::HistoryServiceObserver {
 public:

  PasswordProtectionService(
      const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      history::HistoryService* history_service,
      HostContentSettingsMap* host_content_settings_map);

  ~PasswordProtectionService() override;

  base::WeakPtr<PasswordProtectionService> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Looks up |settings| to find the cached verdict response. If verdict is not
  // available or is expired, return VERDICT_TYPE_UNSPECIFIED. Can be called on
  // any thread.
  LoginReputationClientResponse::VerdictType GetCachedVerdict(
      const GURL& url,
      LoginReputationClientRequest::TriggerType trigger_type,
      ReusedPasswordType password_type,
      LoginReputationClientResponse* out_response);

  // Stores |verdict| in |settings| based on its |trigger_type|, |url|,
  // reused |password_type|, |verdict| and |receive_time|.
  virtual void CacheVerdict(
      const GURL& url,
      LoginReputationClientRequest::TriggerType trigger_type,
      ReusedPasswordType password_type,
      LoginReputationClientResponse* verdict,
      const base::Time& receive_time);

  // Removes all the expired verdicts from cache.
  void CleanUpExpiredVerdicts();

  // Creates an instance of PasswordProtectionRequest and call Start() on that
  // instance. This function also insert this request object in |requests_| for
  // record keeping.
  void StartRequest(content::WebContents* web_contents,
                    const GURL& main_frame_url,
                    const GURL& password_form_action,
                    const GURL& password_form_frame_url,
                    ReusedPasswordType reused_password_type,
                    const std::vector<std::string>& matching_domains,
                    LoginReputationClientRequest::TriggerType trigger_type,
                    bool password_field_exists);

  virtual void MaybeStartPasswordFieldOnFocusRequest(
      content::WebContents* web_contents,
      const GURL& main_frame_url,
      const GURL& password_form_action,
      const GURL& password_form_frame_url);

  virtual void MaybeStartProtectedPasswordEntryRequest(
      content::WebContents* web_contents,
      const GURL& main_frame_url,
      ReusedPasswordType reused_password_type,
      const std::vector<std::string>& matching_domains,
      bool password_field_exists);

  // Records a Chrome Sync event that sync password reuse was detected.
  virtual void MaybeLogPasswordReuseDetectedEvent(
      content::WebContents* web_contents) = 0;

  scoped_refptr<SafeBrowsingDatabaseManager> database_manager();

  // Safe Browsing backend cannot get a reliable reputation of a URL if
  // (1) URL is not valid
  // (2) URL doesn't have http or https scheme
  // (3) It maps to a local host.
  // (4) Its hostname is an IP Address in an IANA-reserved range.
  // (5) Its hostname is a not-yet-assigned by ICANN gTLD.
  // (6) Its hostname is a dotless domain.
  static bool CanGetReputationOfURL(const GURL& url);

  // If we want to show password reuse modal warning.
  bool ShouldShowModalWarning(
      LoginReputationClientRequest::TriggerType trigger_type,
      ReusedPasswordType reused_password_type,
      LoginReputationClientResponse::VerdictType verdict_type);

  // Shows modal warning dialog on the current |web_contents| and pass the
  // |verdict_token| to callback of this dialog.
  virtual void ShowModalWarning(content::WebContents* web_contents,
                                const std::string& verdict_token,
                                ReusedPasswordType reused_password_type) = 0;

  // Shows chrome://reset-password interstitial.
  virtual void ShowInterstitial(content::WebContents* web_contens,
                                ReusedPasswordType password_type) = 0;

  virtual void UpdateSecurityState(safe_browsing::SBThreatType threat_type,
                                   ReusedPasswordType password_type,
                                   content::WebContents* web_contents) = 0;

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
  bool IsWarningEnabled();

  // Returns if the event logging is enabled.
  bool IsEventLoggingEnabled();

  // Returns the pref value of password protection warning trigger.
  virtual PasswordProtectionTrigger GetPasswordProtectionWarningTriggerPref()
      const = 0;

  // If |url| matches Safe Browsing whitelist domains, password protection
  // change password URL, or password protection login URLs in the enterprise
  // policy.
  virtual bool IsURLWhitelistedForPasswordEntry(
      const GURL& url,
      RequestOutcome* reason) const = 0;

  // Called when password reuse warning or phishing reuse warning is shown.
  // Must be called on UI thread.
  virtual void OnPolicySpecifiedPasswordReuseDetected(const GURL& url,
                                                      bool is_phishing_url) = 0;

  // Called when a protected password change is detected. Must be called on
  // UI thread.
  virtual void OnPolicySpecifiedPasswordChanged() = 0;

  // Converts from password::metrics_util::PasswordType to
  // LoginReputationClientRequest::PasswordReuseEvent::ReusedPasswordType.
  static ReusedPasswordType GetPasswordProtectionReusedPasswordType(
      password_manager::metrics_util::PasswordType password_type);

  // If we can send ping for this type of reused password.
  bool IsSupportedPasswordTypeForPinging(
      ReusedPasswordType reused_password_type) const;

  // If we can show modal warning for this type of reused password.
  bool IsSupportedPasswordTypeForModalWarning(
      ReusedPasswordType reused_password_type) const;

 protected:
  friend class PasswordProtectionRequest;

  // Chrome can send password protection ping if it is allowed by for the
  // |trigger_type| and if Safe Browsing can compute reputation of
  // |main_frame_url| (e.g. Safe Browsing is not able to compute reputation of a
  // private IP or a local host). Update |reason| if sending ping is not
  // allowed. |password_type| is used for UMA metric recording.
  bool CanSendPing(LoginReputationClientRequest::TriggerType trigger_type,
                   const GURL& main_frame_url,
                   ReusedPasswordType password_type,
                   RequestOutcome* reason);

  // Called by a PasswordProtectionRequest instance when it finishes to remove
  // itself from |requests_|.
  virtual void RequestFinished(
      PasswordProtectionRequest* request,
      bool already_cached,
      std::unique_ptr<LoginReputationClientResponse> response);

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

  virtual bool IsIncognito() = 0;

  virtual bool IsPingingEnabled(
      LoginReputationClientRequest::TriggerType trigger_type,
      ReusedPasswordType password_type,
      RequestOutcome* reason) = 0;

  virtual bool IsHistorySyncEnabled() = 0;

  virtual bool IsUnderAdvancedProtection() = 0;

  // Gets the type of sync account associated with current profile or
  // |NOT_SIGNED_IN|.
  virtual LoginReputationClientRequest::PasswordReuseEvent::SyncAccountType
  GetSyncAccountType() const = 0;

  // Records a Chrome Sync event for the result of the URL reputation lookup
  // if the user enters their sync password on a website.
  virtual void MaybeLogPasswordReuseLookupEvent(
      content::WebContents* web_contents,
      RequestOutcome,
      const LoginReputationClientResponse*) = 0;

  void CheckCsdWhitelistOnIOThread(const GURL& url, bool* check_result);

  HostContentSettingsMap* content_settings() const { return content_settings_; }

  void RemoveWarningRequestsByWebContents(content::WebContents* web_contents);

  bool IsModalWarningShowingInWebContents(content::WebContents* web_contents);

  virtual bool CanShowInterstitial(RequestOutcome reason,
                                   ReusedPasswordType password_type,
                                   const GURL& main_frame_url) = 0;

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

  // Posted to UI thread by OnURLsDeleted(..). This function cleans up password
  // protection content settings related to deleted URLs.
  void RemoveContentSettingsOnURLsDeleted(bool all_history,
                                          const history::URLRows& deleted_rows);

  // Posted to UI thread by OnURLsDeleted(...). This function remove the related
  // entries in kSafeBrowsingUnhandledSyncPasswordReuses.
  virtual void RemoveUnhandledSyncPasswordReuseOnURLsDeleted(
      bool all_history,
      const history::URLRows& deleted_rows) = 0;

  // Helper function called by RemoveContentSettingsOnURLsDeleted(..). It
  // calculate the number of verdicts of |type| that associate with |url|.
  int GetVerdictCountForURL(const GURL& url,
                            LoginReputationClientRequest::TriggerType type);

  // Remove verdict of |type| from |cache_dictionary|. Return false if no
  // verdict removed, true otherwise.
  bool RemoveExpiredVerdicts(LoginReputationClientRequest::TriggerType type,
                             base::DictionaryValue* cache_dictionary);

  // Helper function called by RemoveExpiredVerdicts(..). Returns the number of
  // expired entries removed.
  size_t RemoveExpiredEntries(base::Value* verdict_dictionary);

  static bool ParseVerdictEntry(base::Value* verdict_entry,
                                int* out_verdict_received_time,
                                LoginReputationClientResponse* out_verdict);

  static bool PathVariantsMatchCacheExpression(
      const std::vector<std::string>& generated_paths,
      const std::string& cache_expression_path);

  static bool IsCacheExpired(int cache_creation_time, int cache_duration);

  static void GeneratePathVariantsWithoutQuery(const GURL& url,
                                               std::vector<std::string>* paths);

  static std::string GetCacheExpressionPath(
      const std::string& cache_expression);

  static std::unique_ptr<base::DictionaryValue> CreateDictionaryFromVerdict(
      const LoginReputationClientResponse* verdict,
      const base::Time& receive_time);

  void RecordNoPingingReason(
      LoginReputationClientRequest::TriggerType trigger_type,
      RequestOutcome reason,
      ReusedPasswordType password_type);

  // Get the content area size of current browsing window.
  virtual gfx::Size GetCurrentContentAreaSize() const = 0;

  // Number of verdict stored for this profile for password on focus pings.
  int stored_verdict_count_password_on_focus_;

  // Number of verdict stored for this profile for protected password entry
  // pings.
  int stored_verdict_count_password_entry_;

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

  ScopedObserver<history::HistoryService, history::HistoryServiceObserver>
      history_service_observer_;

  // Content settings map associated with this instance.
  HostContentSettingsMap* content_settings_;

  // Weakptr can only cancel task if it is posted to the same thread. Therefore,
  // we need CancelableTaskTracker to cancel tasks posted to IO thread.
  base::CancelableTaskTracker tracker_;

  base::WeakPtrFactory<PasswordProtectionService> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(PasswordProtectionService);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_PASSWORD_PROTECTION_PASSWORD_PROTECTION_SERVICE_H_
