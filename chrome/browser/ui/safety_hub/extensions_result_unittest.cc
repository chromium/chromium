// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/extensions_result.h"

#include <memory>

#include "chrome/browser/extensions/cws_info_service.h"
#include "chrome/browser/extensions/cws_info_service_factory.h"
#include "chrome/browser/ui/safety_hub/safety_hub_service.h"
#include "chrome/browser/ui/safety_hub/safety_hub_test_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/blocklist_extension_prefs.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension_urls.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class SafetyHubExtensionsResultTest : public testing::Test {
 public:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        {features::kSafetyHubExtensionsUwSTrigger,
         features::kSafetyHubExtensionsOffStoreTrigger,
         features::kSafetyHubExtensionsNoPrivacyPracticesTrigger},
        /*disabled_features=*/{});
    extension_prefs_ = extensions::ExtensionPrefs::Get(profile());
  }

  std::unique_ptr<KeyedService> SetMockCWSInfoService(
      content::BrowserContext* context) {
    return safety_hub_test_util::GetMockCWSInfoService(profile());
  }

 protected:
  TestingProfile* profile() { return &profile_; }
  extensions::ExtensionPrefs* extension_prefs() { return extension_prefs_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  base::test::ScopedFeatureList feature_list_;
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
  // Create mock extensions, of which five are a trigger for review (malware,
  // policy violation, unpublished, and a combination of malware + unpublished,
  // and offstore extension).
  safety_hub_test_util::CreateMockExtensions(profile());
  extensions::CWSInfoServiceFactory::GetInstance()->SetTestingFactory(
      profile(),
      base::BindRepeating(&SafetyHubExtensionsResultTest::SetMockCWSInfoService,
                          base::Unretained(this)));
  std::optional<std::unique_ptr<SafetyHubService::Result>> sh_result =
      SafetyHubExtensionsResult::GetResult(profile(), false);
  ASSERT_TRUE(sh_result.has_value());
  auto* result = static_cast<SafetyHubExtensionsResult*>(sh_result->get());
  EXPECT_EQ(5U, result->GetNumTriggeringExtensions());

  // Reset the same mock calls, of which two are unpublished extensions
  // (including one where this is combined with malware).
  extensions::CWSInfoServiceFactory::GetInstance()->SetTestingFactory(
      profile(),
      base::BindRepeating(&SafetyHubExtensionsResultTest::SetMockCWSInfoService,
                          base::Unretained(this)));
  std::optional<std::unique_ptr<SafetyHubService::Result>> sh_menu_result =
      SafetyHubExtensionsResult::GetResult(profile(), true);
  ASSERT_TRUE(sh_menu_result.has_value());
  auto* menu_result =
      static_cast<SafetyHubExtensionsResult*>(sh_menu_result->get());
  EXPECT_EQ(2U, menu_result->GetNumTriggeringExtensions());
}
