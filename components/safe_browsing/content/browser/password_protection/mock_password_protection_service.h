// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_PASSWORD_PROTECTION_MOCK_PASSWORD_PROTECTION_SERVICE_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_PASSWORD_PROTECTION_MOCK_PASSWORD_PROTECTION_SERVICE_H_

#include "components/safe_browsing/content/browser/password_protection/password_protection_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace safe_browsing {

class MockPasswordProtectionService : public PasswordProtectionService {
 public:
  MockPasswordProtectionService();
  MockPasswordProtectionService(
      const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      history::HistoryService* history_service,
      PrefService* pref_service,
      std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher,
      bool is_off_the_record,
      signin::IdentityManager* identity_manager,
      bool try_token_fetch,
      SafeBrowsingMetricsCollector* metrics_collector);

  MockPasswordProtectionService(const MockPasswordProtectionService&) = delete;
  MockPasswordProtectionService& operator=(
      const MockPasswordProtectionService&) = delete;

  ~MockPasswordProtectionService() override;

  // safe_browsing::PasswordProtectionService
  MOCK_CONST_METHOD0(GetSyncAccountType,
                     safe_browsing::LoginReputationClientRequest::
                         PasswordReuseEvent::SyncAccountType());
  MOCK_CONST_METHOD0(
      GetUrlDisplayExperiment,
      safe_browsing::LoginReputationClientRequest::UrlDisplayExperiment());

  MOCK_CONST_METHOD0(GetCurrentContentAreaSize, gfx::Size());
  MOCK_CONST_METHOD0(GetAccountInfo, AccountInfo());
  MOCK_CONST_METHOD0(IsPrimaryAccountSyncingHistory, bool());
  MOCK_CONST_METHOD0(IsPrimaryAccountSignedIn, bool());
  MOCK_CONST_METHOD1(GetPasswordProtectionWarningTriggerPref,
                     PasswordProtectionTrigger(ReusedPasswordAccountType));
  MOCK_CONST_METHOD1(GetAccountInfoForUsername,
                     AccountInfo(const std::string&));
  MOCK_CONST_METHOD1(IsAccountGmail, bool(const std::string&));
  MOCK_CONST_METHOD1(IsURLAllowlistedForPasswordEntry, bool(const GURL&));

  MOCK_METHOD2(FillUserPopulation,
               void(const GURL&, LoginReputationClientRequest*));
  MOCK_METHOD0(CanSendSamplePing, bool());
  MOCK_METHOD0(IsIncognito, bool());
  MOCK_METHOD0(IsExtendedReporting, bool());
  MOCK_METHOD1(IsInPasswordAlertMode, bool(ReusedPasswordAccountType));
  MOCK_METHOD0(IsInExcludedCountry, bool());
  MOCK_METHOD0(ReportPasswordChanged, void());
  MOCK_METHOD1(UserClickedThroughSBInterstitial,
               bool(PasswordProtectionRequest*));
  MOCK_METHOD1(MaybeLogPasswordReuseDetectedEvent, void(content::WebContents*));
  MOCK_METHOD1(SanitizeReferrerChain, void(ReferrerChain*));
  MOCK_METHOD2(ShowInterstitial,
               void(content::WebContents*, ReusedPasswordAccountType));
  MOCK_METHOD1(
      PersistPhishedSavedPasswordCredential,
      void(const std::vector<password_manager::MatchingReusedCredential>&));
  MOCK_METHOD1(
      RemovePhishedSavedPasswordCredential,
      void(const std::vector<password_manager::MatchingReusedCredential>&));
  MOCK_METHOD1(
      GetReferringAppInfo,
      LoginReputationClientRequest::ReferringAppInfo(content::WebContents*));
  MOCK_METHOD2(IsPingingEnabled,
               bool(LoginReputationClientRequest::TriggerType,
                    ReusedPasswordAccountType));
  MOCK_METHOD3(GetPingNotSentReason,
               RequestOutcome(LoginReputationClientRequest::TriggerType,
                              const GURL&,
                              ReusedPasswordAccountType));
  MOCK_METHOD4(ShowModalWarning,
               void(PasswordProtectionRequest*,
                    LoginReputationClientResponse::VerdictType,
                    const std::string&,
                    ReusedPasswordAccountType));
  MOCK_METHOD5(MaybeReportPasswordReuseDetected,
               void(const GURL&, const std::string&, PasswordType, bool, bool));
  MOCK_METHOD3(UpdateSecurityState,
               void(safe_browsing::SBThreatType,
                    ReusedPasswordAccountType,
                    content::WebContents*));
  MOCK_METHOD2(RemoveUnhandledSyncPasswordReuseOnURLsDeleted,
               void(bool, const history::URLRows&));
  MOCK_METHOD3(FillReferrerChain,
               void(const GURL&,
                    SessionID,
                    LoginReputationClientRequest::Frame*));
  MOCK_METHOD4(MaybeLogPasswordReuseLookupEvent,
               void(content::WebContents*,
                    RequestOutcome,
                    PasswordType,
                    const safe_browsing::LoginReputationClientResponse*));
  MOCK_METHOD2(CanShowInterstitial,
               bool(ReusedPasswordAccountType, const GURL&));
  MOCK_METHOD5(MaybeStartPasswordFieldOnFocusRequest,
               void(content::WebContents*,
                    const GURL&,
                    const GURL&,
                    const GURL&,
                    const std::string&));
  MOCK_METHOD6(
      MaybeStartProtectedPasswordEntryRequest,
      void(content::WebContents*,
           const GURL&,
           const std::string&,
           PasswordType,
           const std::vector<password_manager::MatchingReusedCredential>&,
           bool));
  MOCK_CONST_METHOD0(GetUserPopulationPref,
                     ChromeUserPopulation::UserPopulation());
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_PASSWORD_PROTECTION_MOCK_PASSWORD_PROTECTION_SERVICE_H_
