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
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/webui/management/management_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/strings/grit/components_strings.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icon_types.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "ui/chromeos/devicetype_utils.h"
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

  void AddEnterpriseManagedPolicies() {
    policy::PolicyMap policy_map;
    policy_map.Set("test-policy", policy::POLICY_LEVEL_MANDATORY,
                   policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_PLATFORM,
                   base::Value("hello world"), nullptr);
    provider()->UpdateChromePolicy(policy_map);
  }

  // Returns whether we expect the management UI to actually be displayed for
  // supervised users in this test.
  bool ExpectManagedUiForSupervisedUsers() const {
#if BUILDFLAG(IS_CHROMEOS)
    return false;
#else
    return true;
#endif
  }

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
};

IN_PROC_BROWSER_TEST_F(
    ManagedUiTest,
    ShouldDisplayManagedUiNoPoliciesNotSupervisedReturnsFalse) {
  EXPECT_FALSE(chrome::ShouldDisplayManagedUi(browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(
    ManagedUiTest,
    ShouldDisplayManagedUiWithPoliciesNotSupervisedReturnsTrueOnDesktop) {
  AddEnterpriseManagedPolicies();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_FALSE(chrome::ShouldDisplayManagedUi(browser()->profile()));
#else
  EXPECT_TRUE(chrome::ShouldDisplayManagedUi(browser()->profile()));
#endif
}

IN_PROC_BROWSER_TEST_F(ManagedUiTest, ShouldDisplayManagedUiSupervised) {
  TestingProfile::Builder builder;
  builder.SetIsSupervisedProfile();
  std::unique_ptr<TestingProfile> profile = builder.Build();

  EXPECT_EQ(ExpectManagedUiForSupervisedUsers(),
            chrome::ShouldDisplayManagedUi(profile.get()));
}

// On ChromeOS we don't display the management UI for enterprise or supervised
// users.
IN_PROC_BROWSER_TEST_F(ManagedUiTest, GetDeviceManagedUiHelpLabelEnterprise) {
  // Simulate a managed profile.
  AddEnterpriseManagedPolicies();
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

  // Simulate a supervised profile.
  TestingProfile::Builder builder_supervised;
  builder_supervised.SetIsSupervisedProfile();
  std::unique_ptr<TestingProfile> profile_supervised =
      builder_supervised.Build();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE,
                                       ui::GetChromeOSDeviceName()),
            chrome::GetDeviceManagedUiHelpLabel(profile.get()));
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED_BY,
                                 ui::GetChromeOSDeviceName(), u"example.com"),
      chrome::GetDeviceManagedUiHelpLabel(profile_with_domain.get()));
  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE,
                                       ui::GetChromeOSDeviceName()),
            chrome::GetDeviceManagedUiHelpLabel(profile_with_hosted_domain));
  // Enterprise management takes precedence over supervision in the management
  // UI.
  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE,
                                       ui::GetChromeOSDeviceName()),
            chrome::GetDeviceManagedUiHelpLabel(profile_supervised.get()));
#else
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_MANAGEMENT_SUBTITLE),
            chrome::GetDeviceManagedUiHelpLabel(profile.get()));
  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED_BY,
                                       u"example.com"),
            chrome::GetDeviceManagedUiHelpLabel(profile_with_domain.get()));
  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED_BY,
                                       u"hosteddomain.com"),
            chrome::GetDeviceManagedUiHelpLabel(profile_with_hosted_domain));
  // Enterprise management takes precedence over supervision in the management
  // UI.
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_MANAGEMENT_SUBTITLE),
            chrome::GetDeviceManagedUiHelpLabel(profile_supervised.get()));
#endif
}

IN_PROC_BROWSER_TEST_F(ManagedUiTest, GetDeviceManagedUiHelpLabelSupervised) {
  // Simulate a supervised profile.
  TestingProfile::Builder builder;
  builder.SetIsSupervisedProfile();
  std::unique_ptr<TestingProfile> profile = builder.Build();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE,
                                       ui::GetChromeOSDeviceName()),
            chrome::GetDeviceManagedUiHelpLabel(profile.get()));
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE),
            chrome::GetDeviceManagedUiHelpLabel(profile.get()));
#else
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_HELP_MANAGED_BY_YOUR_PARENT),
            chrome::GetDeviceManagedUiHelpLabel(profile.get()));
#endif
}

IN_PROC_BROWSER_TEST_F(ManagedUiTest, GetDeviceManagedUiHelpLabelNotManaged) {
  // Simulate a non managed profile.
  TestingProfile::Builder builder;
  std::unique_ptr<TestingProfile> profile = builder.Build();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE,
                                       ui::GetChromeOSDeviceName()),
            chrome::GetDeviceManagedUiHelpLabel(profile.get()));
