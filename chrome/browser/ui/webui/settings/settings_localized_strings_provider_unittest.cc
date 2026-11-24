// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_localized_strings_provider.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/raw_ref.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_identity.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/test_web_ui_data_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace settings {
namespace {

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::Pointee;

// ScopedResourceOverride allows overriding localized strings in the shared
// instance of the resource bundle, while restoring the bundle state on
// destruction.
class ScopedResourceOverride {
 public:
  ScopedResourceOverride()
      : bundle_(ui::ResourceBundle::GetSharedInstance()),
        app_locale_(g_browser_process->GetApplicationLocale()) {}

  ScopedResourceOverride(const ScopedResourceOverride&) = delete;
  ScopedResourceOverride& operator=(const ScopedResourceOverride&) = delete;

  ~ScopedResourceOverride() {
    // Reloading the resources will discard all overrides.
    bundle_->ReloadLocaleResources(app_locale_);
  }

  void OverrideLocaleStringResource(int string_id, const std::u16string& str) {
    bundle_->OverrideLocaleStringResource(string_id, str);
  }

 private:
  const raw_ref<ui::ResourceBundle> bundle_;
  const std::string app_locale_;
};

TEST(SettingsLocalizedStringsProviderTest,
     AutofillStringsEscapeHtmlAttributes) {
  // Override locale strings before creating `BrowserTaskEnvironment` (and
  // restore them after `BrowserTaskEnvironment` is destroyed) so that
  // `ui::ResourceBundle` is only mutated while no background worker threads are
  // running.
  ScopedResourceOverride resource_override;
  resource_override.OverrideLocaleStringResource(IDS_SETTINGS_OPENS_IN_NEW_TAB,
                                                 u"Opens in a \"new\" tab");
  resource_override.OverrideLocaleStringResource(
      IDS_AUTOFILL_GMAIL_OTP_OPT_IN_LEARN_MORE_A11Y_NAME,
      u"Learn more about \"Autofill\" verification codes");

  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  content::TestWebContentsFactory web_contents_factory;
  content::WebContents* web_contents =
      web_contents_factory.CreateWebContents(&profile);

  std::unique_ptr<content::TestWebUIDataSource> html_source =
      content::TestWebUIDataSource::Create(chrome::kChromeUISettingsHost);
  AddAutofillStringsForTesting(html_source->GetWebUIDataSource(), &profile,
                               web_contents);

  const base::DictValue& strings = html_source->GetLocalizedStrings();
  constexpr char kExpectedAriaDescription[] =
      "aria-description=\"Opens in a &quot;new&quot; tab\"";
  constexpr char kExpectedAriaLabel[] =
      "aria-label=\"Learn more about &quot;Autofill&quot; verification codes\"";

  EXPECT_THAT(strings.FindString("enableGmailOtpFillingDescription"),
              Pointee(AllOf(HasSubstr(kExpectedAriaDescription),
                            HasSubstr(kExpectedAriaLabel))));
  EXPECT_THAT(strings.FindString("gmailOtpRequiredStep1"),
              Pointee(HasSubstr(kExpectedAriaDescription)));
  EXPECT_THAT(strings.FindString("gmailOtpRequiredStep2"),
              Pointee(HasSubstr(kExpectedAriaDescription)));
  EXPECT_THAT(strings.FindString("cardBenefitsToggleSublabel"),
              Pointee(HasSubstr(kExpectedAriaDescription)));
  EXPECT_THAT(strings.FindString("autofillPayOverTimeSettingsSublabel"),
              Pointee(HasSubstr(kExpectedAriaDescription)));
}

TEST(SettingsLocalizedStringsProviderTest,
     GeicStringsSubstituteDeviceManagerDomain) {
  constexpr char kManagerDomain[] = "example.com";
  ScopedDeviceManagerForTesting device_manager(kManagerDomain);
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  std::unique_ptr<content::TestWebUIDataSource> html_source =
      content::TestWebUIDataSource::Create(chrome::kChromeUISettingsHost);
  AddGeicStringsForTesting(html_source->GetWebUIDataSource(), &profile);

  const base::DictValue& strings = html_source->GetLocalizedStrings();
  EXPECT_THAT(
      strings.FindString("geicIntroHeading"),
      Pointee(Eq(l10n_util::GetStringUTF8(IDS_SETTINGS_GEIC_INTRO_HEADING))));
  EXPECT_THAT(strings.FindString("geicDevicePolicyButton"),
              Pointee(Eq(l10n_util::GetStringUTF8(
                  IDS_SETTINGS_GEIC_ENABLED_BY_DEVICE_POLICY_BUTTON))));
  EXPECT_THAT(strings.FindString("geicSwitchConsiderChatHistory"),
              Pointee(Eq(l10n_util::GetStringUTF8(
                  IDS_SETTINGS_GEIC_SWITCH_CONSIDER_CHAT_HISTORY))));
  EXPECT_THAT(strings.FindString("glicSwitchToGeicConsider1"),
              Pointee(Eq(l10n_util::GetStringUTF8(
                  IDS_SETTINGS_GLIC_SWITCH_TO_GEIC_CONSIDER_1))));
  EXPECT_THAT(strings.FindString("glicSwitchToGeicConsider2"),
              Pointee(Eq(l10n_util::GetStringUTF8(
                  IDS_SETTINGS_GLIC_SWITCH_TO_GEIC_CONSIDER_2))));

  EXPECT_THAT(strings.FindString("geicIntroDescription"),
              Pointee(HasSubstr(kManagerDomain)));
  EXPECT_THAT(strings.FindString("geicDevicePolicyToggle"),
              Pointee(HasSubstr(kManagerDomain)));
  EXPECT_THAT(strings.FindString("geicDevicePolicyToggleSublabel"),
              Pointee(HasSubstr(kManagerDomain)));
  EXPECT_THAT(strings.FindString("geicDevicePolicyConsider1"),
              Pointee(HasSubstr(kManagerDomain)));
  EXPECT_THAT(strings.FindString("geicDevicePolicyConsider2"),
              Pointee(HasSubstr(kManagerDomain)));
  EXPECT_THAT(strings.FindString("glicSwitchToGeicToggle"),
              Pointee(HasSubstr(kManagerDomain)));
  EXPECT_THAT(strings.FindString("glicSwitchToGeicToggleSublabel"),
              Pointee(HasSubstr(kManagerDomain)));
}

TEST(SettingsLocalizedStringsProviderTest,
     GeicIntroDescriptionPrefersAccountManagerDomain) {
  constexpr char kDeviceDomain[] = "device.example.com";
  constexpr char kAccountDomain[] = "account.example.com";
  ScopedDeviceManagerForTesting device_manager(kDeviceDomain);
  content::BrowserTaskEnvironment task_environment;
  TestingProfile::Builder profile_builder;
  profile_builder.SetProfileName("user@account.example.com");
  std::unique_ptr<TestingProfile> profile = profile_builder.Build();
  policy::ScopedManagementServiceOverrideForTesting account_management(
      policy::ManagementServiceFactory::GetForProfile(profile.get()),
      policy::EnterpriseManagementAuthority::CLOUD);

  std::unique_ptr<content::TestWebUIDataSource> html_source =
      content::TestWebUIDataSource::Create(chrome::kChromeUISettingsHost);
  AddGeicStringsForTesting(html_source->GetWebUIDataSource(), profile.get());

  const base::DictValue& strings = html_source->GetLocalizedStrings();
  EXPECT_THAT(strings.FindString("geicIntroDescription"),
              Pointee(HasSubstr(kAccountDomain)));
  EXPECT_THAT(strings.FindString("geicDevicePolicyToggle"),
              Pointee(HasSubstr(kDeviceDomain)));
  EXPECT_THAT(strings.FindString("glicSwitchToGeicToggle"),
              Pointee(HasSubstr(kDeviceDomain)));
}

TEST(SettingsLocalizedStringsProviderTest,
     GeicStringsUseFallbackWhenDeviceManagerEmpty) {
  ScopedDeviceManagerForTesting device_manager("");
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  std::unique_ptr<content::TestWebUIDataSource> html_source =
      content::TestWebUIDataSource::Create(chrome::kChromeUISettingsHost);
  AddGeicStringsForTesting(html_source->GetWebUIDataSource(), &profile);

  const std::string fallback =
      l10n_util::GetStringUTF8(IDS_SETTINGS_GEIC_DEFAULT_MANAGEMENT_DOMAIN);
  const base::DictValue& strings = html_source->GetLocalizedStrings();
  EXPECT_THAT(strings.FindString("geicIntroDescription"),
              Pointee(HasSubstr(fallback)));
  EXPECT_THAT(strings.FindString("geicDevicePolicyToggle"),
              Pointee(HasSubstr(fallback)));
  EXPECT_THAT(strings.FindString("geicDevicePolicyToggleSublabel"),
              Pointee(HasSubstr(fallback)));
  EXPECT_THAT(strings.FindString("geicDevicePolicyConsider1"),
              Pointee(HasSubstr(fallback)));
  EXPECT_THAT(strings.FindString("geicDevicePolicyConsider2"),
              Pointee(HasSubstr(fallback)));
  EXPECT_THAT(strings.FindString("glicSwitchToGeicToggle"),
              Pointee(HasSubstr(fallback)));
  EXPECT_THAT(strings.FindString("glicSwitchToGeicToggleSublabel"),
              Pointee(HasSubstr(fallback)));
}

void SetGeminiEnterpriseUrlPolicy(
    policy::MockConfigurationPolicyProvider& policy_provider,
    policy::PolicyScope scope) {
  policy_provider.SetDefaultReturns(
      /*is_initialization_complete_return=*/true,
      /*is_first_policy_load_complete_return=*/true);
  policy::PolicyMap policies;
  policies.Set(
      policy::key::kGeminiEnterpriseUrl, policy::POLICY_LEVEL_MANDATORY, scope,
      policy::POLICY_SOURCE_CLOUD,
      base::Value("https://business.gemini.google/side-panel"), nullptr);
  policy_provider.UpdateChromePolicy(policies);
}

TEST(SettingsLocalizedStringsProviderTest,
     GeicIntroDescriptionUsesDeviceManagerForMachineScopedPolicy) {
  constexpr char kDeviceDomain[] = "device.example.com";
  constexpr char kAccountDomain[] = "account.example.com";
  ScopedDeviceManagerForTesting device_manager(kDeviceDomain);
  content::BrowserTaskEnvironment task_environment;
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider;
  SetGeminiEnterpriseUrlPolicy(policy_provider, policy::POLICY_SCOPE_MACHINE);

  {
    policy::PolicyServiceImpl::Providers providers = {&policy_provider};
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName("user@account.example.com");
    profile_builder.SetPolicyService(
        std::make_unique<policy::PolicyServiceImpl>(std::move(providers)));
    std::unique_ptr<TestingProfile> profile = profile_builder.Build();
    policy::ScopedManagementServiceOverrideForTesting account_management(
        policy::ManagementServiceFactory::GetForProfile(profile.get()),
        policy::EnterpriseManagementAuthority::CLOUD);

    std::unique_ptr<content::TestWebUIDataSource> html_source =
        content::TestWebUIDataSource::Create(chrome::kChromeUISettingsHost);
    AddGeicStringsForTesting(html_source->GetWebUIDataSource(), profile.get());

    const base::DictValue& strings = html_source->GetLocalizedStrings();
    EXPECT_THAT(strings.FindString("geicIntroDescription"),
                Pointee(HasSubstr(kDeviceDomain)));
    EXPECT_THAT(strings.FindString("geicIntroDescription"),
                Pointee(Not(HasSubstr(kAccountDomain))));
  }
  policy_provider.Shutdown();
}

TEST(SettingsLocalizedStringsProviderTest,
     GeicIntroDescriptionUsesAccountManagerForUserScopedPolicy) {
  constexpr char kDeviceDomain[] = "device.example.com";
  constexpr char kAccountDomain[] = "account.example.com";
  ScopedDeviceManagerForTesting device_manager(kDeviceDomain);
  content::BrowserTaskEnvironment task_environment;
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider;
  SetGeminiEnterpriseUrlPolicy(policy_provider, policy::POLICY_SCOPE_USER);

  {
    policy::PolicyServiceImpl::Providers providers = {&policy_provider};
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName("user@account.example.com");
    profile_builder.SetPolicyService(
        std::make_unique<policy::PolicyServiceImpl>(std::move(providers)));
    std::unique_ptr<TestingProfile> profile = profile_builder.Build();
    policy::ScopedManagementServiceOverrideForTesting account_management(
        policy::ManagementServiceFactory::GetForProfile(profile.get()),
        policy::EnterpriseManagementAuthority::CLOUD);

    std::unique_ptr<content::TestWebUIDataSource> html_source =
        content::TestWebUIDataSource::Create(chrome::kChromeUISettingsHost);
    AddGeicStringsForTesting(html_source->GetWebUIDataSource(), profile.get());

    const base::DictValue& strings = html_source->GetLocalizedStrings();
    EXPECT_THAT(strings.FindString("geicIntroDescription"),
                Pointee(HasSubstr(kAccountDomain)));
    EXPECT_THAT(strings.FindString("geicIntroDescription"),
                Pointee(Not(HasSubstr(kDeviceDomain))));
  }
  policy_provider.Shutdown();
}

TEST(SettingsLocalizedStringsProviderTest,
     GeicIntroDescriptionUsesFallbackForUserScopedPolicyWithoutAccountManager) {
  constexpr char kDeviceDomain[] = "device.example.com";
  ScopedDeviceManagerForTesting device_manager(kDeviceDomain);
  content::BrowserTaskEnvironment task_environment;
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider;
  SetGeminiEnterpriseUrlPolicy(policy_provider, policy::POLICY_SCOPE_USER);

  {
    policy::PolicyServiceImpl::Providers providers = {&policy_provider};
    TestingProfile::Builder profile_builder;
    profile_builder.SetPolicyService(
        std::make_unique<policy::PolicyServiceImpl>(std::move(providers)));
    std::unique_ptr<TestingProfile> profile = profile_builder.Build();

    std::unique_ptr<content::TestWebUIDataSource> html_source =
        content::TestWebUIDataSource::Create(chrome::kChromeUISettingsHost);
    AddGeicStringsForTesting(html_source->GetWebUIDataSource(), profile.get());

    const base::DictValue& strings = html_source->GetLocalizedStrings();
    EXPECT_THAT(strings.FindString("geicIntroDescription"),
                Pointee(Eq(l10n_util::GetStringFUTF8(
                    IDS_SETTINGS_GEIC_INTRO_DESCRIPTION,
                    l10n_util::GetStringUTF16(
                        IDS_SETTINGS_GEIC_DEFAULT_MANAGEMENT_DOMAIN)))));
    EXPECT_THAT(strings.FindString("geicIntroDescription"),
                Pointee(Not(HasSubstr(kDeviceDomain))));
  }
  policy_provider.Shutdown();
}

}  // namespace
}  // namespace settings
