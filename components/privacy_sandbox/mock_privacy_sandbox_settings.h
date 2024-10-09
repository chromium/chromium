// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_MOCK_PRIVACY_SANDBOX_SETTINGS_H_
#define COMPONENTS_PRIVACY_SANDBOX_MOCK_PRIVACY_SANDBOX_SETTINGS_H_

#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "components/privacy_sandbox/tpcd_experiment_eligibility.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace privacy_sandbox_test_util {

class MockPrivacySandboxSettings
    : public privacy_sandbox::PrivacySandboxSettings {
 public:
  MockPrivacySandboxSettings();
  ~MockPrivacySandboxSettings() override;

  // PrivacySandboxSettings:
  MOCK_METHOD(bool, IsTopicsAllowed, (), (override, const));
  MOCK_METHOD(bool,
              IsTopicsAllowedForContext,
              (const url::Origin&, const GURL&, content::RenderFrameHost*),
              (override, const));
  MOCK_METHOD(bool,
              IsTopicAllowed,
              (const privacy_sandbox::CanonicalTopic&),
              (override));
  MOCK_METHOD(void,
              SetTopicAllowed,
              (const privacy_sandbox::CanonicalTopic&, bool),
              (override));
  MOCK_METHOD(bool,
              IsTopicPrioritized,
              (const privacy_sandbox::CanonicalTopic&),
              (override));
  MOCK_METHOD(void, ClearTopicSettings, (base::Time, base::Time), (override));
  MOCK_METHOD(base::Time, TopicsDataAccessibleSince, (), (override, const));
  MOCK_METHOD(bool, IsAttributionReportingEverAllowed, (), (override, const));
  MOCK_METHOD(bool,
              IsAttributionReportingAllowed,
              (const url::Origin&,
               const url::Origin&,
               content::RenderFrameHost*),
              (override, const));
  MOCK_METHOD(bool,
              MaySendAttributionReport,
              (const url::Origin&,
               const url::Origin&,
               const url::Origin&,
               content::RenderFrameHost*),
              (override, const));
  MOCK_METHOD(bool,
              IsAttributionReportingTransitionalDebuggingAllowed,
              (const url::Origin&, const url::Origin&, bool&),
              (override, const));
  MOCK_METHOD(void,
              SetFledgeJoiningAllowed,
              (const std::string&, bool),
              (override));
  MOCK_METHOD(void,
              ClearFledgeJoiningAllowedSettings,
              (base::Time, base::Time),
              (override));
  MOCK_METHOD(bool,
              IsFledgeAllowed,
              (const url::Origin&,
               const url::Origin&,
               content::InterestGroupApiOperation,
               content::RenderFrameHost*),
              (override, const));
  MOCK_METHOD(
      bool,
      IsEventReportingDestinationAttested,
      (const url::Origin&,
       privacy_sandbox::PrivacySandboxAttestationsGatedAPI invoking_api),
      (override, const));
  MOCK_METHOD(bool,
              IsSharedStorageAllowed,
              (const url::Origin&,
               const url::Origin&,
               std::string*,
               content::RenderFrameHost*,
               bool*),
              (override, const));
  MOCK_METHOD(bool,
              IsSharedStorageSelectURLAllowed,
              (const url::Origin&, const url::Origin&, std::string*, bool*),
              (override, const));
  MOCK_METHOD(bool,
              IsLocalUnpartitionedDataAccessAllowed,
              (const url::Origin&,
               const url::Origin&,
               content::RenderFrameHost*),
              (override, const));
  MOCK_METHOD(bool,
              IsPrivateAggregationAllowed,
              (const url::Origin&, const url::Origin&, bool*),
              (override, const));
  MOCK_METHOD(bool,
              IsPrivateAggregationDebugModeAllowed,
              (const url::Origin&, const url::Origin&),
              (override, const));
  MOCK_METHOD(privacy_sandbox::TpcdExperimentEligibility,
              GetCookieDeprecationExperimentCurrentEligibility,
              (),
              (override, const));
  MOCK_METHOD(bool, IsCookieDeprecationLabelAllowed, (), (override, const));
  MOCK_METHOD(bool,
              IsCookieDeprecationLabelAllowedForContext,
              (const url::Origin&, const url::Origin&),
              (override, const));
  MOCK_METHOD(void, SetAllPrivacySandboxAllowedForTesting, (), (override));
  MOCK_METHOD(void, SetTopicsBlockedForTesting, (), (override));
  MOCK_METHOD(bool, IsPrivacySandboxRestricted, (), (override, const));
  MOCK_METHOD(bool,
              IsPrivacySandboxCurrentlyUnrestricted,
              (),
              (override, const));
  MOCK_METHOD(bool, IsSubjectToM1NoticeRestricted, (), (override, const));
  MOCK_METHOD(bool, IsRestrictedNoticeEnabled, (), (override, const));
  MOCK_METHOD(void, OnCookiesCleared, (), (override));
  MOCK_METHOD(void, AddObserver, (Observer*), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer*), (override));
  MOCK_METHOD(void,
              SetDelegateForTesting,
              (std::unique_ptr<Delegate>),
              (override));
  MOCK_METHOD(bool, AreRelatedWebsiteSetsEnabled, (), (override, const));
};

}  // namespace privacy_sandbox_test_util

#endif  // COMPONENTS_PRIVACY_SANDBOX_MOCK_PRIVACY_SANDBOX_SETTINGS_H_