#else
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE),
            chrome::GetDeviceManagedUiHelpLabel(profile.get()));
#endif
}

// TODO(crbug.com/40269124): update the tests below to not depend on the exact
// value of the user-visible string (to make string updates simpler).

// On ChromeOS we don't display the management UI for enterprise or supervised
// users.
#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(ManagedUiTest, GetManagedUiIconEnterprise) {
  // Simulate a managed device.
  AddEnterpriseManagedPolicies();
  policy::ScopedManagementServiceOverrideForTesting browser_management(
      policy::ManagementServiceFactory::GetForProfile(browser()->profile()),
      policy::EnterpriseManagementAuthority::CLOUD);

  // An un-supervised profile.
  TestingProfile::Builder builder;
  auto profile = builder.Build();

  // Simulate a supervised profile.
  TestingProfile::Builder builder_supervised;
  builder_supervised.SetIsSupervisedProfile();
  std::unique_ptr<TestingProfile> profile_supervised =
      builder_supervised.Build();

  EXPECT_EQ(vector_icons::kBusinessChromeRefreshIcon.name,
            chrome::GetManagedUiIcon(profile.get()).name);
  // Enterprise management takes precedence over supervision in the management
  // UI.
  EXPECT_EQ(vector_icons::kBusinessChromeRefreshIcon.name,
            chrome::GetManagedUiIcon(profile_supervised.get()).name);
}

IN_PROC_BROWSER_TEST_F(ManagedUiTest, GetManagedUiIconSupervised) {
  // Simulate a supervised profile.
  TestingProfile::Builder builder;
  builder.SetIsSupervisedProfile();
  std::unique_ptr<TestingProfile> profile = builder.Build();

  EXPECT_EQ(vector_icons::kFamilyLinkIcon.name,
            chrome::GetManagedUiIcon(profile.get()).name);
}

IN_PROC_BROWSER_TEST_F(ManagedUiTest, GetManagedUiMenuLinkUrlEnterprise) {
  // Simulate a managed device.
  AddEnterpriseManagedPolicies();
  policy::ScopedManagementServiceOverrideForTesting browser_management(
      policy::ManagementServiceFactory::GetForProfile(browser()->profile()),
      policy::EnterpriseManagementAuthority::CLOUD);

  // An un-supervised profile.
  TestingProfile::Builder builder;
  auto profile = builder.Build();

  // Simulate a supervised profile.
  TestingProfile::Builder builder_supervised;
  builder_supervised.SetIsSupervisedProfile();
  std::unique_ptr<TestingProfile> profile_supervised =
      builder_supervised.Build();

  EXPECT_EQ(GURL(chrome::kChromeUIManagementURL),
            chrome::GetManagedUiUrl(profile.get()));
  // Enterprise management takes precedence over supervision in the management
  // UI.
  EXPECT_EQ(GURL(chrome::kChromeUIManagementURL),
            chrome::GetManagedUiUrl(profile_supervised.get()));
}

IN_PROC_BROWSER_TEST_F(ManagedUiTest, GetManagedUiMenuLinkUrlSupervised) {
  // Simulate a supervised profile.
  TestingProfile::Builder builder;
  builder.SetIsSupervisedProfile();
  std::unique_ptr<TestingProfile> profile = builder.Build();

  EXPECT_EQ(GURL(supervised_user::kManagedByParentUiMoreInfoUrl),
            chrome::GetManagedUiUrl(profile.get()));
}

IN_PROC_BROWSER_TEST_F(ManagedUiTest, GetManagedUiMenuLinkNotManaged) {
  // Non-managed profile.
  TestingProfile::Builder builder;
  std::unique_ptr<TestingProfile> profile = builder.Build();

  EXPECT_EQ(GURL(), chrome::GetManagedUiUrl(profile.get()));
}

