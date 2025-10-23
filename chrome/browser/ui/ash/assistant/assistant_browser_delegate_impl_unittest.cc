// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/assistant/assistant_browser_delegate_impl.h"

#include <memory>
#include <optional>
#include <set>
#include <string>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/ash/browser_delegate/browser_controller_impl.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/login/users/scoped_account_id_annotator.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/web_applications/test/fake_web_app_database_factory.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_app_ui_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_management_type.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_browser_delegate.h"
#include "chromeos/services/assistant/public/shared/constants.h"
#include "components/account_id/account_id.h"
#include "components/account_id/account_id_literal.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/fake_user_manager_delegate.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/test_helper.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_impl.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr auto kAccountId =
    AccountId::Literal::FromUserEmailGaiaId("test@test",
                                            GaiaId::Literal("123456789"));

}  // namespace

class AssistantBrowserDelegateImplTest : public ChromeAshTestBase {
 public:
  AssistantBrowserDelegateImplTest() { set_start_session(false); }
  ~AssistantBrowserDelegateImplTest() override = default;

  void SetUp() override {
    user_manager_.Reset(std::make_unique<user_manager::UserManagerImpl>(
        std::make_unique<user_manager::FakeUserManagerDelegate>(),
        TestingBrowserProcess::GetGlobal()->GetTestingLocalState(),
        /*cros_settings=*/nullptr));
    ASSERT_TRUE(user_manager::TestHelper(user_manager_.Get())
                    .AddRegularUser(kAccountId));

    ChromeAshTestBase::SetUp();
    browser_controller_.emplace();

    // Note: this is not following the production behavior because of test
    // utility. SessionManager is instantiated and as a part of
    // ChromeAshTestBase, but UserManager is not.
    session_manager::SessionManager* session_manager =
        session_manager::SessionManager::Get();
    session_manager->OnUserManagerCreated(user_manager_.Get());

    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());

    delegate_.emplace();
    delegate_->SetGoogleChromeBuildForTesting();

    // Simulate the log-in flow.
    // Note: this is not following the production behavior, either, because
    // of test utility. SimulateUserLogin requires PrefService to dispatch
    // the other parts of Ash systems that AshTestHelper supports. However,
    // in production, the PrefService creation (i.e. Profile creation) happens
    // after the session creation that SimulateUserLogin gives it a try to
    // simulate. Unfortunately, the test utility does not provide a way
    // to simulate the production behavior yet. In this test, the preference
    // distributed to the AshTestHelper and the one held by Profile are
    // different. Fortunately, it does not impact to the test correctness now.
    SimulateUserLogin(kAccountId);

    // Also, AshTestHelper does not respect real SessionManager via
    // SimulateUserLogin. Call it manually here, too.
    session_manager->CreateSession(
        kAccountId,
        user_manager::FakeUserManager::GetFakeUsernameHash(kAccountId),
        /*new_user=*/false,
        /*has_active_session=*/false);

    // Profile creation for the logged in user.
    {
      ash::ScopedAccountIdAnnotator account_id_annotator(
          profile_manager_->profile_manager(), kAccountId);
      profile_ = profile_manager_->CreateTestingProfile(
          std::string(kAccountId.GetUserEmail()));
    }
    user_manager_->OnUserProfileCreated(kAccountId, profile_->GetPrefs());

    web_app::FakeWebAppProvider::Get(profile_.get())
        ->SetDatabaseFactory(
            std::make_unique<web_app::FakeWebAppDatabaseFactory>());

    // Everything is ready, and so notify to start the session.
    session_manager->SessionStarted();
  }

  void TearDown() override {
    user_manager_->OnUserProfileWillBeDestroyed(kAccountId);
    profile_ = nullptr;
    delegate_.reset();
    profile_manager_.reset();
    browser_controller_.reset();
    ChromeAshTestBase::TearDown();
    user_manager_.Reset();
  }

 protected:
  Profile* profile() const { return profile_.get(); }

  web_app::FakeWebAppProvider& fake_provider() const {
    return *web_app::FakeWebAppProvider::Get(profile());
  }

  web_app::FakeWebAppDatabaseFactory& database_factory() const {
    return static_cast<web_app::FakeWebAppDatabaseFactory&>(
        fake_provider().GetDatabaseFactory());
  }

  void InstallNewEntryPointApp() {
    const GURL start_url = GURL("https://example.com/path");
    const webapps::AppId app_id =
        web_app::GenerateAppId(/*manifest_id_path=*/std::nullopt, start_url);
    auto web_app = std::make_unique<web_app::WebApp>(app_id);
    web_app->SetStartUrl(start_url);
    web_app->SetName("test app");
    web_app->AddSource(web_app::WebAppManagement::kSystem);
    web_app->SetDisplayMode(web_app::DisplayMode::kStandalone);
    web_app->SetUserDisplayMode(web_app::mojom::UserDisplayMode::kStandalone);

    web_app::Registry registry;
    registry[app_id] = std::move(web_app);
    database_factory().WriteRegistry(registry);

    delegate_->OverrideEntryPointIdForTesting(app_id);
  }

  user_manager::ScopedUserManager user_manager_{
      std::make_unique<user_manager::UserManagerImpl>(
          std::make_unique<user_manager::FakeUserManagerDelegate>(),
          TestingBrowserProcess::GetGlobal()->GetTestingLocalState(),
          /*cros_settings=*/nullptr)};
  std::optional<ash::BrowserControllerImpl> browser_controller_;
  std::unique_ptr<TestingProfileManager> profile_manager_;

  raw_ptr<Profile> profile_;
  std::optional<AssistantBrowserDelegateImpl> delegate_;
};

