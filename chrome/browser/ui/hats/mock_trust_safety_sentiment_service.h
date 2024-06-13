// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_HATS_MOCK_TRUST_SAFETY_SENTIMENT_SERVICE_H_
#define CHROME_BROWSER_UI_HATS_MOCK_TRUST_SAFETY_SENTIMENT_SERVICE_H_

#include <memory>

#include "chrome/browser/ui/hats/trust_safety_sentiment_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {
class BrowserContext;
}

class KeyedService;
class Profile;

class MockTrustSafetySentimentService : public TrustSafetySentimentService {
 public:
  explicit MockTrustSafetySentimentService(Profile* profile);
  ~MockTrustSafetySentimentService() override;

  MOCK_METHOD(void, OpenedNewTabPage, (), (override));
  MOCK_METHOD(void,
              InteractedWithPrivacySettings,
              (content::WebContents * web_contents),
              (override));
  MOCK_METHOD(void, RanSafetyCheck, (), (override));
  MOCK_METHOD(void, PageInfoOpened, (), (override));
  MOCK_METHOD(void, InteractedWithPageInfo, (), (override));
  MOCK_METHOD(void, PageInfoClosed, (), (override));
  MOCK_METHOD(void, SavedPassword, (), (override));
  MOCK_METHOD(void,
              OpenedPasswordManager,
              (content::WebContents * web_contents),
              (override));
  MOCK_METHOD(void, SavedCard, (), (override));
  MOCK_METHOD(void,
              InteractedWithPrivacySandbox4,
              (FeatureArea feature_area),
              (override));
  MOCK_METHOD(void, RanPasswordCheck, (), (override));
  MOCK_METHOD(void,
              ClearedBrowsingData,
              (browsing_data::BrowsingDataType datatype),
              (override));
  MOCK_METHOD(void, FinishedPrivacyGuide, (), (override));
  MOCK_METHOD(void,
              InteractedWithSafeBrowsingInterstitial,
              (bool, safe_browsing::SBThreatType),
              (override));
  MOCK_METHOD(void,
              InteractedWithDownloadWarningUI,
              (DownloadItemWarningData::WarningSurface,
               DownloadItemWarningData::WarningAction),
              (override));
  MOCK_METHOD(void,
              ProtectResetOrCheckPasswordClicked,
              (PasswordProtectionUIType),
              (override));
  MOCK_METHOD(void,
              PhishedPasswordUpdateNotClicked,
              (PasswordProtectionUIType, PasswordProtectionUIAction),
              (override));
  MOCK_METHOD(void, PhishedPasswordUpdateFinished, (), (override));
  MOCK_METHOD(void,
              TriggerSafetyHubSurvey,
              (TrustSafetySentimentService::FeatureArea,
               (std::map<std::string, bool>)),
              (override));
};

std::unique_ptr<KeyedService> BuildMockTrustSafetySentimentService(
    content::BrowserContext* context);

#endif  // CHROME_BROWSER_UI_HATS_MOCK_TRUST_SAFETY_SENTIMENT_SERVICE_H_
