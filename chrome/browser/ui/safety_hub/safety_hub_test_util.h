// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_SAFETY_HUB_TEST_UTIL_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_SAFETY_HUB_TEST_UTIL_H_

#include <memory>
#include <string_view>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/safety_hub/safety_hub_service.h"
#include "chrome/browser/ui/safety_hub/unused_site_permissions_service.h"
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check_service.h"
#include "testing/gmock/include/gmock/gmock.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/extensions/cws_info_service.h"
#include "chrome/browser/ui/safety_hub/password_status_check_service.h"
#include "chrome/browser/ui/safety_hub/safety_hub_hats_service.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace safety_hub_test_util {

#if !BUILDFLAG(IS_ANDROID)
// Mock CWS info service for extensions.
class MockCWSInfoService : public extensions::CWSInfoService {
 public:
  explicit MockCWSInfoService(Profile* profile);
  ~MockCWSInfoService() override;

  MOCK_METHOD(std::optional<CWSInfoServiceInterface::CWSInfo>,
              GetCWSInfo,
              (const extensions::Extension&),
              (const, override));
};

// This will run UpdateInsecureCredentialCountAsync on
// PasswordStatusCheckService and return when the check is completed.
void UpdatePasswordCheckServiceAsync(
    PasswordStatusCheckService* password_service);

// This will run until ongoing checks in PasswordStatusCheckService to be
// completed.
void RunUntilPasswordCheckCompleted(Profile* profile);

// Creates a mock service that returns mock results for the CWS info service. If
// |with_calls| is true, total six extensions with different properties are
// mocked: malware, policy violation, unpublished, combination of malware and
// unpublished, no violation, and an extension that is not present.
std::unique_ptr<testing::NiceMock<MockCWSInfoService>> GetMockCWSInfoService(
    Profile* profile,
    bool with_calls = true);

// Creates a mock service that returns mock results for the CWS info service.
// Each call will respond with a extension having no triggers.
std::unique_ptr<testing::NiceMock<MockCWSInfoService>>
GetMockCWSInfoServiceNoTriggers(Profile* profile);

// Adds a testing extension with |name| and |location| to |profile|.
void AddExtension(
    const std::string& name,
    extensions::mojom::ManifestLocation location,
    Profile* profile,
    std::string update_url = extension_urls::kChromeWebstoreUpdateURL);

// Adds seven extensions, of which one is installed by an external policy.
void CreateMockExtensions(TestingProfile* profile);

// Deletes all mock extensions that are added by CreateMockExtensions.
void CleanAllMockExtensions(Profile* profile);

// Creates and returns a CWSInfo without any triggers.
extensions::CWSInfoService::CWSInfo GetCWSInfoNoTrigger();

// Removes an extension from the Chrome registry and extension prefs.
void RemoveExtension(const std::string& name,
                     extensions::mojom::ManifestLocation location,
                     Profile* profile);

// Add the `ack_safety_check_warning_reason` pref to an extension. This
// will remove it from the Safety Hub.
void AcknowledgeSafetyCheckExtensions(const std::string& name,
                                      Profile* profile);

// Creates the service used for bulk leak checks.
password_manager::BulkLeakCheckService* CreateAndUseBulkLeakCheckService(
    signin::IdentityManager* identity_manager,
    Profile* profile);

// Creates a form for the password manager with a given username, password and
// origin. If |is_leaked| is set to |true|, the password will be considered a
// leaked password.
password_manager::PasswordForm MakeForm(std::u16string_view username,
                                        std::u16string_view password,
                                        std::string origin,
                                        bool is_leaked = false);
#endif  // BUILDFLAG(IS_ANDROID)

// This will run the UpdateAsync function on the provided SafetyHubService and
// return when both the background task and UI task are completed. It will
// temporary add an observer to the service, which will be removed again before
// the function returns.
void UpdateSafetyHubServiceAsync(SafetyHubService* service);

// This will run the UpdateAsync function on the UnusedSitePermissionsService
// and return when both the background task and UI task are completed. This
// separate helper is needed because abusive notification revocation is
// asynchronous, so this method should run until idle.
void UpdateUnusedSitePermissionsServiceAsync(
    UnusedSitePermissionsService* service);

// Returns true if the provided list of content settings has a setting with the
// provided url.
bool IsUrlInSettingsList(ContentSettingsForOneType content_settings, GURL url);

// Create a series of notifications on a site that has not been interacted with
// that will result in a Safety Hub menu notification being shown.
void GenerateSafetyHubMenuNotification(Profile* profile);

}  // namespace safety_hub_test_util

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_SAFETY_HUB_TEST_UTIL_H_
