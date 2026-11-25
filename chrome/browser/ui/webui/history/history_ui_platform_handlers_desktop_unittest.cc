// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_provider.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/glic/test_support/scoped_glic_provider_override.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/webui/history/history_ui_platform_handlers.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui_data_source.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/test/glic_user_session_test_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace history {
namespace {

class HistoryUIGlicTest : public testing::Test {
 protected:
  void SetUp() override {
    TestingProfileManager* testing_profile_manager =
        TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
            /*profile_manager=*/true);
#if BUILDFLAG(IS_CHROMEOS)
    glic_user_session_test_helper_.PreProfileSetUp(
        testing_profile_manager->profile_manager());
#endif  // BUILDFLAG(IS_CHROMEOS)
    profile_ = testing_profile_manager->CreateTestingProfile(
        TestingProfile::kDefaultProfileUserName,
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories());
    glic_test_environment_.SetupProfile(profile_);
  }

  void TearDown() override {
    profile_ = nullptr;
    TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();
#if BUILDFLAG(IS_CHROMEOS)
    glic_user_session_test_helper_.PostProfileTearDown();
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

  Profile* profile() { return profile_; }

  content::BrowserTaskEnvironment task_environment_;
#if BUILDFLAG(IS_CHROMEOS)
  // Glic can run only in a user session, so ChromeOS requires setting up the
  // user session manually.
  ash::GlicUserSessionTestHelper glic_user_session_test_helper_;
#endif  // BUILDFLAG(IS_CHROMEOS)
  glic::GlicUnitTestEnvironment glic_test_environment_;
  raw_ptr<TestingProfile> profile_ = nullptr;
};

TEST_F(HistoryUIGlicTest, ConsumerProviderPopulatesGlicAndActuationFlags) {
  auto* glic_service = glic::GlicKeyedService::Get(profile());
  ASSERT_NE(glic_service, nullptr);
  glic_service->enabling().SetUserEnabledActuationOnWeb(true);

  auto source =
      content::TestWebUIDataSource::Create(chrome::kChromeUIHistoryHost);
  PopulatePlatformDataSource(source->GetWebUIDataSource(), profile());

  EXPECT_EQ(source->GetLocalizedStrings().FindBool(kIsGlicEnabledKey), true);
  EXPECT_EQ(
      source->GetLocalizedStrings().FindBool(kIsGlicWebActuationAvailableKey),
      true);
}

class HistoryUIGlicGeminiEnterpriseTest : public HistoryUIGlicTest {
 public:
  HistoryUIGlicGeminiEnterpriseTest() {
    scoped_feature_list_.InitAndDisableFeature(features::kGeicActuation);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  glic::ScopedGlicProviderOverride provider_override_{
      glic::GlicProvider::kGeminiEnterprise};
};

TEST_F(HistoryUIGlicGeminiEnterpriseTest,
       GeminiEnterpriseProviderHidesGeminiAppsActivityAndActuation) {
  ASSERT_TRUE(glic::GlicEnabling::ShouldShowSettingsPage(profile()));
  ASSERT_TRUE(glic::GlicEnabling::IsEnabledAndConsentForProfile(profile()));
  auto* glic_service = glic::GlicKeyedService::Get(profile());
  ASSERT_NE(glic_service, nullptr);
  glic_service->enabling().SetUserEnabledActuationOnWeb(true);

  auto source =
      content::TestWebUIDataSource::Create(chrome::kChromeUIHistoryHost);
  PopulatePlatformDataSource(source->GetWebUIDataSource(), profile());

  EXPECT_EQ(source->GetLocalizedStrings().FindBool(kIsGlicEnabledKey), false);
  EXPECT_EQ(
      source->GetLocalizedStrings().FindBool(kIsGlicWebActuationAvailableKey),
      false);
}

}  // namespace
}  // namespace history
