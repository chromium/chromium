// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/device_signals_consent/consent_dialog_coordinator.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"

class ConsentDialogUiTest : public InProcessBrowserTest {
 public:
  ConsentDialogUiTest() {}

  ConsentDialogUiTest(const ConsentDialogUiTest&) = delete;
  ConsentDialogUiTest& operator=(const ConsentDialogUiTest&) = delete;

  ~ConsentDialogUiTest() override = default;

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

  std::u16string GetDialogBodyTextFromProfile(Profile* profile) {
    return std::make_unique<ConsentDialogCoordinator>(browser(), profile)
        ->GetDialogBodyText();
  }

  std::u16string GetExpectedBodyText(
      std::optional<std::string> manager = std::nullopt) {
    return (!manager) ? l10n_util::GetStringUTF16(
                            IDS_DEVICE_SIGNALS_CONSENT_DIALOG_DEFAULT_BODY_TEXT)
                      : l10n_util::GetStringFUTF16(
                            IDS_DEVICE_SIGNALS_CONSENT_DIALOG_BODY_TEXT,
                            base::UTF8ToUTF16(manager.value()));
  }

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
};

IN_PROC_BROWSER_TEST_F(ConsentDialogUiTest, GetConsentDialogBodyTest) {
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

  EXPECT_EQ(GetExpectedBodyText(), GetDialogBodyTextFromProfile(profile.get()));
  EXPECT_EQ(GetExpectedBodyText("example.com"),
            GetDialogBodyTextFromProfile(profile_with_domain.get()));
  EXPECT_EQ(GetExpectedBodyText("hosteddomain.com"),
            GetDialogBodyTextFromProfile(profile_with_hosted_domain));
  // Enterprise management takes precedence over supervision in the management
  // UI.
  EXPECT_EQ(GetExpectedBodyText(),
            GetDialogBodyTextFromProfile(profile_supervised.get()));
}
