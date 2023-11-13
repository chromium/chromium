// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/safety_hub_test_util.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"

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

void AddExtension(const std::string& name,
                  extensions::mojom::ManifestLocation location,
                  Profile* profile) {
  const std::string kId = crx_file::id_util::GenerateId(name);
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder(name)
          .SetManifestKey("host_permissions",
                          base::Value::List().Append(kAllHostsPermission))
          .SetLocation(location)
          .SetID(kId)
          .Build();
  extensions::ExtensionPrefs::Get(profile)->OnExtensionInstalled(
      extension.get(), extensions::Extension::State::ENABLED,
      syncer::StringOrdinal(), "");
  extensions::ExtensionRegistry::Get(profile)->AddEnabled(extension);
}

void RemoveExtension(const std::string& name,
                     extensions::mojom::ManifestLocation location,
                     Profile* profile) {
  const std::string kId = crx_file::id_util::GenerateId(name);
  extensions::ExtensionPrefs::Get(profile)->OnExtensionUninstalled(
      kId, location, false);
  extensions::ExtensionRegistry::Get(profile)->RemoveEnabled(kId);
}

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
// Is not a trigger: malware but not present.
static extensions::CWSInfoService::CWSInfo cws_info_no_data{
    false,
    false,
    base::Time::Now(),
    extensions::CWSInfoService::CWSViolationType::kMalware,
    false,
    false};

}  // namespace

namespace safety_hub_test_util {

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

std::unique_ptr<testing::NiceMock<MockCWSInfoService>> GetMockCWSInfoService(
    Profile* profile) {
  // Ensure that the mock CWSInfo service returns the needed information.
  std::unique_ptr<testing::NiceMock<MockCWSInfoService>> mock_cws_info_service(
      new testing::NiceMock<MockCWSInfoService>(profile));
  EXPECT_CALL(*mock_cws_info_service, GetCWSInfo)
      .Times(6)
      .WillOnce(testing::Return(cws_info_malware))
      .WillOnce(testing::Return(cws_info_policy))
      .WillOnce(testing::Return(cws_info_unpublished))
      .WillOnce(testing::Return(cws_info_multi))
      .WillOnce(testing::Return(cws_info_no_data))
      .WillOnce(testing::Return(cws_info_no_trigger));
  return mock_cws_info_service;
}

void CreateMockExtensions(Profile* profile) {
  AddExtension("TestExtension1", ManifestLocation::kInternal, profile);
  AddExtension("TestExtension2", ManifestLocation::kInternal, profile);
  AddExtension("TestExtension3", ManifestLocation::kInternal, profile);
  AddExtension("TestExtension4", ManifestLocation::kInternal, profile);
  AddExtension("TestExtension5", ManifestLocation::kInternal, profile);
  AddExtension("TestExtension6", ManifestLocation::kInternal, profile);
  // Extensions installed by policies will be ignored by Safety Hub. So
  // extension 7 will not trigger the handler.
  AddExtension("TestExtension7", ManifestLocation::kExternalPolicyDownload,
               profile);
}

void CleanAllMockExtensions(Profile* profile) {
  RemoveExtension("TestExtension1", ManifestLocation::kInternal, profile);
  RemoveExtension("TestExtension2", ManifestLocation::kInternal, profile);
  RemoveExtension("TestExtension3", ManifestLocation::kInternal, profile);
  RemoveExtension("TestExtension4", ManifestLocation::kInternal, profile);
  RemoveExtension("TestExtension5", ManifestLocation::kInternal, profile);
  RemoveExtension("TestExtension6", ManifestLocation::kInternal, profile);
  RemoveExtension("TestExtension7", ManifestLocation::kExternalPolicyDownload,
                  profile);

  // Check that all extensions were successfully uninstalled.
  const extensions::ExtensionSet extensions =
      extensions::ExtensionRegistry::Get(profile)
          ->GenerateInstalledExtensionsSet(
              extensions::ExtensionRegistry::ENABLED);
  EXPECT_TRUE(extensions.empty());
}

}  // namespace safety_hub_test_util