IN_PROC_BROWSER_TEST_F(ManagedUiTest, GetManagedUiMenuItemLabelEnterprise) {
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

  // Simulate a supervised profile.
  TestingProfile::Builder builder_supervised;
  builder_supervised.SetIsSupervisedProfile();
  std::unique_ptr<TestingProfile> profile_supervised =
      builder_supervised.Build();

  const std::string unknown_device_manager = "";
  chrome::ScopedDeviceManagerForTesting unknown_device_manager_for_testing(
      unknown_device_manager.c_str());
  {
    // Unmanaged profile
    policy::ScopedManagementServiceOverrideForTesting
        profile_supervised_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_supervised.get()),
            policy::EnterpriseManagementAuthority::NONE);
    EXPECT_EQ(u"Managed by your parent",
              chrome::GetManagedUiMenuItemLabel(profile_supervised.get()));
  }

  {
    // Simulate a managed profile
    policy::ScopedManagementServiceOverrideForTesting
        profile_with_domain_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_with_domain.get()),
            policy::EnterpriseManagementAuthority::CLOUD);
    EXPECT_EQ(u"Profile managed by example.com",
              chrome::GetManagedUiMenuItemLabel(profile_with_domain.get()));

    policy::ScopedManagementServiceOverrideForTesting
        profile_with_hosted_domain_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_with_hosted_domain),
            policy::EnterpriseManagementAuthority::CLOUD);
    EXPECT_EQ(u"Profile managed by hosteddomain.com",
              chrome::GetManagedUiMenuItemLabel(profile_with_hosted_domain));
  }

  {
    // Simulate managed browser.
    policy::ScopedManagementServiceOverrideForTesting profile_management(
        policy::ManagementServiceFactory::GetForProfile(profile.get()),
        policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(u"Managed by your organization",
              chrome::GetManagedUiMenuItemLabel(profile.get()));

    policy::ScopedManagementServiceOverrideForTesting
        profile_with_domain_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_with_domain.get()),
            policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(u"Managed by your organization",
              chrome::GetManagedUiMenuItemLabel(profile_with_domain.get()));

    policy::ScopedManagementServiceOverrideForTesting
        profile_with_hosted_domain_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_with_hosted_domain),
            policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(u"Managed by your organization",
              chrome::GetManagedUiMenuItemLabel(profile_with_hosted_domain));

    policy::ScopedManagementServiceOverrideForTesting
        profile_supervised_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_supervised.get()),
            policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(u"Managed by your organization",
              chrome::GetManagedUiMenuItemLabel(profile_supervised.get()));
  }

  {
    // Simulate managed browser and profile.
    policy::ScopedManagementServiceOverrideForTesting
        profile_with_domain_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_with_domain.get()),
            policy::EnterpriseManagementAuthority::CLOUD |
                policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(u"Your browser and profile are managed",
              chrome::GetManagedUiMenuItemLabel(profile_with_domain.get()));

    policy::ScopedManagementServiceOverrideForTesting
        profile_with_hosted_domain_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_with_hosted_domain),
            policy::EnterpriseManagementAuthority::CLOUD |
                policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(u"Your browser and profile are managed",
              chrome::GetManagedUiMenuItemLabel(profile_with_hosted_domain));
  }

  {
    // Simulate managed browser with known manager and profile.
    const std::string device_manager = "example.com";
    chrome::ScopedDeviceManagerForTesting device_manager_for_testing(
        device_manager.c_str());
    policy::ScopedManagementServiceOverrideForTesting profile_management(
        policy::ManagementServiceFactory::GetForProfile(profile.get()),
        policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(u"Managed by example.com",
              chrome::GetManagedUiMenuItemLabel(profile.get()));

    policy::ScopedManagementServiceOverrideForTesting
        profile_with_domain_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_with_domain.get()),
            policy::EnterpriseManagementAuthority::CLOUD |
                policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(u"Managed by example.com",
              chrome::GetManagedUiMenuItemLabel(profile_with_domain.get()));

    policy::ScopedManagementServiceOverrideForTesting
        profile_with_hosted_domain_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_with_hosted_domain),
            policy::EnterpriseManagementAuthority::CLOUD |
                policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(u"Your browser and profile are managed",
              chrome::GetManagedUiMenuItemLabel(profile_with_hosted_domain));

    policy::ScopedManagementServiceOverrideForTesting
        profile_supervised_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_supervised.get()),
            policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(u"Managed by example.com",
              chrome::GetManagedUiMenuItemLabel(profile_supervised.get()));
  }
}

