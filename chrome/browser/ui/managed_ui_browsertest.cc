// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/managed_ui.h"

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "testing/gmock/include/gmock/gmock.h"

class ManagedUiTest : public InProcessBrowserTest {
 public:
  ManagedUiTest() = default;
  ~ManagedUiTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    EXPECT_CALL(provider_, IsInitializationComplete(testing::_))
        .WillRepeatedly(testing::Return(true));
    policy::BrowserPolicyConnectorBase::SetPolicyProviderForTesting(&provider_);
  }

  policy::MockConfigurationPolicyProvider* provider() { return &provider_; }

 private:
  policy::MockConfigurationPolicyProvider provider_;

  DISALLOW_COPY_AND_ASSIGN(ManagedUiTest);
};

IN_PROC_BROWSER_TEST_F(ManagedUiTest, ShouldDisplayManagedUiNoPolicies) {
  EXPECT_FALSE(chrome::ShouldDisplayManagedUi(browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(ManagedUiTest, ShouldDisplayManagedUiOnDesktop) {
  policy::PolicyMap policy_map;
  policy_map.Set("test-policy", policy::POLICY_LEVEL_MANDATORY,
                 policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_PLATFORM,
                 std::make_unique<base::Value>("hello world"), nullptr);
  provider()->UpdateChromePolicy(policy_map);

#if defined(OS_CHROMEOS)
  EXPECT_FALSE(chrome::ShouldDisplayManagedUi(browser()->profile()));
#else
  EXPECT_TRUE(chrome::ShouldDisplayManagedUi(browser()->profile()));
#endif
}

IN_PROC_BROWSER_TEST_F(ManagedUiTest, GetManagedUiMenuItemLabel) {
  TestingProfile::Builder builder;
  auto profile = builder.Build();

  TestingProfile::Builder builder_with_domain;
  builder_with_domain.SetProfileName("foobar@example.com");
  auto profile_with_domain = builder_with_domain.Build();

  EXPECT_EQ(base::ASCIIToUTF16("Managed by your organization"),
            chrome::GetManagedUiMenuItemLabel(profile.get()));
  EXPECT_EQ(base::ASCIIToUTF16("Managed by example.com"),
            chrome::GetManagedUiMenuItemLabel(profile_with_domain.get()));
}

IN_PROC_BROWSER_TEST_F(ManagedUiTest, GetManagedUiWebUILabel) {
  TestingProfile::Builder builder;
  auto profile = builder.Build();

  TestingProfile::Builder builder_with_domain;
  builder_with_domain.SetProfileName("foobar@example.com");
  auto profile_with_domain = builder_with_domain.Build();

  EXPECT_EQ(
      base::ASCIIToUTF16(
          "Your <a href=\"chrome://management\">browser is managed</a> by your "
          "organization"),
      chrome::GetManagedUiWebUILabel(profile.get()));
  EXPECT_EQ(
      base::ASCIIToUTF16(
          "Your <a href=\"chrome://management\">browser is managed</a> by "
          "example.com"),
      chrome::GetManagedUiWebUILabel(profile_with_domain.get()));
#if defined(OS_CHROMEOS)
  EXPECT_EQ(base::ASCIIToUTF16("Your <a target=\"_blank\" "
                               "href=\"chrome://management\">Chrome device is "
                               "managed</a> by your organization"),
            chrome::GetDeviceManagedUiWebUILabel(profile.get()));
  EXPECT_EQ(base::ASCIIToUTF16("Your <a target=\"_blank\" "
                               "href=\"chrome://management\">Chrome device is "
                               "managed</a> by example.com"),
            chrome::GetDeviceManagedUiWebUILabel(profile_with_domain.get()));
#endif
}
