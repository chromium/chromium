// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/enterprise/browser_management/management_identity.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service_factory.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/browser/browser_policy_connector_base.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/strings/grit/components_strings.h"
#include "components/supervised_user/core/browser/supervised_user_test_environment.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/vector_icon_types.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "ui/chromeos/devicetype_utils.h"
#endif

namespace {

class ManagedUiSupervisedUserTest : public testing::Test {
 protected:
  ManagedUiSupervisedUserTest() = default;
  ~ManagedUiSupervisedUserTest() override = default;

  void SetUp() override {
    provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnectorBase::SetPolicyProviderForTesting(&provider_);

    TestingProfile::Builder builder;
    builder.AddTestingFactories(IdentityTestEnvironmentProfileAdaptor::
                                    GetIdentityTestEnvironmentFactories());
    profile_ = builder.Build();

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());

    // Sync the supervision status to the profile.
    supervised_user::SupervisedUserTestEnvironment::EnableSupervisedAccount(
        identity_manager());
  }

  void TearDown() override {
    identity_test_env_adaptor_.reset();
    profile_.reset();
    policy::BrowserPolicyConnectorBase::SetPolicyProviderForTesting(nullptr);
  }

  void AddEnterpriseManagedPolicies() {
    policy::PolicyMap policy_map;
    policy_map.Set("test-policy", policy::POLICY_LEVEL_MANDATORY,
                   policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_PLATFORM,
                   base::Value("hello world"), nullptr);
    provider_.UpdateChromePolicy(policy_map);
  }

  // On ChromeOS, management UI is displayed at the OS level, so Family Link
  // supervision does not display browser-level managed UI.
  bool ExpectManagedUiForSupervisedUsers() const {
#if BUILDFLAG(IS_CHROMEOS)
    return false;
#else
    return true;
#endif
  }

  Profile* profile() { return profile_.get(); }

 private:
  signin::IdentityManager* identity_manager() {
    return identity_test_env_adaptor_->identity_test_env()->identity_manager();
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
};

TEST_F(ManagedUiSupervisedUserTest, ShouldDisplayManagedUiSupervised) {
  EXPECT_EQ(ExpectManagedUiForSupervisedUsers(),
            ShouldDisplayManagedUi(profile()));
}

TEST_F(ManagedUiSupervisedUserTest, GetDeviceManagedUiHelpLabelEnterprise) {
  AddEnterpriseManagedPolicies();

  // Enterprise management takes precedence over supervision in the management
  // UI.
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE,
                                       ui::GetChromeOSDeviceName()),
            GetDeviceManagedUiHelpLabel(profile()));
#else
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_MANAGEMENT_SUBTITLE),
            GetDeviceManagedUiHelpLabel(profile()));
#endif
}

TEST_F(ManagedUiSupervisedUserTest, GetDeviceManagedUiHelpLabelSupervised) {
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE,
                                       ui::GetChromeOSDeviceName()),
            GetDeviceManagedUiHelpLabel(profile()));
#else
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_HELP_MANAGED_BY_YOUR_PARENT),
            GetDeviceManagedUiHelpLabel(profile()));
#endif
}

// On ChromeOS we don't display the management UI for supervised users.
#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(ManagedUiSupervisedUserTest, GetManagedUiIconEnterprise) {
  // Simulate a managed device.
  AddEnterpriseManagedPolicies();

  // Enterprise management takes precedence over supervision in the management
  // UI.
  EXPECT_EQ(features::IsRoundedIconsEnabled()
                ? vector_icons::kDomainIcon.name
                : vector_icons::kBusinessChromeRefreshOldIcon.name,
            GetManagedUiIcon(profile()).name);
}

TEST_F(ManagedUiSupervisedUserTest, GetManagedUiIconSupervised) {
  EXPECT_EQ(features::IsRoundedIconsEnabled()
                ? vector_icons::kFamilyLinkIcon.name
                : vector_icons::kFamilyLinkOldIcon.name,
            GetManagedUiIcon(profile()).name);
}