IN_PROC_BROWSER_TEST_F(ManagedUiTest, GetManagedUiMenuItemTooltipEnterprise) {
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

  // Simulate a supervised profile.
  TestingProfile::Builder builder_supervised;
  builder_supervised.SetIsSupervisedProfile();
  std::unique_ptr<TestingProfile> profile_supervised =
      builder_supervised.Build();

  const std::string unknown_device_manager = "";
  chrome::ScopedDeviceManagerForTesting unknown_device_manager_for_testing(
      unknown_device_manager.c_str());

  {
    // Simulate a managed profile
    policy::ScopedManagementServiceOverrideForTesting
        profile_with_domain_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_with_domain.get()),
            policy::EnterpriseManagementAuthority::CLOUD);
    EXPECT_EQ(std::u16string(),
              chrome::GetManagedUiMenuItemTooltip(profile_with_domain.get()));

    policy::ScopedManagementServiceOverrideForTesting
        profile_with_hosted_domain_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_with_hosted_domain),
            policy::EnterpriseManagementAuthority::CLOUD);
    EXPECT_EQ(std::u16string(),
              chrome::GetManagedUiMenuItemTooltip(profile_with_hosted_domain));
  }

  {
    // Simulate managed browser.
    policy::ScopedManagementServiceOverrideForTesting profile_management(
        policy::ManagementServiceFactory::GetForProfile(profile.get()),
        policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(std::u16string(),
              chrome::GetManagedUiMenuItemTooltip(profile.get()));

    policy::ScopedManagementServiceOverrideForTesting
        profile_with_domain_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_with_domain.get()),
            policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(std::u16string(),
              chrome::GetManagedUiMenuItemTooltip(profile_with_domain.get()));

    policy::ScopedManagementServiceOverrideForTesting
        profile_with_hosted_domain_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_with_hosted_domain),
            policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(std::u16string(),
              chrome::GetManagedUiMenuItemTooltip(profile_with_hosted_domain));

    policy::ScopedManagementServiceOverrideForTesting
        profile_supervised_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_supervised.get()),
            policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(std::u16string(),
              chrome::GetManagedUiMenuItemTooltip(profile_supervised.get()));
  }

  {
    // Simulate managed browser and profile.
    policy::ScopedManagementServiceOverrideForTesting
        profile_with_domain_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_with_domain.get()),
            policy::EnterpriseManagementAuthority::CLOUD |
                policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(
        l10n_util::GetStringFUTF16(
            IDS_BROWSER_MANAGED_AND_PROFILE_MANAGED_BY_TOOLTIP, u"example.com"),
        chrome::GetManagedUiMenuItemTooltip(profile_with_domain.get()));

    policy::ScopedManagementServiceOverrideForTesting
        profile_with_hosted_domain_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_with_hosted_domain),
            policy::EnterpriseManagementAuthority::CLOUD |
                policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(l10n_util::GetStringFUTF16(
                  IDS_BROWSER_MANAGED_AND_PROFILE_MANAGED_BY_TOOLTIP,
                  u"hosteddomain.com"),
              chrome::GetManagedUiMenuItemTooltip(profile_with_hosted_domain));
  }

  {
    // Simulate managed browser with known manager and profile.
    const std::string device_manager = "example.com";
    chrome::ScopedDeviceManagerForTesting device_manager_for_testing(
        device_manager.c_str());
    policy::ScopedManagementServiceOverrideForTesting profile_management(
        policy::ManagementServiceFactory::GetForProfile(profile.get()),
        policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(std::u16string(),
              chrome::GetManagedUiMenuItemTooltip(profile.get()));

    policy::ScopedManagementServiceOverrideForTesting
        profile_with_domain_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_with_domain.get()),
            policy::EnterpriseManagementAuthority::CLOUD |
                policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(std::u16string(),
              chrome::GetManagedUiMenuItemTooltip(profile_with_domain.get()));

    policy::ScopedManagementServiceOverrideForTesting
        profile_with_hosted_domain_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_with_hosted_domain),
            policy::EnterpriseManagementAuthority::CLOUD |
                policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(l10n_util::GetStringFUTF16(
                  IDS_BROWSER_AND_PROFILE_DIFFERENT_MANAGED_BY_TOOLTIP,
                  u"example.com", u"hosteddomain.com"),
              chrome::GetManagedUiMenuItemTooltip(profile_with_hosted_domain));

    policy::ScopedManagementServiceOverrideForTesting
        profile_supervised_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_supervised.get()),
            policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(std::u16string(),
              chrome::GetManagedUiMenuItemTooltip(profile_supervised.get()));
  }
}

