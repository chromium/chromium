// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/safety_hub_test_util.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "chrome/browser/ui/safety_hub/password_status_check_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "services/network/test/test_shared_url_loader_factory.h"

namespace {

using extensions::mojom::ManifestLocation;

const char kAllHostsPermission[] = "*://*/*";

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

}  // namespace

namespace safety_hub_test_util {

using password_manager::BulkLeakCheckService;
using password_manager::TestPasswordStore;

MockCWSInfoService::MockCWSInfoService(Profile* profile)
    : extensions::CWSInfoService(profile) {}
MockCWSInfoService::~MockCWSInfoService() = default;

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

void CreateMockExtensions(Profile* profile) {
  AddExtension("TestExtension1", ManifestLocation::kInternal, profile);
  AddExtension("TestExtension2", ManifestLocation::kInternal, profile);
  AddExtension("TestExtension3", ManifestLocation::kInternal, profile);
  AddExtension("TestExtension4", ManifestLocation::kInternal, profile);
  AddExtension("TestExtension5", ManifestLocation::kInternal, profile);
  AddExtension("TestExtension6", ManifestLocation::kInternal, profile);
  AddExtension("TestExtension7", ManifestLocation::kInternal, profile,
               "https://example.com");
  // Extensions installed by policies will be ignored by Safety Hub. So
  // extension 7 will not trigger the handler.
  AddExtension("TestExtension8", ManifestLocation::kExternalPolicyDownload,
               profile);
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
  const extensions::ExtensionSet extensions =
      extensions::ExtensionRegistry::Get(profile)
          ->GenerateInstalledExtensionsSet(
              extensions::ExtensionRegistry::ENABLED);
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
      name, "ack_safety_check_warning", base::Value(true));
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

}  // namespace safety_hub_test_util
