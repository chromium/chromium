// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/extensions_result.h"

#include <memory>

#include "chrome/browser/ui/safety_hub/safety_hub_service.h"
#include "chrome/browser/ui/safety_hub/safety_hub_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/blocklist_extension_prefs.h"
#include "extensions/browser/extension_prefs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class SafetyHubExtensionsResultTest : public testing::Test {
 public:
  void SetUp() override {
    extension_prefs_ = extensions::ExtensionPrefs::Get(profile());
  }

 protected:
  TestingProfile* profile() { return &profile_; }
  extensions::ExtensionPrefs* extension_prefs() { return extension_prefs_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  raw_ptr<extensions::ExtensionPrefs> extension_prefs_;
};

TEST_F(SafetyHubExtensionsResultTest, CloneResult) {
  // Create a result with two triggering extensions. Using unpublished
  // extensions only, as this is the only type that can serialized into a Dict.
  std::set<extensions::ExtensionId> extension_ids;
  extension_ids.insert(crx_file::id_util::GenerateId("Extension1"));
  extension_ids.insert(crx_file::id_util::GenerateId("Extension2"));
  auto result =
      std::make_unique<SafetyHubExtensionsResult>(extension_ids, true);
  EXPECT_TRUE(result->IsTriggerForMenuNotification());
  EXPECT_EQ(2U, result->GetNumTriggeringExtensions());

  // Cloning the result should also result in the same triggering result.
  std::unique_ptr<SafetyHubService::Result> cloned_result = result->Clone();
  auto* cloned_extensions_result =
      static_cast<SafetyHubExtensionsResult*>(cloned_result.get());
  EXPECT_TRUE(cloned_extensions_result->IsTriggerForMenuNotification());
  EXPECT_EQ(2U, cloned_extensions_result->GetNumTriggeringExtensions());
}

TEST_F(SafetyHubExtensionsResultTest, GetResult) {
  // Create mock extensions, of which four are a trigger for review (malware,
  // policy violation, unpublished, and a combination of malware + unpublished).
  safety_hub_test_util::CreateMockExtensions(profile());
  std::unique_ptr<testing::NiceMock<safety_hub_test_util::MockCWSInfoService>>
      cws_info_service = safety_hub_test_util::GetMockCWSInfoService(profile());
  std::optional<std::unique_ptr<SafetyHubService::Result>> sh_result =
      SafetyHubExtensionsResult::GetResult(cws_info_service.get(), profile(),
                                           false);
  ASSERT_TRUE(sh_result.has_value());
  auto* result = static_cast<SafetyHubExtensionsResult*>(sh_result->get());
  EXPECT_EQ(4U, result->GetNumTriggeringExtensions());

  // Reset the same mock calls, of which two are unpublished extensions
  // (including one where this is combined with malware).
  cws_info_service = safety_hub_test_util::GetMockCWSInfoService(profile());
  std::optional<std::unique_ptr<SafetyHubService::Result>> sh_menu_result =
      SafetyHubExtensionsResult::GetResult(cws_info_service.get(), profile(),
                                           true);
  ASSERT_TRUE(sh_menu_result.has_value());
  auto* menu_result =
      static_cast<SafetyHubExtensionsResult*>(sh_menu_result->get());
  EXPECT_EQ(2U, menu_result->GetNumTriggeringExtensions());
}

TEST_F(SafetyHubExtensionsResultTest, GetResult_BlocklistPrefs) {
  // Create 3 mock extensions, of which 2 are a blocklist triggers for review
  // (malware, policy violation).
  const std::string extension_name_malware = "TestExtensionMalware";
  const std::string extension_name_policy = "TestExtensionPolicy";
  safety_hub_test_util::AddExtension(
      extension_name_malware, extensions::mojom::ManifestLocation::kInternal,
      profile());
  safety_hub_test_util::AddExtension(
      extension_name_policy, extensions::mojom::ManifestLocation::kInternal,
      profile());
  safety_hub_test_util::AddExtension(
      "TestExtension", extensions::mojom::ManifestLocation::kInternal,
      profile());

  extensions::blocklist_prefs::SetSafeBrowsingExtensionBlocklistState(
      crx_file::id_util::GenerateId(extension_name_malware),
      extensions::BitMapBlocklistState::BLOCKLISTED_MALWARE, extension_prefs());
  extensions::blocklist_prefs::SetSafeBrowsingExtensionBlocklistState(
      crx_file::id_util::GenerateId(extension_name_policy),
      extensions::BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION,
      extension_prefs());

  std::unique_ptr<testing::NiceMock<safety_hub_test_util::MockCWSInfoService>>
      cws_info_service = safety_hub_test_util::GetMockCWSInfoService(
          profile(), /*with_calls=*/false);
  EXPECT_CALL(*cws_info_service, GetCWSInfo)
      .Times(3)
      .WillOnce(testing::Return(safety_hub_test_util::GetCWSInfoNoTrigger()))
      .WillOnce(testing::Return(safety_hub_test_util::GetCWSInfoNoTrigger()))
      .WillOnce(testing::Return(safety_hub_test_util::GetCWSInfoNoTrigger()));

  std::optional<std::unique_ptr<SafetyHubService::Result>> sh_result =
      SafetyHubExtensionsResult::GetResult(cws_info_service.get(), profile(),
                                           false);
  ASSERT_TRUE(sh_result.has_value());
  auto* result = static_cast<SafetyHubExtensionsResult*>(sh_result->get());
  EXPECT_EQ(2U, result->GetNumTriggeringExtensions());
}
