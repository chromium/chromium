// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/managed_ui.h"

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
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

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "components/policy/core/common/policy_loader_lacros.h"
#endif

class ManagedUiTest : public InProcessBrowserTest {
 public:
  ManagedUiTest() = default;

  ManagedUiTest(const ManagedUiTest&) = delete;
  ManagedUiTest& operator=(const ManagedUiTest&) = delete;

  ~ManagedUiTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnectorBase::SetPolicyProviderForTesting(&provider_);
  }

  policy::MockConfigurationPolicyProvider* provider() { return &provider_; }

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
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

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
IN_PROC_BROWSER_TEST_F(ManagedUiTest, DoNotDisplayManagedUiForAChild) {
  TestingProfile::Builder builder;
  builder.SetIsSupervisedProfile();
  std::unique_ptr<TestingProfile> profile = builder.Build();

  EXPECT_FALSE(chrome::ShouldDisplayManagedUi(profile.get()));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)

IN_PROC_BROWSER_TEST_F(ManagedUiTest, GetManagedUiMenuItemLabel) {
  // Simulate a managed profile.
  policy::ScopedManagementServiceOverrideForTesting browser_management(
      policy::ManagementServiceFactory::GetForProfile(browser()->profile()),
      policy::EnterpriseManagementAuthority::CLOUD);

  TestingProfile::Builder builder;
  auto profile = builder.Build();

  TestingProfile::Builder builder_with_domain;
  builder_with_domain.SetProfileName("foobar@example.com");
  builder_with_domain.OverridePolicyConnectorIsManagedForTesting(true);
  auto profile_with_domain = builder_with_domain.Build();

  auto* profile_with_hosted_domain = browser()->profile();
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_with_hosted_domain->GetPath());
  ASSERT_TRUE(entry);
  entry->SetHostedDomain("hosteddomain.com");

  EXPECT_EQ(u"Managed by your organization",
            chrome::GetManagedUiMenuItemLabel(profile.get()));
  EXPECT_EQ(u"Managed by example.com",
            chrome::GetManagedUiMenuItemLabel(profile_with_domain.get()));
  EXPECT_EQ(u"Managed by hosteddomain.com",
            chrome::GetManagedUiMenuItemLabel(profile_with_hosted_domain));
}

IN_PROC_BROWSER_TEST_F(ManagedUiTest, GetManagedUiWebUILabel) {
  // Simulate a managed profile.
  policy::ScopedManagementServiceOverrideForTesting browser_management(
      policy::ManagementServiceFactory::GetForProfile(browser()->profile()),
      policy::EnterpriseManagementAuthority::CLOUD);

  TestingProfile::Builder builder;
  auto profile = builder.Build();

  TestingProfile::Builder builder_with_domain;
  builder_with_domain.SetProfileName("foobar@example.com");
  builder_with_domain.OverridePolicyConnectorIsManagedForTesting(true);
  auto profile_with_domain = builder_with_domain.Build();

  auto* profile_with_hosted_domain = browser()->profile();
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_with_hosted_domain->GetPath());
  ASSERT_TRUE(entry);
  entry->SetHostedDomain("hosteddomain.com");

  EXPECT_EQ(
      u"Your <a href=\"chrome://management\">browser is managed</a> by your "
      u"organization",
      chrome::GetManagedUiWebUILabel(profile.get()));
  EXPECT_EQ(
      u"Your <a href=\"chrome://management\">browser is managed</a> by "
      u"example.com",
      chrome::GetManagedUiWebUILabel(profile_with_domain.get()));
  EXPECT_EQ(
      u"Your <a href=\"chrome://management\">browser is managed</a> by "
      u"hosteddomain.com",
      chrome::GetManagedUiWebUILabel(profile_with_hosted_domain));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
using ManagedUiTestCros = policy::DevicePolicyCrosBrowserTest;
IN_PROC_BROWSER_TEST_F(ManagedUiTestCros, GetManagedUiWebUILabel) {
  policy::ScopedManagementServiceOverrideForTesting platform_management(
      policy::ManagementServiceFactory::GetForPlatform(),
      policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);

  EXPECT_EQ(
      u"Your <a target=\"_blank\" "
      u"href=\"chrome://management\">Chrome device is "
      u"managed</a> by example.com",
      chrome::GetDeviceManagedUiWebUILabel());
}
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
IN_PROC_BROWSER_TEST_F(ManagedUiTest, GetSessionManagerIdentity_Unmanaged) {
  EXPECT_EQ(absl::nullopt, chrome::GetSessionManagerIdentity());
}

IN_PROC_BROWSER_TEST_F(ManagedUiTest, GetSessionManagerIdentity_Managed) {
  enterprise_management::PolicyData profile_policy_data;
  profile_policy_data.add_user_affiliation_ids("affiliation-id-1");
  profile_policy_data.set_managed_by("domain.com");
  profile_policy_data.set_device_id("fake-profile-client-id");
  profile_policy_data.set_request_token("fake-browser-dm-token");
  policy::PolicyLoaderLacros::set_main_user_policy_data_for_testing(
      std::move(profile_policy_data));

  absl::optional<std::string> identity = chrome::GetSessionManagerIdentity();
  EXPECT_TRUE(identity.has_value());
  EXPECT_EQ("domain.com", *identity);
}
#endif
