// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/managed_ui.h"

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#endif

class ManagedUiTest : public InProcessBrowserTest {
 public:
  ManagedUiTest() = default;
  ~ManagedUiTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    ON_CALL(provider_, IsInitializationComplete(testing::_))
        .WillByDefault(testing::Return(true));
    ON_CALL(provider_, IsFirstPolicyLoadComplete(testing::_))
        .WillByDefault(testing::Return(true));
    policy::BrowserPolicyConnectorBase::SetPolicyProviderForTesting(&provider_);
  }

  policy::MockConfigurationPolicyProvider* provider() { return &provider_; }

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;

  DISALLOW_COPY_AND_ASSIGN(ManagedUiTest);
};

IN_PROC_BROWSER_TEST_F(ManagedUiTest, ShouldDisplayManagedUiNoPolicies) {
  EXPECT_FALSE(chrome::ShouldDisplayManagedUi(browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(ManagedUiTest, ShouldDisplayManagedUiOnDesktop) {
  policy::PolicyMap policy_map;
  policy_map.Set("test-policy", policy::POLICY_LEVEL_MANDATORY,
                 policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_PLATFORM,
                 base::Value("hello world"), nullptr);
  provider()->UpdateChromePolicy(policy_map);

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
  builder_with_domain.OverridePolicyConnectorIsManagedForTesting(true);
  auto profile_with_domain = builder_with_domain.Build();

  EXPECT_EQ(u"Managed by your organization",
            chrome::GetManagedUiMenuItemLabel(profile.get()));
  EXPECT_EQ(u"Managed by example.com",
            chrome::GetManagedUiMenuItemLabel(profile_with_domain.get()));
}

IN_PROC_BROWSER_TEST_F(ManagedUiTest, GetManagedUiWebUILabel) {
  TestingProfile::Builder builder;
  auto profile = builder.Build();

  TestingProfile::Builder builder_with_domain;
  builder_with_domain.SetProfileName("foobar@example.com");
  builder_with_domain.OverridePolicyConnectorIsManagedForTesting(true);
  auto profile_with_domain = builder_with_domain.Build();

  EXPECT_EQ(
      u"Your <a href=\"chrome://management\">browser is managed</a> by your "
      u"organization",
      chrome::GetManagedUiWebUILabel(profile.get()));
  EXPECT_EQ(
      u"Your <a href=\"chrome://management\">browser is managed</a> by "
      u"example.com",
      chrome::GetManagedUiWebUILabel(profile_with_domain.get()));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
using ManagedUiTestCros = policy::DevicePolicyCrosBrowserTest;
IN_PROC_BROWSER_TEST_F(ManagedUiTestCros, GetManagedUiWebUILabel) {
  policy::ScopedManagementServiceOverrideForTesting platform_management(
      policy::ManagementTarget::PLATFORM,
      {policy::EnterpriseManagementAuthority::DOMAIN_LOCAL});

  EXPECT_EQ(
      u"Your <a target=\"_blank\" "
      u"href=\"chrome://management\">Chrome device is "
      u"managed</a> by example.com",
      chrome::GetDeviceManagedUiWebUILabel());
}
#endif
