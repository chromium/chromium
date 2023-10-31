// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/extensions_result.h"

#include <memory>

#include "chrome/browser/ui/safety_hub/safety_hub_service.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

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
// Is not a trigger: malware but not present.
static extensions::CWSInfoService::CWSInfo cws_info_no_data{
    false,
    false,
    base::Time::Now(),
    extensions::CWSInfoService::CWSViolationType::kMalware,
    false,
    false};

class MockCWSInfoService : public extensions::CWSInfoService {
 public:
  MOCK_METHOD(absl::optional<CWSInfoServiceInterface::CWSInfo>,
              GetCWSInfo,
              (const extensions::Extension&),
              (const, override));
};

}  // namespace

class SafetyHubExtensionsResultTest : public testing::Test {
 protected:
  void AddExtension(const std::string& name,
                    extensions::mojom::ManifestLocation location) {
    const std::string kId = crx_file::id_util::GenerateId(name);
    scoped_refptr<const extensions::Extension> extension =
        CreateExtension(name, location);
    extensions::ExtensionRegistry::Get(profile())->AddEnabled(extension);
  }

  scoped_refptr<const extensions::Extension> CreateExtension(
      const std::string& name,
      extensions::mojom::ManifestLocation location) {
    const std::string kId = crx_file::id_util::GenerateId(name);
    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder(name)
            .SetManifestKey("host_permissions",
                            base::Value::List().Append(kAllHostsPermission))
            .SetLocation(location)
            .SetID(kId)
            .Build();
    extensions::ExtensionPrefs::Get(profile())->OnExtensionInstalled(
        extension.get(), extensions::Extension::State::ENABLED,
        syncer::StringOrdinal(), "");
    return extension;
  }

  void AddMockExtensions() {
    AddExtension("TestExtension1", ManifestLocation::kInternal);
    AddExtension("TestExtension2", ManifestLocation::kInternal);
    AddExtension("TestExtension3", ManifestLocation::kInternal);
    AddExtension("TestExtension4", ManifestLocation::kInternal);
    AddExtension("TestExtension5", ManifestLocation::kInternal);
    AddExtension("TestExtension6", ManifestLocation::kInternal);
    // Extensions installed by policies will be ignored by the safety
    // check. So extension 7 will not trigger the handler.
    AddExtension("TestExtension7", ManifestLocation::kExternalPolicyDownload);
  }

  void SetMockGetCWSInfoCalls() {
    // Ensure that the mock CWSInfo service returns the needed information.
    EXPECT_CALL(mock_cws_info_service_, GetCWSInfo)
        .Times(6)
        .WillOnce(testing::Return(cws_info_malware))
        .WillOnce(testing::Return(cws_info_policy))
        .WillOnce(testing::Return(cws_info_unpublished))
        .WillOnce(testing::Return(cws_info_multi))
        .WillOnce(testing::Return(cws_info_no_data))
        .WillOnce(testing::Return(cws_info_no_trigger));
  }

  TestingProfile* profile() { return &profile_; }
  extensions::ExtensionPrefs* extension_prefs() {
    return extensions::ExtensionPrefsFactory::GetForBrowserContext(profile());
  }
  extensions::ExtensionRegistry* extension_registry() {
    return extensions::ExtensionRegistry::Get(profile());
  }
  MockCWSInfoService* cws_info_service() { return &mock_cws_info_service_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  testing::NiceMock<MockCWSInfoService> mock_cws_info_service_;
};

TEST_F(SafetyHubExtensionsResultTest, ResultToFromDictAndClone) {
  // Create a result with two triggering extensions. Using unpublished
  // extensions only, as this is the only type that can serialized into a Dict.
  std::set<extensions::ExtensionId> extension_ids;
  extension_ids.insert(crx_file::id_util::GenerateId("Extension1"));
  extension_ids.insert(crx_file::id_util::GenerateId("Extension2"));
  auto result =
      std::make_unique<SafetyHubExtensionsResult>(extension_ids, true);
  EXPECT_TRUE(result->IsTriggerForMenuNotification());
  EXPECT_EQ(2U, result->GetNumTriggeringExtensions());

  // Serialize and de-serialize the result should result in the same triggering
  // extensions.
  auto new_result =
      std::make_unique<SafetyHubExtensionsResult>(result->ToDictValue());
  EXPECT_TRUE(new_result->IsTriggerForMenuNotification());
  EXPECT_EQ(2U, new_result->GetNumTriggeringExtensions());

  // Cloning the result should also result in the same triggering result.
  std::unique_ptr<SafetyHubService::Result> cloned_result = new_result->Clone();
  auto* cloned_extensions_result =
      static_cast<SafetyHubExtensionsResult*>(cloned_result.get());
  EXPECT_TRUE(cloned_extensions_result->IsTriggerForMenuNotification());
  EXPECT_EQ(2U, cloned_extensions_result->GetNumTriggeringExtensions());
}

TEST_F(SafetyHubExtensionsResultTest, GetResult) {
  // Create mock extensions, of which four are a trigger for review (malware,
  // policy violation, unpublished, and a combination of malware + unpublished).
  AddMockExtensions();
  SetMockGetCWSInfoCalls();
  absl::optional<std::unique_ptr<SafetyHubService::Result>> sh_result =
      SafetyHubExtensionsResult::GetResult(
          cws_info_service(), extension_prefs(), extension_registry(), false);
  ASSERT_TRUE(sh_result.has_value());
  auto* result = static_cast<SafetyHubExtensionsResult*>(sh_result->get());
  EXPECT_EQ(4U, result->GetNumTriggeringExtensions());

  // Reset the same mock calls, of which two are unpublished extensions
  // (including one where this is combined with malware).
  SetMockGetCWSInfoCalls();
  absl::optional<std::unique_ptr<SafetyHubService::Result>> sh_menu_result =
      SafetyHubExtensionsResult::GetResult(
          cws_info_service(), extension_prefs(), extension_registry(), true);
  ASSERT_TRUE(sh_menu_result.has_value());
  auto* menu_result =
      static_cast<SafetyHubExtensionsResult*>(sh_menu_result->get());
  EXPECT_EQ(2U, menu_result->GetNumTriggeringExtensions());
}