IN_PROC_BROWSER_TEST_F(ManagedUiTest, GetManagedUiMenuItemLabelSupervised) {
  if (!ExpectManagedUiForSupervisedUsers()) {
    return;
  }

  // Simulate a supervised profile.
  TestingProfile::Builder builder;
  builder.SetIsSupervisedProfile();
  std::unique_ptr<TestingProfile> profile = builder.Build();

  EXPECT_EQ(u"Managed by your parent",
            chrome::GetManagedUiMenuItemLabel(profile.get()));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(ManagedUiTest, GetManagedUiWebUIIconEnterprise) {
  // Simulate a managed profile.
  AddEnterpriseManagedPolicies();
  policy::ScopedManagementServiceOverrideForTesting browser_management(
      policy::ManagementServiceFactory::GetForProfile(browser()->profile()),
      policy::EnterpriseManagementAuthority::CLOUD);

  TestingProfile::Builder builder;
  auto profile = builder.Build();

  // Simulate a supervised profile.
  TestingProfile::Builder builder_supervised;
  builder_supervised.SetIsSupervisedProfile();
  std::unique_ptr<TestingProfile> profile_supervised =
      builder_supervised.Build();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_TRUE(chrome::GetManagedUiWebUIIcon(profile.get()).empty());
  EXPECT_TRUE(chrome::GetManagedUiWebUIIcon(profile_supervised.get()).empty());
#else
  EXPECT_EQ("cr:domain", chrome::GetManagedUiWebUIIcon(profile.get()));
  // Enterprise management takes precedence over supervision in the management
  // UI.
  EXPECT_EQ("cr:domain",
            chrome::GetManagedUiWebUIIcon(profile_supervised.get()));
#endif
}

IN_PROC_BROWSER_TEST_F(ManagedUiTest, GetManagedUiWebUIIconSupervised) {
  // Simulate a supervised profile.
  TestingProfile::Builder builder;
  builder.SetIsSupervisedProfile();
  std::unique_ptr<TestingProfile> profile = builder.Build();

  if (ExpectManagedUiForSupervisedUsers()) {
    EXPECT_EQ("cr20:kite", chrome::GetManagedUiWebUIIcon(profile.get()));
  } else {
    EXPECT_TRUE(chrome::GetManagedUiWebUIIcon(profile.get()).empty());
  }
}

IN_PROC_BROWSER_TEST_F(ManagedUiTest, GetManagedUiWebUILabelEnterprise) {
  TestingProfile::Builder builder;
  builder.SetProfileName("foo");
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

  // Simulate a supervised profile.
  TestingProfile::Builder builder_supervised;
  builder_supervised.SetIsSupervisedProfile();
  std::unique_ptr<TestingProfile> profile_supervised =
      builder_supervised.Build();

  const std::string unknown_device_manager = "";
  chrome::ScopedDeviceManagerForTesting unknown_device_manager_for_testing(
      unknown_device_manager.c_str());
  {
    // Simulate a managed profile
    policy::ScopedManagementServiceOverrideForTesting
        profile_with_domain_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_with_domain.get()),
            policy::EnterpriseManagementAuthority::CLOUD);
    EXPECT_EQ(
        u"Your <a href=\"chrome://management\">profile is managed</a> by "
        u"example.com",
        chrome::GetManagedUiWebUILabel(profile_with_domain.get()));

    policy::ScopedManagementServiceOverrideForTesting
        profile_with_hosted_domain_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_with_hosted_domain),
            policy::EnterpriseManagementAuthority::CLOUD);
    EXPECT_EQ(
        u"Your <a href=\"chrome://management\">profile is managed</a> by "
        u"hosteddomain.com",
        chrome::GetManagedUiWebUILabel(profile_with_hosted_domain));
  }

  {
    // Simulate managed browser.
    policy::ScopedManagementServiceOverrideForTesting profile_management(
        policy::ManagementServiceFactory::GetForProfile(profile.get()),
        policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(
        u"Your <a href=\"chrome://management\">browser is managed</a> by your "
        u"organization",
        chrome::GetManagedUiWebUILabel(profile.get()));

    policy::ScopedManagementServiceOverrideForTesting
        profile_with_domain_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_with_domain.get()),
            policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(
        u"Your <a href=\"chrome://management\">browser is managed</a> by your "
        u"organization",
        chrome::GetManagedUiWebUILabel(profile_with_domain.get()));

    policy::ScopedManagementServiceOverrideForTesting
        profile_with_hosted_domain_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_with_hosted_domain),
            policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(
        u"Your <a href=\"chrome://management\">browser is managed</a> by your "
        u"organization",
        chrome::GetManagedUiWebUILabel(profile_with_hosted_domain));

    policy::ScopedManagementServiceOverrideForTesting
        profile_supervised_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_supervised.get()),
            policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(
        u"Your <a href=\"chrome://management\">browser is managed</a> by your "
        u"organization",
        chrome::GetManagedUiWebUILabel(profile_supervised.get()));
  }

  {
    // Simulate managed browser and profile.
    policy::ScopedManagementServiceOverrideForTesting profile_management(
        policy::ManagementServiceFactory::GetForProfile(profile.get()),
        policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(
        u"Your <a href=\"chrome://management\">browser is managed</a> by your "
        u"organization",
        chrome::GetManagedUiWebUILabel(profile.get()));

    policy::ScopedManagementServiceOverrideForTesting
        profile_with_domain_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_with_domain.get()),
            policy::EnterpriseManagementAuthority::CLOUD |
                policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(
        u"Your <a href=\"chrome://management\">browser is managed</a> by your "
        u"organization and your <a href=\"chrome://management\">profile is "
        u"managed</a> by example.com",
        chrome::GetManagedUiWebUILabel(profile_with_domain.get()));

    policy::ScopedManagementServiceOverrideForTesting
        profile_with_hosted_domain_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_with_hosted_domain),
            policy::EnterpriseManagementAuthority::CLOUD |
                policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(
        u"Your <a href=\"chrome://management\">browser is managed</a> by your "
        u"organization and your <a href=\"chrome://management\">profile is "
        u"managed</a> by hosteddomain.com",
        chrome::GetManagedUiWebUILabel(profile_with_hosted_domain));

    policy::ScopedManagementServiceOverrideForTesting
        profile_supervised_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_supervised.get()),
            policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(
        u"Your <a href=\"chrome://management\">browser is managed</a> by your "
        u"organization",
        chrome::GetManagedUiWebUILabel(profile_supervised.get()));
  }

  {
    // Simulate managed browser with known manager and profile.
    const std::string device_manager = "example.com";
    chrome::ScopedDeviceManagerForTesting device_manager_for_testing(
        device_manager.c_str());
    policy::ScopedManagementServiceOverrideForTesting profile_management(
        policy::ManagementServiceFactory::GetForProfile(profile.get()),
        policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(
        u"Your <a href=\"chrome://management\">browser is managed</a> by "
        u"example.com",
        chrome::GetManagedUiWebUILabel(profile.get()));

    policy::ScopedManagementServiceOverrideForTesting
        profile_with_domain_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_with_domain.get()),
            policy::EnterpriseManagementAuthority::CLOUD |
                policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(
        u"Your <a href=\"chrome://management\">browser and profile are "
        u"managed</a> by example.com",
        chrome::GetManagedUiWebUILabel(profile_with_domain.get()));

    policy::ScopedManagementServiceOverrideForTesting
        profile_with_hosted_domain_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_with_hosted_domain),
            policy::EnterpriseManagementAuthority::CLOUD |
                policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(
        u"Your <a href=\"chrome://management\">browser is managed</a> by "
        u"example.com "
        u"and your <a href=\"chrome://management\">profile is "
        u"managed</a> by hosteddomain.com",
        chrome::GetManagedUiWebUILabel(profile_with_hosted_domain));

    policy::ScopedManagementServiceOverrideForTesting
        profile_supervised_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_supervised.get()),
            policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(
        u"Your <a href=\"chrome://management\">browser is managed</a> by "
        u"example.com",
        chrome::GetManagedUiWebUILabel(profile_supervised.get()));
  }
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(ManagedUiTest, GetManagementPageSubtitle) {
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

  // Simulate a supervised profile.
  TestingProfile::Builder builder_supervised;
  builder_supervised.SetIsSupervisedProfile();
  std::unique_ptr<TestingProfile> profile_supervised =
      builder_supervised.Build();

  const std::string unknown_device_manager = "";
  chrome::ScopedDeviceManagerForTesting unknown_device_manager_for_testing(
      unknown_device_manager.c_str());
  {
    // Simulate a managed profile
    policy::ScopedManagementServiceOverrideForTesting
        profile_with_domain_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_with_domain.get()),
            policy::EnterpriseManagementAuthority::CLOUD);
    EXPECT_EQ(u"Your profile is managed by example.com",
              chrome::GetManagementPageSubtitle(profile_with_domain.get()));

    policy::ScopedManagementServiceOverrideForTesting
        profile_with_hosted_domain_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_with_hosted_domain),
            policy::EnterpriseManagementAuthority::CLOUD);
    EXPECT_EQ(u"Your profile is managed by hosteddomain.com",
              chrome::GetManagementPageSubtitle(profile_with_hosted_domain));
  }

  {
    // Simulate managed browser.
    policy::ScopedManagementServiceOverrideForTesting profile_management(
        policy::ManagementServiceFactory::GetForProfile(profile.get()),
        policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(u"Your browser is managed by your organization",
              chrome::GetManagementPageSubtitle(profile.get()));

    policy::ScopedManagementServiceOverrideForTesting
        profile_with_domain_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_with_domain.get()),
            policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(u"Your browser is managed by your organization",
              chrome::GetManagementPageSubtitle(profile_with_domain.get()));

    policy::ScopedManagementServiceOverrideForTesting
        profile_with_hosted_domain_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_with_hosted_domain),
            policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(u"Your browser is managed by your organization",
              chrome::GetManagementPageSubtitle(profile_with_hosted_domain));

    policy::ScopedManagementServiceOverrideForTesting
        profile_supervised_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_supervised.get()),
            policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(u"Your browser is managed by your organization",
              chrome::GetManagementPageSubtitle(profile_supervised.get()));
  }

  {
    // Simulate managed browser and profile.
    policy::ScopedManagementServiceOverrideForTesting profile_management(
        policy::ManagementServiceFactory::GetForProfile(profile.get()),
        policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(u"Your browser is managed by your organization",
              chrome::GetManagementPageSubtitle(profile.get()));

    policy::ScopedManagementServiceOverrideForTesting
        profile_with_domain_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_with_domain.get()),
            policy::EnterpriseManagementAuthority::CLOUD |
                policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(
        u"Your browser is managed by your organization and your profile is "
        u"managed by example.com",
        chrome::GetManagementPageSubtitle(profile_with_domain.get()));

    policy::ScopedManagementServiceOverrideForTesting
        profile_with_hosted_domain_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_with_hosted_domain),
            policy::EnterpriseManagementAuthority::CLOUD |
                policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(
        u"Your browser is managed by your organization and your profile is "
        u"managed by hosteddomain.com",
        chrome::GetManagementPageSubtitle(profile_with_hosted_domain));

    policy::ScopedManagementServiceOverrideForTesting
        profile_supervised_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_supervised.get()),
            policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(u"Your browser is managed by your organization",
              chrome::GetManagementPageSubtitle(profile_supervised.get()));
  }

  {
    // Simulate managed browser with known manager and profile.
    const std::string device_manager = "example.com";
    chrome::ScopedDeviceManagerForTesting device_manager_for_testing(
        device_manager.c_str());
    policy::ScopedManagementServiceOverrideForTesting profile_management(
        policy::ManagementServiceFactory::GetForProfile(profile.get()),
        policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(u"Your browser is managed by example.com",
              chrome::GetManagementPageSubtitle(profile.get()));

    policy::ScopedManagementServiceOverrideForTesting
        profile_with_domain_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_with_domain.get()),
            policy::EnterpriseManagementAuthority::CLOUD |
                policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(u"Your browser and profile are managed by example.com",
              chrome::GetManagementPageSubtitle(profile_with_domain.get()));

    policy::ScopedManagementServiceOverrideForTesting
        profile_with_hosted_domain_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_with_hosted_domain),
            policy::EnterpriseManagementAuthority::CLOUD |
                policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(
        u"Your browser is managed by example.com and your profile is "
        u"managed by hosteddomain.com",
        chrome::GetManagementPageSubtitle(profile_with_hosted_domain));

    policy::ScopedManagementServiceOverrideForTesting
        profile_supervised_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_supervised.get()),
            policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(u"Your browser is managed by example.com",
              chrome::GetManagementPageSubtitle(profile_supervised.get()));
  }
}
#endif  //  !BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(ManagedUiTest, GetManagementBubbleTitle) {
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

  // Simulate a supervised profile.
  TestingProfile::Builder builder_supervised;
  builder_supervised.SetIsSupervisedProfile();
  std::unique_ptr<TestingProfile> profile_supervised =
      builder_supervised.Build();

  const std::string unknown_device_manager = "";
  chrome::ScopedDeviceManagerForTesting unknown_device_manager_for_testing(
      unknown_device_manager.c_str());
  {
    // Simulate a managed profile
    policy::ScopedManagementServiceOverrideForTesting
        profile_with_domain_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_with_domain.get()),
            policy::EnterpriseManagementAuthority::CLOUD);
    EXPECT_EQ(u"example.com manages your profile",
              chrome::GetManagementBubbleTitle(profile_with_domain.get()));

    policy::ScopedManagementServiceOverrideForTesting
        profile_with_hosted_domain_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_with_hosted_domain),
            policy::EnterpriseManagementAuthority::CLOUD);
    EXPECT_EQ(u"hosteddomain.com manages your profile",
              chrome::GetManagementBubbleTitle(profile_with_hosted_domain));
  }

  {
    // Simulate managed browser.
    policy::ScopedManagementServiceOverrideForTesting profile_management(
        policy::ManagementServiceFactory::GetForProfile(profile.get()),
        policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(l10n_util::GetStringUTF16(IDS_MANAGEMENT_DIALOG_BROWSER_MANAGED),
              chrome::GetManagementBubbleTitle(profile.get()));

    policy::ScopedManagementServiceOverrideForTesting
        profile_with_domain_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_with_domain.get()),
            policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(l10n_util::GetStringUTF16(IDS_MANAGEMENT_DIALOG_BROWSER_MANAGED),
              chrome::GetManagementBubbleTitle(profile_with_domain.get()));

    policy::ScopedManagementServiceOverrideForTesting
        profile_with_hosted_domain_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_with_hosted_domain),
            policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(l10n_util::GetStringUTF16(IDS_MANAGEMENT_DIALOG_BROWSER_MANAGED),
              chrome::GetManagementBubbleTitle(profile_with_hosted_domain));

    policy::ScopedManagementServiceOverrideForTesting
        profile_supervised_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_supervised.get()),
            policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(l10n_util::GetStringUTF16(IDS_MANAGEMENT_DIALOG_BROWSER_MANAGED),
              chrome::GetManagementBubbleTitle(profile_supervised.get()));
  }

  {
    // Simulate managed browser and profile.
    policy::ScopedManagementServiceOverrideForTesting profile_management(
        policy::ManagementServiceFactory::GetForProfile(profile.get()),
        policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(l10n_util::GetStringUTF16(IDS_MANAGEMENT_DIALOG_BROWSER_MANAGED),
              chrome::GetManagementBubbleTitle(profile.get()));

    policy::ScopedManagementServiceOverrideForTesting
        profile_with_domain_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_with_domain.get()),
            policy::EnterpriseManagementAuthority::CLOUD |
                policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(
        l10n_util::GetStringUTF16(
            IDS_MANAGEMENT_DIALOG_BROWSER_MANAGED_BY_MULTIPLE_ORGANIZATIONS),
        chrome::GetManagementBubbleTitle(profile_with_domain.get()));

    policy::ScopedManagementServiceOverrideForTesting
        profile_with_hosted_domain_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_with_hosted_domain),
            policy::EnterpriseManagementAuthority::CLOUD |
                policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(
        l10n_util::GetStringUTF16(
            IDS_MANAGEMENT_DIALOG_BROWSER_MANAGED_BY_MULTIPLE_ORGANIZATIONS),
        chrome::GetManagementBubbleTitle(profile_with_hosted_domain));

    policy::ScopedManagementServiceOverrideForTesting
        profile_supervised_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_supervised.get()),
            policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(l10n_util::GetStringUTF16(IDS_MANAGEMENT_DIALOG_BROWSER_MANAGED),
              chrome::GetManagementBubbleTitle(profile_supervised.get()));
  }

  {
    // Simulate managed browser with known manager and profile.
    const std::string device_manager = "example.com";
    chrome::ScopedDeviceManagerForTesting device_manager_for_testing(
        device_manager.c_str());
    policy::ScopedManagementServiceOverrideForTesting profile_management(
        policy::ManagementServiceFactory::GetForProfile(profile.get()),
        policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(l10n_util::GetStringFUTF16(
                  IDS_MANAGEMENT_DIALOG_BROWSER_MANAGED_BY, u"example.com"),
              chrome::GetManagementBubbleTitle(profile.get()));

    policy::ScopedManagementServiceOverrideForTesting
        profile_with_domain_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_with_domain.get()),
            policy::EnterpriseManagementAuthority::CLOUD |
                policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(l10n_util::GetStringFUTF16(
                  IDS_MANAGEMENT_DIALOG_BROWSER_MANAGED_BY, u"example.com"),
              chrome::GetManagementBubbleTitle(profile_with_domain.get()));

    policy::ScopedManagementServiceOverrideForTesting
        profile_with_hosted_domain_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_with_hosted_domain),
            policy::EnterpriseManagementAuthority::CLOUD |
                policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(
        l10n_util::GetStringUTF16(
            IDS_MANAGEMENT_DIALOG_BROWSER_MANAGED_BY_MULTIPLE_ORGANIZATIONS),
        chrome::GetManagementBubbleTitle(profile_with_hosted_domain));

    policy::ScopedManagementServiceOverrideForTesting
        profile_supervised_management(
            policy::ManagementServiceFactory::GetForProfile(
                profile_supervised.get()),
            policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(l10n_util::GetStringFUTF16(
                  IDS_MANAGEMENT_DIALOG_BROWSER_MANAGED_BY, u"example.com"),
              chrome::GetManagementBubbleTitle(profile_supervised.get()));
  }
}
#endif  //  !BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(ManagedUiTest, GetManagedUiWebUILabelSupervised) {
  // Simulate a supervised profile.
  TestingProfile::Builder builder;
  builder.SetIsSupervisedProfile();
  std::unique_ptr<TestingProfile> profile = builder.Build();

  if (ExpectManagedUiForSupervisedUsers()) {
    EXPECT_EQ(
        u"Your <a href=\"https://familylink.google.com/setting/resource/94\">"
        u"browser is managed</a> by your parent",
        chrome::GetManagedUiWebUILabel(profile.get()));
  } else {
    EXPECT_TRUE(chrome::GetManagedUiWebUILabel(profile.get()).empty());
  }
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
  EXPECT_EQ(std::nullopt, chrome::GetSessionManagerIdentity());
}

IN_PROC_BROWSER_TEST_F(ManagedUiTest, GetSessionManagerIdentity_Managed) {
  enterprise_management::PolicyData profile_policy_data;
  profile_policy_data.add_user_affiliation_ids("affiliation-id-1");
  profile_policy_data.set_managed_by("domain.com");
  profile_policy_data.set_device_id("fake-profile-client-id");
  profile_policy_data.set_request_token("fake-browser-dm-token");
  policy::PolicyLoaderLacros::set_main_user_policy_data_for_testing(
      std::move(profile_policy_data));

  std::optional<std::string> identity = chrome::GetSessionManagerIdentity();
  EXPECT_TRUE(identity.has_value());
  EXPECT_EQ("domain.com", *identity);
}
#endif
