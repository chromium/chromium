// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_SAFETY_HUB_TEST_UTIL_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_SAFETY_HUB_TEST_UTIL_H_

#include "chrome/browser/extensions/cws_info_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/safety_hub/password_status_check_service.h"
#include "chrome/browser/ui/safety_hub/safety_hub_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace safety_hub_test_util {

// Mock CWS info service for extensions.
class MockCWSInfoService : public extensions::CWSInfoService {
 public:
  explicit MockCWSInfoService(Profile* profile);
  ~MockCWSInfoService() override;

  MOCK_METHOD(absl::optional<CWSInfoServiceInterface::CWSInfo>,
              GetCWSInfo,
              (const extensions::Extension&),
              (const, override));
};

// This will run the UpdateAsync function on the provided SafetyHubService and
// return when both the background task and UI task are completed. It will
// temporary add an observer to the service, which will be removed again before
// the function returns.
void UpdateSafetyHubServiceAsync(SafetyHubService* service);

// This will run UpdateInsecureCredentialCountAsync on
// PasswordStatusCheckService and return when the check is completed.
void UpdatePasswordCheckServiceAsync(
    PasswordStatusCheckService* password_service);

// Creates a mock service that returns mock results for the CWS info service. In
// total six extensions with different properties are mocked: malware, policy
// violation, unpublished, combination of malware and unpublished, no violation,
// and an extension that is not present.
std::unique_ptr<testing::NiceMock<MockCWSInfoService>> GetMockCWSInfoService(
    Profile* profile);

// Adds seven extensions, of which one is installed by an external policy.
void CreateMockExtensions(Profile* profile);

// Deletes all mock extensions that are added by CreateMockExtensions.
void CleanAllMockExtensions(Profile* profile);

}  // namespace safety_hub_test_util

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_SAFETY_HUB_TEST_UTIL_H_
