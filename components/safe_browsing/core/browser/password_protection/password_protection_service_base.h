// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_PASSWORD_PROTECTION_PASSWORD_PROTECTION_SERVICE_BASE_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_PASSWORD_PROTECTION_PASSWORD_PROTECTION_SERVICE_BASE_H_

#include <set>

#include "base/feature_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/task/cancelable_task_tracker.h"
#include "build/build_config.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_reuse_detector.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/password_protection/metrics_util.h"
#include "components/safe_browsing/core/browser/referrer_chain_provider.h"
#include "components/safe_browsing/core/browser/safe_browsing_metrics_collector.h"
#include "components/safe_browsing/core/browser/safe_browsing_token_fetcher.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sessions/core/session_id.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"
#include "ui/gfx/geometry/size.h"

class GURL;

namespace safe_browsing {

class PasswordProtectionRequest;
class SafeBrowsingDatabaseManager;

using ReusedPasswordAccountType =
    LoginReputationClientRequest::PasswordReuseEvent::ReusedPasswordAccountType;
using ReusedPasswordType =
    LoginReputationClientRequest::PasswordReuseEvent::ReusedPasswordType;
using password_manager::metrics_util::PasswordType;

// Manage password protection pings and verdicts. There is one instance of this
// class per profile. Therefore, every PasswordProtectionServiceBase instance is
// associated with a unique HistoryService instance and a unique
// HostContentSettingsMap instance.
class PasswordProtectionServiceBase : public history::HistoryServiceObserver {
 public:
  // Creates an instance with various fields set. Needs pref_service to get safe
  // browsing protection level, is_off_the_record to check for incognito,
  // identity_manager to verify that the user is signed in, and token_fetcher to
  // try fetching the token. If try_token_fetch is false, the class will not
  // attempt to fetch a token, or do any of the checks associated with
  // pref_service, token_fetcher, and identity_manager.
  PasswordProtectionServiceBase(
      const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      history::HistoryService* history_service,
      PrefService* pref_service,
      std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher,
      bool is_off_the_record,
      signin::IdentityManager* identity_manager,
      bool try_token_fetch,
      SafeBrowsingMetricsCollector* metrics_collector);

  PasswordProtectionServiceBase(const PasswordProtectionServiceBase&) = delete;
  PasswordProtectionServiceBase& operator=(
      const PasswordProtectionServiceBase&) = delete;

  ~PasswordProtectionServiceBase() override;

