// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/views/device_signals_consent/consent_dialog_coordinator.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/supervised_user/core/browser/supervised_user_test_environment.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include "base/functional/callback_helpers.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/profiles/profile_ui_test_utils.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"
#endif

class ConsentDialogUiTest : public InProcessBrowserTest {
 public:
  ConsentDialogUiTest() = default;

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

#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(ConsentDialogUiTest,
                       CancelKeepsProfilePickerOutOfShutdown) {
  Profile* profile = browser()->GetProfile();
  ScopedProfileKeepAlive profile_keep_alive(
      profile, ProfileKeepAliveOrigin::kBrowserWindow);
  base::test::TestFuture<views::Widget*> widget_shown;
  views::AnyWidgetObserver widget_observer(views::test::AnyWidgetTestPasskey{});
  widget_observer.set_shown_callback(widget_shown.GetRepeatingCallback());
  auto coordinator =
      std::make_unique<ConsentDialogCoordinator>(browser(), profile);
  coordinator->RequestConsent(base::DoNothing());
  views::Widget* widget = widget_shown.Get();
  widget_observer.set_shown_callback({});
  ASSERT_TRUE(widget);
  auto* dialog = widget->widget_delegate()->AsDialogDelegate();
  ASSERT_TRUE(dialog);

  dialog->CancelDialog();
  coordinator.reset();
  ASSERT_TRUE(base::test::RunUntil(
      []() { return GlobalBrowserCollection::GetInstance()->IsEmpty(); }));
  ASSERT_TRUE(ProfilePicker::IsOpen());
  profiles::testing::WaitForPickerWidgetCreated();

  EXPECT_FALSE(browser_shutdown::HasShutdownStarted());
  EXPECT_EQ(BrowserWindowInterface::CreationStatus::kOk,
            GetBrowserWindowCreationStatusForProfile(*profile));
}
#endif

// TODO(crbug.com/416157468): Enable the test.
#if BUILDFLAG(IS_WIN)
#define MAYBE_GetConsentDialogBodyTest DISABLED_GetConsentDialogBodyTest
#else
#define MAYBE_GetConsentDialogBodyTest GetConsentDialogBodyTest
#endif
IN_PROC_BROWSER_TEST_F(ConsentDialogUiTest, MAYBE_GetConsentDialogBodyTest) {
  // Simulate a managed profile.
  AddEnterpriseManagedPolicies();
  policy::ScopedManagementServiceOverrideForTesting browser_management(
      policy::ManagementServiceFactory::GetForProfile(browser()->GetProfile()),
      policy::EnterpriseManagementAuthority::CLOUD);

  TestingProfile::Builder builder;
  auto profile = builder.Build();

  TestingProfile::Builder builder_with_domain;
  builder_with_domain.SetProfileName("foobar@example.com");
  builder_with_domain.OverridePolicyConnectorIsManagedForTesting(true);
  auto profile_with_domain = builder_with_domain.Build();

  auto* profile_with_hosted_domain = browser()->GetProfile();
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_with_hosted_domain->GetPath());
  ASSERT_TRUE(entry);
  entry->SetIsManaged(signin::Tribool::kTrue);
  entry->SetHostedDomain("hosteddomain.com");

  // Simulate a supervised profile.
  TestingProfile::Builder builder_supervised;
  builder_supervised.AddTestingFactories(
      IdentityTestEnvironmentProfileAdaptor::
          GetIdentityTestEnvironmentFactories());
  std::unique_ptr<TestingProfile> profile_supervised =
      builder_supervised.Build();
  supervised_user::SupervisedUserTestEnvironment::EnableSupervisedAccount(
      IdentityManagerFactory::GetForProfile(profile_supervised.get()));

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