TEST_F(ManagedUiSupervisedUserTest, GetManagedUiMenuLinkUrlEnterprise) {
  // Simulate a managed device.
  AddEnterpriseManagedPolicies();

  // Enterprise management takes precedence over supervision in the management
  // UI.
  EXPECT_EQ(GURL(chrome::kChromeUIManagementURL), GetManagedUiUrl(profile()));
}

TEST_F(ManagedUiSupervisedUserTest, GetManagedUiMenuLinkUrlSupervised) {
  EXPECT_EQ(GURL(supervised_user::kManagedByParentUiMoreInfoUrl),
            GetManagedUiUrl(profile()));
}

TEST_F(ManagedUiSupervisedUserTest, GetManagedUiMenuItemLabelEnterprise) {
  ScopedDeviceManagerForTesting unknown_device_manager("");

  {
    // Unmanaged profile
    policy::ScopedManagementServiceOverrideForTesting profile_management(
        policy::ManagementServiceFactory::GetForProfile(profile()),
        policy::EnterpriseManagementAuthority::NONE);
    EXPECT_EQ(u"Managed by your parent", GetManagedUiMenuItemLabel(profile()));
  }

  {
    // Simulate managed browser.
    policy::ScopedManagementServiceOverrideForTesting profile_management(
        policy::ManagementServiceFactory::GetForProfile(profile()),
        policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(u"Managed by your organization",
              GetManagedUiMenuItemLabel(profile()));
  }

  {
    // Simulate managed browser with known manager and profile.
    ScopedDeviceManagerForTesting device_manager_override("example.com");
    policy::ScopedManagementServiceOverrideForTesting profile_management(
        policy::ManagementServiceFactory::GetForProfile(profile()),
        policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(u"Managed by example.com", GetManagedUiMenuItemLabel(profile()));
  }
}

TEST_F(ManagedUiSupervisedUserTest, GetManagedUiMenuItemTooltipEnterprise) {
  ScopedDeviceManagerForTesting unknown_device_manager("");

  {
    // Simulate managed browser.
    policy::ScopedManagementServiceOverrideForTesting profile_management(
        policy::ManagementServiceFactory::GetForProfile(profile()),
        policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(std::u16string(), GetManagedUiMenuItemTooltip(profile()));
  }

  {
    // Simulate managed browser with known manager and profile.
    ScopedDeviceManagerForTesting device_manager("example.com");
    policy::ScopedManagementServiceOverrideForTesting profile_management(
        policy::ManagementServiceFactory::GetForProfile(profile()),
        policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(std::u16string(), GetManagedUiMenuItemTooltip(profile()));
  }
}

#endif  // !BUILDFLAG(IS_CHROMEOS)

TEST_F(ManagedUiSupervisedUserTest, GetManagedUiWebUIIconEnterprise) {
  // Simulate a managed profile.
  AddEnterpriseManagedPolicies();

#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_TRUE(GetManagedUiWebUIIcon(profile()).empty());
#else
  // Enterprise management takes precedence over supervision in the management
  // UI.
  EXPECT_EQ("cr:domain", GetManagedUiWebUIIcon(profile()));
#endif
}

TEST_F(ManagedUiSupervisedUserTest, GetManagedUiWebUIIconSupervised) {
  if (ExpectManagedUiForSupervisedUsers()) {
    EXPECT_EQ("cr20:family-link", GetManagedUiWebUIIcon(profile()));
  } else {
    EXPECT_TRUE(GetManagedUiWebUIIcon(profile()).empty());
  }
}

TEST_F(ManagedUiSupervisedUserTest, GetManagedUiWebUILabelEnterprise) {
  ScopedDeviceManagerForTesting unknown_device_manager("");

  {
    // Simulate managed browser.
    policy::ScopedManagementServiceOverrideForTesting profile_management(
        policy::ManagementServiceFactory::GetForProfile(profile()),
        policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(
        u"Your <a href=\"chrome://management\">browser is managed</a> by your "
        u"organization",
        GetManagedUiWebUILabel(profile()));
  }

  {
    // Simulate managed browser and profile.
    policy::ScopedManagementServiceOverrideForTesting profile_management(
        policy::ManagementServiceFactory::GetForProfile(profile()),
        policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(
        u"Your <a href=\"chrome://management\">browser is managed</a> by your "
        u"organization",
        GetManagedUiWebUILabel(profile()));
  }

  {
    // Simulate managed browser with known manager and profile.
    ScopedDeviceManagerForTesting device_manager("example.com");
    policy::ScopedManagementServiceOverrideForTesting profile_management(
        policy::ManagementServiceFactory::GetForProfile(profile()),
        policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(
        u"Your <a href=\"chrome://management\">browser is managed</a> by "
        u"example.com",
        GetManagedUiWebUILabel(profile()));
  }
}

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(ManagedUiSupervisedUserTest, GetManagementPageSubtitle) {
  ScopedDeviceManagerForTesting unknown_device_manager("");

  {
    // Simulate managed browser.
    policy::ScopedManagementServiceOverrideForTesting profile_management(
        policy::ManagementServiceFactory::GetForProfile(profile()),
        policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(u"Your browser is managed by your organization",
              GetManagementPageSubtitle(profile()));
  }

  {
    // Simulate managed browser and profile.
    policy::ScopedManagementServiceOverrideForTesting profile_management(
        policy::ManagementServiceFactory::GetForProfile(profile()),
        policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(u"Your browser is managed by your organization",
              GetManagementPageSubtitle(profile()));
  }

  {
    // Simulate managed browser with known manager and profile.
    ScopedDeviceManagerForTesting device_manager("example.com");
    policy::ScopedManagementServiceOverrideForTesting profile_management(
        policy::ManagementServiceFactory::GetForProfile(profile()),
        policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(u"Your browser is managed by example.com",
              GetManagementPageSubtitle(profile()));
  }
}

TEST_F(ManagedUiSupervisedUserTest, GetManagementBubbleTitle) {
  ScopedDeviceManagerForTesting unknown_device_manager("");

  {
    // Simulate managed browser.
    policy::ScopedManagementServiceOverrideForTesting profile_management(
        policy::ManagementServiceFactory::GetForProfile(profile()),
        policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(l10n_util::GetStringUTF16(IDS_MANAGEMENT_DIALOG_BROWSER_MANAGED),
              GetManagementBubbleTitle(profile()));
  }

  {
    // Simulate managed browser and profile.
    policy::ScopedManagementServiceOverrideForTesting profile_management(
        policy::ManagementServiceFactory::GetForProfile(profile()),
        policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(l10n_util::GetStringUTF16(IDS_MANAGEMENT_DIALOG_BROWSER_MANAGED),
              GetManagementBubbleTitle(profile()));
  }

  {
    // Simulate managed browser with known manager and profile.
    ScopedDeviceManagerForTesting device_manager("example.com");
    policy::ScopedManagementServiceOverrideForTesting profile_management(
        policy::ManagementServiceFactory::GetForProfile(profile()),
        policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);
    EXPECT_EQ(l10n_util::GetStringFUTF16(
                  IDS_MANAGEMENT_DIALOG_BROWSER_MANAGED_BY, u"example.com"),
              GetManagementBubbleTitle(profile()));
  }
}
#endif  //  !BUILDFLAG(IS_CHROMEOS)

TEST_F(ManagedUiSupervisedUserTest, GetManagedUiWebUILabelSupervised) {
  if (ExpectManagedUiForSupervisedUsers()) {
    EXPECT_EQ(
        u"Your <a href=\"https://familylink.google.com/setting/resource/94\">"
        u"browser is managed</a> by your parent",
        GetManagedUiWebUILabel(profile()));
  } else {
    EXPECT_TRUE(GetManagedUiWebUILabel(profile()).empty());
  }
}
}  // namespace