  base::WeakPtr<PasswordProtectionServiceBase> GetWeakPtr() {
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

  // If we want to show password reuse modal warning.
  bool ShouldShowModalWarning(
      LoginReputationClientRequest::TriggerType trigger_type,
      ReusedPasswordAccountType password_type,
      LoginReputationClientResponse::VerdictType verdict_type);

  // Shows modal warning dialog on the current |web_contents| and pass the
  // |verdict_token| to callback of this dialog.
  virtual void ShowModalWarning(
      PasswordProtectionRequest* request,
      LoginReputationClientResponse::VerdictType verdict_type,
      const std::string& verdict_token,
      ReusedPasswordAccountType password_type) = 0;

// The following functions are disabled on Android, because enterprise reporting
// extension is not supported.
#if !BUILDFLAG(IS_ANDROID)
  // Triggers the safeBrowsingPrivate.OnPolicySpecifiedPasswordReuseDetected.
  virtual void MaybeReportPasswordReuseDetected(const GURL& main_frame_url,
                                                const std::string& username,
                                                PasswordType password_type,
                                                bool is_phishing_url,
                                                bool warning_shown) = 0;

  // Called when a protected password change is detected. Must be called on
  // UI thread.
  virtual void ReportPasswordChanged() = 0;
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

  // If user has clicked through any Safe Browsing interstitial for |request|'s
  // web contents.
  virtual bool UserClickedThroughSBInterstitial(
      PasswordProtectionRequest* request) = 0;

  // Returns if the warning UI is enabled.
  bool IsWarningEnabled(ReusedPasswordAccountType password_type);

  // Returns the pref value of password protection warning trigger.
  virtual PasswordProtectionTrigger GetPasswordProtectionWarningTriggerPref(
      ReusedPasswordAccountType password_type) const = 0;

  // If |url| matches Safe Browsing allowlist domains, password protection
  // change password URL, or password protection login URLs in the enterprise
  // policy.
  virtual bool IsURLAllowlistedForPasswordEntry(const GURL& url) const = 0;

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

  // Returns the URL where PasswordProtectionRequest instances send requests.
  static GURL GetPasswordProtectionRequestUrl();

  // Gets the UserPopulation value for this profile.
  virtual ChromeUserPopulation::UserPopulation GetUserPopulationPref()
      const = 0;

  std::set<scoped_refptr<PasswordProtectionRequest>>&
  get_pending_requests_for_testing() {
    return pending_requests_;
  }

 protected:
  friend class PasswordProtectionRequest;
  friend class PasswordProtectionRequestContent;

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

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory() {
    return url_loader_factory_;
  }

  // Gets the request timeout in milliseconds.
  static int GetRequestTimeoutInMS();

  // Obtains referrer chain of |event_url| and |event_tab_id| and adds this
  // info into |frame|.
  virtual void FillReferrerChain(
      const GURL& event_url,
      SessionID
          event_tab_id,  // SessionID::InvalidValue() if tab not available.
      LoginReputationClientRequest::Frame* frame) = 0;

  virtual void FillUserPopulation(
      const GURL& main_frame_url,
      LoginReputationClientRequest* request_proto) = 0;

  virtual bool IsExtendedReporting() = 0;

  virtual bool IsIncognito() = 0;

  virtual bool IsInPasswordAlertMode(
      ReusedPasswordAccountType password_type) = 0;

  virtual bool IsPingingEnabled(
      LoginReputationClientRequest::TriggerType trigger_type,
      ReusedPasswordAccountType password_type) = 0;

  // If primary account is syncing history.
  virtual bool IsPrimaryAccountSyncingHistory() const = 0;

  // If primary account is signed in.
  virtual bool IsPrimaryAccountSignedIn() const = 0;

  // If the domain for the account is equal to |kNoHostedDomainFound|,
  // this means that the account is a Gmail account.
  virtual bool IsAccountGmail(const std::string& username) const = 0;

  // Gets the account based off of the username from a list of signed in
  // accounts.
  virtual AccountInfo GetAccountInfoForUsername(
      const std::string& username) const = 0;

  // If Safe browsing endpoint is not enabled in the country.
  virtual bool IsInExcludedCountry() = 0;

  // Determines if we should show chrome://reset-password interstitial based on
  // the reused |password_type| and the |main_frame_url|.
  virtual bool CanShowInterstitial(ReusedPasswordAccountType password_type,
                                   const GURL& main_frame_url) = 0;

  void CheckCsdAllowlistOnIOThread(const GURL& url, bool* check_result);

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

  // Subclasses may override this method to resume deferred navigations when a
  // request finishes. By default, deferred navigations are not handled.
  virtual void ResumeDeferredNavigationsIfNeeded(
      PasswordProtectionRequest* request) {}

  bool CanGetAccessToken();

  SafeBrowsingTokenFetcher* token_fetcher() { return token_fetcher_.get(); }

  // Set of pending PasswordProtectionRequests that are still waiting for
  // verdict.
  std::set<scoped_refptr<PasswordProtectionRequest>> pending_requests_;

  // Set of PasswordProtectionRequests that are triggering modal warnings.
  std::set<scoped_refptr<PasswordProtectionRequest>> warning_requests_;

  // The username of the account which password has been reused on. It is only
  // set once a modal warning or interstitial is verified to be shown.
  std::string username_for_last_shown_warning_ = "";

  // The last ReusedPasswordAccountType that was shown a warning or
  // interstitial.
  ReusedPasswordAccountType
      reused_password_account_type_for_last_shown_warning_;

  std::vector<password_manager::MatchingReusedCredential>
      saved_passwords_matching_reused_credentials_;

 private:
  friend class PasswordProtectionServiceTest;
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
  FRIEND_TEST_ALL_PREFIXES(PasswordProtectionServiceTest,
                           NoSendPingPrivateIpHostname);

  // Overridden from history::HistoryServiceObserver.
  void OnHistoryDeletions(history::HistoryService* history_service,
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

  SEQUENCE_CHECKER(sequence_checker_);

  std::vector<std::string> saved_passwords_matching_domains_;

  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;

  // The context we use to issue network requests. This request_context_getter
  // is obtained from SafeBrowsingService so that we can use the Safe Browsing
  // cookie store.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_service_observation_{this};

  // Weakptr can only cancel task if it is posted to the same thread. Therefore,
  // we need CancelableTaskTracker to cancel tasks posted to IO thread.
  base::CancelableTaskTracker tracker_;

  // Unowned object used for getting preference settings.
  raw_ptr<PrefService> pref_service_;

  // The token fetcher used for getting access token.
  std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher_;

  // A boolean indicates whether the profile associated is an
  // incognito profile.
  bool is_off_the_record_;

  // Use identity manager to check if account is signed in, before fetching
  // access token.
  raw_ptr<signin::IdentityManager> identity_manager_;

  // A boolean indicates whether access token fetch should be attempted or not.
  // Use this to disable token fetches from ios and certain tests.
  bool try_token_fetch_;

  // Unowned object used for recording metrics/prefs.
  raw_ptr<SafeBrowsingMetricsCollector> metrics_collector_;

  base::WeakPtrFactory<PasswordProtectionServiceBase> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_PASSWORD_PROTECTION_PASSWORD_PROTECTION_SERVICE_BASE_H_
