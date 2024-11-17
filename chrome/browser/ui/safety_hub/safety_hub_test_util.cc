// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/safety_hub_test_util.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/password_manager/bulk_leak_check_service_factory.h"
#include "chrome/browser/permissions/notifications_engagement_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/safety_hub/notification_permission_review_service_factory.h"
#include "components/content_settings/core/common/content_settings.h"
#include "services/network/test/test_shared_url_loader_factory.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/extensions/extension_management_test_util.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/ui/safety_hub/password_status_check_service_factory.h"
#include "components/crx_file/id_util.h"  // nogncheck crbug.com/40147906
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace {

#if !BUILDFLAG(IS_ANDROID)
using extensions::mojom::ManifestLocation;

const char kAllHostsPermission[] = "*://*/*";

// These `cws_info` variables are used to test the various states that an
// extension could be in. Is a trigger due to the malware violation.
static extensions::CWSInfoService::CWSInfo cws_info_malware{
    true,
    false,
    base::Time::Now(),
    extensions::CWSInfoService::CWSViolationType::kMalware,
    false,
    false};
// Is a trigger due to the policy violation.
static extensions::CWSInfoService::CWSInfo cws_info_policy{
    true,
    false,
    base::Time::Now(),
    extensions::CWSInfoService::CWSViolationType::kPolicy,
    false,
    false};
// Is a trigger due to being unpublished.
static extensions::CWSInfoService::CWSInfo cws_info_unpublished{
    true,
    false,
    base::Time::Now(),
    extensions::CWSInfoService::CWSViolationType::kNone,
    true,
    false};
// Is a trigger due to multiple triggers (malware and unpublished).
static extensions::CWSInfoService::CWSInfo cws_info_multi{
    true,
    false,
    base::Time::Now(),
    extensions::CWSInfoService::CWSViolationType::kMalware,
    true,
    false};
// Is not a trigger.
static extensions::CWSInfoService::CWSInfo cws_info_no_trigger{
    true,
    false,
    base::Time::Now(),
    extensions::CWSInfoService::CWSViolationType::kNone,
    false,
    false};

#endif  // BUILDFLAG(IS_ANDROID)

class TestObserver : public SafetyHubService::Observer {
 public:
  void SetCallback(const base::RepeatingClosure& callback) {
    callback_ = callback;
  }

  void OnResultAvailable(const SafetyHubService::Result* result) override {
    callback_.Run();
  }

 private:
  base::RepeatingClosure callback_;
};

}  // namespace

namespace safety_hub_test_util {

#if !BUILDFLAG(IS_ANDROID)
using password_manager::BulkLeakCheckService;

MockCWSInfoService::MockCWSInfoService(Profile* profile)
    : extensions::CWSInfoService(profile) {}
MockCWSInfoService::~MockCWSInfoService() = default;

void UpdatePasswordCheckServiceAsync(
    PasswordStatusCheckService* password_service) {
  password_service->UpdateInsecureCredentialCountAsync();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !password_service->IsUpdateRunning() &&
           !password_service->IsInfrastructureReady();
  }));
}

void RunUntilPasswordCheckCompleted(Profile* profile) {
  PasswordStatusCheckService* service =
      PasswordStatusCheckServiceFactory::GetForProfile(profile);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !service->IsUpdateRunning() && !service->IsInfrastructureReady();
  }));
}

std::unique_ptr<testing::NiceMock<MockCWSInfoService>> GetMockCWSInfoService(
    Profile* profile,
    bool with_calls) {
  // Ensure that the mock CWSInfo service returns the needed information.
  std::unique_ptr<testing::NiceMock<MockCWSInfoService>> mock_cws_info_service(
      new testing::NiceMock<MockCWSInfoService>(profile));
  if (with_calls) {
    EXPECT_CALL(*mock_cws_info_service, GetCWSInfo)
        .Times(7)
        .WillOnce(testing::Return(cws_info_malware))
        .WillOnce(testing::Return(cws_info_policy))
        .WillOnce(testing::Return(cws_info_unpublished))
        .WillOnce(testing::Return(cws_info_multi))
        .WillOnce(testing::Return(std::nullopt))
        .WillOnce(testing::Return(std::nullopt))
        .WillOnce(testing::Return(cws_info_no_trigger));
  }
  return mock_cws_info_service;
}

