// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/managed_ui_handler.h"

#include <optional>

#include "base/token.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "content/public/test/test_web_ui_data_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/policy/core/browser/browser_policy_connector_base.h"
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

class TestManagedUIHandler : public ManagedUIHandler {
 public:
  using ManagedUIHandler::InitializeInternal;
};

class ManagedUIHandlerTest : public testing::Test {
 public:
  ManagedUIHandlerTest()
      : source_(content::TestWebUIDataSource::Create(
            base::Token::CreateRandom().ToString())) {
    // Create a TestingProfile that uses our MockConfigurationPolicyProvider.
    policy_provider()->Init();
    policy::PolicyServiceImpl::Providers providers = {policy_provider()};
    TestingProfile::Builder builder;
    builder.SetPolicyService(
        std::make_unique<policy::PolicyServiceImpl>(std::move(providers)));
    profile_ = builder.Build();

    // `source_` has been created with a random name, so calling
    // AddDataSourceForBrowserContext() won't accidentally destroy other data
    // sources.
    source_->AddDataSourceForBrowserContext(profile());
  }

  void TearDown() override { policy_provider()->Shutdown(); }

  TestingProfile* profile() { return profile_.get(); }
  policy::MockConfigurationPolicyProvider* policy_provider() {
    return &policy_provider_;
  }
  policy::ProfilePolicyConnector* profile_policy_connector() {
    return profile()->GetProfilePolicyConnector();
  }

  void InitializeHandler() {
    TestManagedUIHandler::InitializeInternal(
        &web_ui_, source_->GetWebUIDataSource(), profile());
    web_ui_.HandleReceivedMessage("observeManagedUI", base::Value::List());
  }

  bool IsSourceManaged() {
    const auto* local_strings = source_->GetLocalizedStrings();
    std::optional<bool> managed = local_strings->FindBool("isManaged");
    if (!managed.has_value()) {
      ADD_FAILURE();
      return false;
    }
    return managed.value();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;

  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
  std::unique_ptr<TestingProfile> profile_;

  content::TestWebUI web_ui_;
  std::unique_ptr<content::TestWebUIDataSource> source_;
};

TEST_F(ManagedUIHandlerTest, ManagedUIDisabledByDefault) {
  InitializeHandler();
  EXPECT_FALSE(IsSourceManaged());
}

TEST_F(ManagedUIHandlerTest, ManagedUIEnabledWhenManaged) {
  profile_policy_connector()->OverrideIsManagedForTesting(true);
  InitializeHandler();
  EXPECT_TRUE(IsSourceManaged());
}

TEST_F(ManagedUIHandlerTest, ManagedUIBecomesEnabledByProfile) {
  InitializeHandler();
  EXPECT_FALSE(IsSourceManaged());

  // Make ProfilePolicyConnector::IsManaged() return true.
  profile_policy_connector()->OverrideIsManagedForTesting(true);
  policy::PolicyMap non_empty_map;
  non_empty_map.Set("FakePolicyName", policy::POLICY_LEVEL_MANDATORY,
                    policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                    base::Value("fake"), nullptr);
  policy_provider()->UpdateChromePolicy(non_empty_map);

  // Source should auto-update.
  EXPECT_TRUE(IsSourceManaged());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(ManagedUIHandlerTest, ManagedUIDisabledForChildAccount) {
  profile_policy_connector()->OverrideIsManagedForTesting(true);
  profile()->SetIsSupervisedProfile();

  InitializeHandler();

  EXPECT_FALSE(IsSourceManaged());
}
#endif
