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

  // PrivacySandboxSettings:
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
  MOCK_METHOD(void, AddObserver, (Observer*), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer*), (override));
  MOCK_METHOD(bool, AreRelatedWebsiteSetsEnabled, (), (override, const));
};

}  // namespace privacy_sandbox_test_util

#endif  // COMPONENTS_PRIVACY_SANDBOX_MOCK_PRIVACY_SANDBOX_SETTINGS_H_