std::unique_ptr<testing::NiceMock<MockCWSInfoService>>
GetMockCWSInfoServiceNoTriggers(Profile* profile) {
  // Ensure that the mock CWSInfo service returns the needed information.
  std::unique_ptr<testing::NiceMock<MockCWSInfoService>> mock_cws_info_service(
      new testing::NiceMock<MockCWSInfoService>(profile));
  ON_CALL(*mock_cws_info_service, GetCWSInfo)
      .WillByDefault(testing::Return(cws_info_no_trigger));
  return mock_cws_info_service;
}

void AddExtension(const std::string& name,
                  extensions::mojom::ManifestLocation location,
                  Profile* profile,
                  std::string update_url) {
  const std::string kId = crx_file::id_util::GenerateId(name);
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder(name)
          .SetManifestKey("host_permissions",
                          base::Value::List().Append(kAllHostsPermission))
          .SetManifestKey(extensions::manifest_keys::kUpdateURL, update_url)
          .SetLocation(location)
          .SetID(kId)
          .Build();
  extensions::ExtensionPrefs::Get(profile)->OnExtensionInstalled(
      extension.get(), extensions::Extension::State::ENABLED,
      syncer::StringOrdinal(), "");
  extensions::ExtensionRegistry::Get(profile)->AddEnabled(extension);
}

void CreateMockExtensions(TestingProfile* profile) {
  AddExtension("TestExtension1", ManifestLocation::kInternal, profile);
  AddExtension("TestExtension2", ManifestLocation::kInternal, profile);
  AddExtension("TestExtension3", ManifestLocation::kInternal, profile);
  AddExtension("TestExtension4", ManifestLocation::kInternal, profile);
  AddExtension("TestExtension5", ManifestLocation::kInternal, profile);
  AddExtension("TestExtension6", ManifestLocation::kInternal, profile);
  AddExtension("TestExtension7", ManifestLocation::kInternal, profile,
               "https://example.com");
  // Extensions installed by policies will be ignored by Safety Hub. So
  // extension 8 will not trigger the handler.
  AddExtension("TestExtension8", ManifestLocation::kExternalPolicyDownload,
               profile);
  using PolicyUpdater = extensions::ExtensionManagementPrefUpdater<
      sync_preferences::TestingPrefServiceSyncable>;
  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile->GetTestingPrefService();
  PolicyUpdater(prefs).SetIndividualExtensionAutoInstalled(
      crx_file::id_util::GenerateId("TestExtension8"),
      extension_urls::kChromeWebstoreUpdateURL, true);
}

void CleanAllMockExtensions(Profile* profile) {
  RemoveExtension("TestExtension1", ManifestLocation::kInternal, profile);
  RemoveExtension("TestExtension2", ManifestLocation::kInternal, profile);
  RemoveExtension("TestExtension3", ManifestLocation::kInternal, profile);
  RemoveExtension("TestExtension4", ManifestLocation::kInternal, profile);
  RemoveExtension("TestExtension5", ManifestLocation::kInternal, profile);
  RemoveExtension("TestExtension6", ManifestLocation::kInternal, profile);
  RemoveExtension("TestExtension7", ManifestLocation::kInternal, profile);
  RemoveExtension("TestExtension8", ManifestLocation::kExternalPolicyDownload,
                  profile);

  // Check that all extensions were successfully uninstalled.
  const extensions::ExtensionSet& extensions =
      extensions::ExtensionRegistry::Get(profile)->enabled_extensions();
  EXPECT_TRUE(extensions.empty());
}

extensions::CWSInfoService::CWSInfo GetCWSInfoNoTrigger() {
  return extensions::CWSInfoService::CWSInfo{
      true,
      false,
      base::Time::Now(),
      extensions::CWSInfoService::CWSViolationType::kNone,
      false,
      false};
}

void RemoveExtension(const std::string& name,
                     extensions::mojom::ManifestLocation location,
                     Profile* profile) {
  const std::string kId = crx_file::id_util::GenerateId(name);
  extensions::ExtensionPrefs::Get(profile)->OnExtensionUninstalled(
      kId, location, false);
  extensions::ExtensionRegistry::Get(profile)->RemoveEnabled(kId);
}