TEST_F(AssistantBrowserDelegateImplTest, NewEntryPointOpensApp) {
  InstallNewEntryPointApp();
  web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());

  auto* provider = web_app::WebAppProvider::GetForWebApps(profile());
  base::test::TestFuture<apps::AppLaunchParams,
                         web_app::LaunchWebAppWindowSetting>
      app_launch_future;
  static_cast<web_app::FakeWebAppUiManager*>(&provider->ui_manager())
      ->SetOnLaunchWebAppCallback(app_launch_future.GetRepeatingCallback());

  // Try opening new entry point.
  delegate_->OpenNewEntryPoint();

  // New entry point should open.
  ASSERT_TRUE(app_launch_future.Wait())
      << "New entry point should launch, but didn't.";
}

TEST_F(AssistantBrowserDelegateImplTest, EligibleToNewEntryPoint) {
  base::HistogramTester histogram_tester;
  InstallNewEntryPointApp();
  web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());

  base::expected<bool, ash::assistant::AssistantBrowserDelegate::Error>
      maybe_eligibility = delegate_->IsNewEntryPointEligibleForPrimaryProfile();
  ASSERT_TRUE(maybe_eligibility.has_value());
  EXPECT_TRUE(maybe_eligibility.value());
  // sample=0 is kEligible.
  histogram_tester.ExpectUniqueSample("Assistant.NewEntryPoint.Eligibility",
                                      /*sample=*/0,
                                      /*expected_bucket_count=*/1);
}

TEST_F(AssistantBrowserDelegateImplTest, NewEntryPointDoNotOpenIfNotInstalled) {
  web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());

  auto* provider = web_app::WebAppProvider::GetForWebApps(profile());
  base::test::TestFuture<apps::AppLaunchParams,
                         web_app::LaunchWebAppWindowSetting>
      app_launch_future;
  static_cast<web_app::FakeWebAppUiManager*>(&provider->ui_manager())
      ->SetOnLaunchWebAppCallback(app_launch_future.GetRepeatingCallback());

  // Try opening new entry point without installing app.
  delegate_->OpenNewEntryPoint();

  // Wait for things to settle to check if entry point is launched.
  task_environment()->RunUntilIdle();

  // New entry point should not open.
  ASSERT_FALSE(app_launch_future.IsReady())
      << "New entry point should not launch, but somehow it did.";
}

TEST_F(AssistantBrowserDelegateImplTest, NotEligibleBecauseOfNoEntryPointApp) {
  base::HistogramTester histogram_tester;
  web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());

  base::expected<bool, ash::assistant::AssistantBrowserDelegate::Error>
      maybe_eligibility = delegate_->IsNewEntryPointEligibleForPrimaryProfile();
  ASSERT_TRUE(maybe_eligibility.has_value());
  EXPECT_FALSE(maybe_eligibility.value());
  // sample=4 is kNotEligibleNotInstalled.
  histogram_tester.ExpectUniqueSample("Assistant.NewEntryPoint.Eligibility",
                                      /*sample=*/4,
                                      /*expected_bucket_count=*/1);
}

TEST_F(AssistantBrowserDelegateImplTest, NewEntryPointName) {
  InstallNewEntryPointApp();
  web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());

  EXPECT_EQ("test app", delegate_->GetNewEntryPointName());
}

TEST_F(AssistantBrowserDelegateImplTest,
       NoEntryPointNameBecauseOfNoEntryPointApp) {
  web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());

  EXPECT_EQ(std::nullopt, delegate_->GetNewEntryPointName());
}
