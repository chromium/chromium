// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_PASSWORD_PROTECTION_MOCK_PASSWORD_PROTECTION_SERVICE_H_
#define COMPONENTS_SAFE_BROWSING_PASSWORD_PROTECTION_MOCK_PASSWORD_PROTECTION_SERVICE_H_

#include "base/macros.h"
#include "components/safe_browsing/password_protection/password_protection_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace safe_browsing {

class MockPasswordProtectionService : public PasswordProtectionService {
 public:
  MockPasswordProtectionService();
  MockPasswordProtectionService(
      const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      history::HistoryService* history_service,
      scoped_refptr<HostContentSettingsMap> content_setting_map);
  ~MockPasswordProtectionService() override;

  // safe_browsing::PasswordProtectionService
  MOCK_CONST_METHOD0(GetSyncAccountType,
                     safe_browsing::LoginReputationClientRequest::
                         PasswordReuseEvent::SyncAccountType());
  MOCK_CONST_METHOD0(GetBrowserPolicyConnector,
                     const policy::BrowserPolicyConnector*());
  MOCK_CONST_METHOD0(GetPasswordProtectionWarningTriggerPref,
                     PasswordProtectionTrigger());
  MOCK_CONST_METHOD0(GetCurrentContentAreaSize, gfx::Size());
  MOCK_CONST_METHOD2(IsURLWhitelistedForPasswordEntry,
                     bool(const GURL&, RequestOutcome*));

  MOCK_METHOD0(IsExtendedReporting, bool());
  MOCK_METHOD0(IsIncognito, bool());
  MOCK_METHOD0(IsHistorySyncEnabled, bool());
  MOCK_METHOD0(IsUnderAdvancedProtection, bool());
  MOCK_METHOD0(OnPolicySpecifiedPasswordChanged, void());
  MOCK_METHOD1(MaybeLogPasswordReuseDetectedEvent, void(content::WebContents*));
  MOCK_METHOD1(UserClickedThroughSBInterstitial, bool(content::WebContents*));
  MOCK_METHOD2(ShowInterstitial,
               void(content::WebContents*, ReusedPasswordType));
  MOCK_METHOD3(IsPingingEnabled,
               bool(LoginReputationClientRequest::TriggerType,
                    ReusedPasswordType,
                    RequestOutcome*));
  MOCK_METHOD3(ShowModalWarning,
               void(content::WebContents*,
                    const std::string&,
                    ReusedPasswordType));
  MOCK_METHOD3(UpdateSecurityState,
               void(safe_browsing::SBThreatType,
                    ReusedPasswordType,
                    content::WebContents*));
  MOCK_METHOD2(RemoveUnhandledSyncPasswordReuseOnURLsDeleted,
               void(bool, const history::URLRows&));
  MOCK_METHOD2(OnPolicySpecifiedPasswordReuseDetected, void(const GURL&, bool));
  MOCK_METHOD3(FillReferrerChain,
               void(const GURL&,
                    SessionID,
                    LoginReputationClientRequest::Frame*));
  MOCK_METHOD3(MaybeLogPasswordReuseLookupEvent,
               void(content::WebContents*,
                    RequestOutcome,
                    const safe_browsing::LoginReputationClientResponse*));
  MOCK_METHOD3(CanShowInterstitial,
               bool(RequestOutcome, ReusedPasswordType, const GURL&));
  MOCK_METHOD4(
      MaybeStartPasswordFieldOnFocusRequest,
      void(content::WebContents*, const GURL&, const GURL&, const GURL&));
  MOCK_METHOD5(MaybeStartProtectedPasswordEntryRequest,
               void(content::WebContents*,
                    const GURL&,
                    ReusedPasswordType,
                    const std::vector<std::string>&,
                    bool));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPasswordProtectionService);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_PASSWORD_PROTECTION_MOCK_PASSWORD_PROTECTION_SERVICE_H_