void AcknowledgeSafetyCheckExtensions(const std::string& name,
                                      Profile* profile) {
  extensions::ExtensionPrefs::Get(profile)->UpdateExtensionPref(
      name, "ack_safety_check_warning_reason",
      /* Malware Acknowledged */ base::Value(3));
}

BulkLeakCheckService* CreateAndUseBulkLeakCheckService(
    signin::IdentityManager* identity_manager,
    Profile* profile) {
  return static_cast<BulkLeakCheckService*>(
      BulkLeakCheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile, base::BindLambdaForTesting([identity_manager](
                                                  content::BrowserContext*) {
            return std::unique_ptr<
                KeyedService>(std::make_unique<BulkLeakCheckService>(
                identity_manager,
                base::MakeRefCounted<network::TestSharedURLLoaderFactory>()));
          })));
}

password_manager::PasswordForm MakeForm(std::u16string_view username,
                                        std::u16string_view password,
                                        const std::string origin,
                                        bool is_leaked) {
  password_manager::PasswordForm form;
  form.username_value = username;
  form.password_value = password;
  form.signon_realm = origin;
  form.url = GURL(origin);

  if (is_leaked) {
    // Credential issues for weak and reused are detected automatically and
    // don't need to be specified explicitly.
    form.password_issues.insert_or_assign(
        password_manager::InsecureType::kLeaked,
        password_manager::InsecurityMetadata(
            base::Time::Now(), password_manager::IsMuted(false),
            password_manager::TriggerBackendNotification(false)));
  }
  return form;
}
#endif  // BUILDFLAG(IS_ANDROID)

void UpdateSafetyHubServiceAsync(SafetyHubService* service) {
  auto test_observer = std::make_shared<TestObserver>();
  service->AddObserver(test_observer.get());
  // We need to check if there is any update process currently active, and wait
  // until all have completed before running another update.
  while (service->IsUpdateRunning()) {
    base::RunLoop ongoing_update_loop;
    test_observer->SetCallback(ongoing_update_loop.QuitClosure());
    ongoing_update_loop.Run();
  }
  base::RunLoop loop;
  test_observer->SetCallback(loop.QuitClosure());
  service->UpdateAsync();
  loop.Run();
  service->RemoveObserver(test_observer.get());
}

void UpdateUnusedSitePermissionsServiceAsync(
    UnusedSitePermissionsService* service) {
  // Run until the checks complete for unused site permission revocation.
  UpdateSafetyHubServiceAsync(service);

  // Run until the checks complete for abusive notification revocation.
  base::RunLoop().RunUntilIdle();
}

bool IsUrlInSettingsList(ContentSettingsForOneType content_settings, GURL url) {
  for (const auto& setting : content_settings) {
    if (setting.primary_pattern.ToRepresentativeUrl() == url) {
      return true;
    }
  }
  return false;
}

void GenerateSafetyHubMenuNotification(Profile* profile) {
  // Creating and showing a notification for a site that has never been
  // interacted with, will be caught by the notification permission review
  // service, and raised as a Safety Hub issue to be reviewed. In this case a
  // menu entry should be there with the action to open the Safety Hub
  // settings page.
  auto* hcsm = HostContentSettingsMapFactory::GetForProfile(profile);
  const GURL kUrl("https://example.com");
  hcsm->SetContentSettingDefaultScope(
      kUrl, GURL(), content_settings::mojom::ContentSettingsType::NOTIFICATIONS,
      CONTENT_SETTING_ALLOW);
  auto* notifications_engagement_service =
      NotificationsEngagementServiceFactory::GetForProfile(profile);
  // There should be at least an average of 1 recorded notification per day,
  // for the past week to trigger a Safety Hub review.
  notifications_engagement_service->RecordNotificationDisplayed(kUrl, 7);

  // Update the notification permissions review service for it to capture the
  // recently added notification permission.
  auto* notification_permissions_service =
      NotificationPermissionsReviewServiceFactory::GetForProfile(profile);
  safety_hub_test_util::UpdateSafetyHubServiceAsync(
      notification_permissions_service);
}

}  // namespace safety_hub_test_util
