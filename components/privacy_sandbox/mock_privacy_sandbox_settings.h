// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_MOCK_PRIVACY_SANDBOX_SETTINGS_H_
#define COMPONENTS_PRIVACY_SANDBOX_MOCK_PRIVACY_SANDBOX_SETTINGS_H_

#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace privacy_sandbox_test_util {

class MockPrivacySandboxSettings
    : public privacy_sandbox::PrivacySandboxSettings {
 public:
  MockPrivacySandboxSettings();
  ~MockPrivacySandboxSettings() override;
  void SetUpDefaultResponse();

  // PrivacySandboxSettings:
  MOCK_METHOD(bool, IsTopicsAllowed, (), (override, const));
  MOCK_METHOD(bool,
              IsTopicsAllowedForContext,
              (const url::Origin&, const GURL&),
              (override, const));
  MOCK_METHOD(bool,
              IsTopicAllowed,
              (const privacy_sandbox::CanonicalTopic&),
              (override));
  MOCK_METHOD(void,
              SetTopicAllowed,
              (const privacy_sandbox::CanonicalTopic&, bool),
              (override));
  MOCK_METHOD(void, ClearTopicSettings, (base::Time, base::Time), (override));
  MOCK_METHOD(base::Time, TopicsDataAccessibleSince, (), (override, const));
  MOCK_METHOD(bool, IsAttributionReportingEverAllowed, (), (override, const));
  MOCK_METHOD(bool,
              IsAttributionReportingAllowed,
              (const url::Origin&, const url::Origin&),
              (override, const));
  MOCK_METHOD(bool,
              MaySendAttributionReport,
              (const url::Origin&, const url::Origin&, const url::Origin&),
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
              IsFledgeJoiningAllowed,
              (const url::Origin&),
              (override, const));
  MOCK_METHOD(bool,
              IsFledgeAllowed,
              (const url::Origin&, const url::Origin&),
              (override, const));
  MOCK_METHOD(bool,
              IsSharedStorageAllowed,
              (const url::Origin&, const url::Origin&),
              (override, const));
  MOCK_METHOD(bool,
              IsSharedStorageSelectURLAllowed,
              (const url::Origin&, const url::Origin&),
              (override, const));
  MOCK_METHOD(bool,
              IsPrivateAggregationAllowed,
              (const url::Origin&, const url::Origin&),
              (override, const));
  MOCK_METHOD(bool, IsPrivacySandboxEnabled, (), (override, const));
  MOCK_METHOD(void, SetAllPrivacySandboxAllowedForTesting, (), (override));
  MOCK_METHOD(void, SetTopicsBlockedForTesting, (), (override));
  MOCK_METHOD(void, SetPrivacySandboxEnabled, (bool), (override));
  MOCK_METHOD(bool, IsTrustTokensAllowed, (), (override));
  MOCK_METHOD(bool, IsPrivacySandboxRestricted, (), (override, const));
  MOCK_METHOD(void, OnCookiesCleared, (), (override));
  MOCK_METHOD(void, AddObserver, (Observer*), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer*), (override));
  MOCK_METHOD(void,
              SetDelegateForTesting,
              (std::unique_ptr<Delegate>),
              (override));
};

}  // namespace privacy_sandbox_test_util

#endif  // COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_TEST_UTIL_H_
