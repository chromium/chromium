// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/projector/projector_client_impl.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/projector/test/mock_projector_client.h"
#include "ash/public/cpp/projector/projector_client.h"
#include "ash/public/cpp/projector/projector_new_screencast_precondition.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/drivefs_test_support.h"
#include "chrome/browser/ash/login/test/fake_gaia_mixin.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/projector/projector_app_client_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_types.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/account_id/account_id.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_type.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {

namespace {

apps::AppServiceProxy* GetAppServiceProxy(Profile* profile) {
  return apps::AppServiceProxyFactory::GetForProfile(profile);
}

// Returns the account id for logging in.
absl::optional<AccountId> GetPrimaryAccountId(bool is_managed) {
  if (is_managed) {
    return AccountId::FromUserEmailGaiaId(
        ash::FakeGaiaMixin::kEnterpriseUser1,
        ash::FakeGaiaMixin::kEnterpriseUser1GaiaId);
  }
  // Use the default FakeGaiaMixin::kFakeUserEmail consumer test account id.
  return absl::nullopt;
}

}  // namespace

// A class helps to verify enable/disable Drive could invoke
// ProjectorAppClient::Observer::OnDriveFsMountStatusChanged().
class DriveFsMountStatusWaiter : public ProjectorAppClient::Observer {
 public:
  explicit DriveFsMountStatusWaiter(drive::DriveIntegrationService* service)
      : service_(service) {
    GetProjectorAppClientImpl()->AddObserver(this);
  }

  DriveFsMountStatusWaiter(const DriveFsMountStatusWaiter&) = delete;
  DriveFsMountStatusWaiter& operator=(const DriveFsMountStatusWaiter&) = delete;

  ~DriveFsMountStatusWaiter() override {
    GetProjectorAppClientImpl()->RemoveObserver(this);
  }

  // ProjectorAppClient::Observer:
  void OnNewScreencastPreconditionChanged(
      const NewScreencastPrecondition& condition) override {
    std::move(quit_closure_).Run();
  }
  MOCK_METHOD1(OnScreencastsPendingStatusChanged,
               void(const PendingScreencastSet&));
  MOCK_METHOD1(OnSodaProgress, void(int));
  MOCK_METHOD0(OnSodaError, void());
  MOCK_METHOD0(OnSodaInstalled, void());

  void SetDriveEnabled(bool enabled_drive, base::OnceClosure quit_closure) {
    quit_closure_ = std::move(quit_closure);
    service_->SetEnabled(enabled_drive);
  }

  ProjectorAppClientImpl* GetProjectorAppClientImpl() {
    return static_cast<ProjectorAppClientImpl*>(ash::ProjectorAppClient::Get());
  }

 private:
  base::OnceClosure quit_closure_;
  drive::DriveIntegrationService* service_;
};

class ProjectorClientTest : public InProcessBrowserTest {
 public:
  ProjectorClientTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kProjector, features::kProjectorAnnotator,
         features::kOnDeviceSpeechRecognition},
        {});
  }

  ~ProjectorClientTest() override = default;
  ProjectorClientTest(const ProjectorClientTest&) = delete;
  ProjectorClientTest& operator=(const ProjectorClientTest&) = delete;

  // InProcessBrowserTest:
  void SetUpInProcessBrowserTestFixture() override {
    create_drive_integration_service_ =
        base::BindRepeating(&ProjectorClientTest::CreateDriveIntegrationService,
                            base::Unretained(this));
    service_factory_for_test_ = std::make_unique<
        drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>(
        &create_drive_integration_service_);
  }

  // This test helper verifies that navigating to the |url| doesn't result in a
  // 404 error.
  void VerifyUrlValid(const char* url) {
    GURL gurl(url);
    EXPECT_TRUE(gurl.is_valid()) << "url isn't valid: " << url;
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl))
        << "navigating to url failed: " << url;
    content::WebContents* tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_EQ(tab->GetController().GetLastCommittedEntry()->GetPageType(),
              content::PAGE_TYPE_NORMAL)
        << "page has unexpected errors: " << url;
  }

  drive::DriveIntegrationService* CreateDriveIntegrationService(
      Profile* profile) {
    base::FilePath mount_path = profile->GetPath().Append("drivefs");
    fake_drivefs_helpers_[profile] =
        std::make_unique<drive::FakeDriveFsHelper>(profile, mount_path);
    // The integration service is owned by `KeyedServiceFactory`.
    auto* integration_service = new drive::DriveIntegrationService(
        profile, /*test_mount_point_name=*/std::string(), mount_path,
        fake_drivefs_helpers_[profile]->CreateFakeDriveFsListenerFactory());
    return integration_service;
  }

  ProjectorClient* client() { return ProjectorClient::Get(); }

 private:
  drive::DriveIntegrationServiceFactory::FactoryCallback
      create_drive_integration_service_;
  std::unique_ptr<drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>
      service_factory_for_test_;
  std::map<Profile*, std::unique_ptr<drive::FakeDriveFsHelper>>
      fake_drivefs_helpers_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

// This test verifies that the (un)trusted Projector app and annotator WebUI
// URLs are valid.
IN_PROC_BROWSER_TEST_F(ProjectorClientTest, AppUrlsValid) {
  VerifyUrlValid(kChromeUITrustedProjectorAppUrl);
  VerifyUrlValid(kChromeUIUntrustedProjectorAppUrl);
  VerifyUrlValid(kChromeUITrustedAnnotatorAppUrl);
  VerifyUrlValid(kChromeUIUntrustedAnnotatorAppUrl);
}

IN_PROC_BROWSER_TEST_F(ProjectorClientTest, OpenProjectorApp) {
  auto* profile = browser()->profile();
  web_app::WebAppProvider::GetForTest(profile)
      ->system_web_app_manager()
      .InstallSystemAppsForTesting();

  client()->OpenProjectorApp();
  web_app::FlushSystemWebAppLaunchesForTesting(profile);

  // Verify that Projector App is opened.
  Browser* app_browser =
      FindSystemWebAppBrowser(profile, web_app::SystemAppType::PROJECTOR);
  ASSERT_TRUE(app_browser);
  content::WebContents* tab =
      app_browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);
  EXPECT_EQ(tab->GetController().GetVisibleEntry()->GetPageType(),
            content::PAGE_TYPE_NORMAL);
}

IN_PROC_BROWSER_TEST_F(ProjectorClientTest, MinimizeProjectorApp) {
  auto* profile = browser()->profile();
  web_app::WebAppProvider::GetForTest(profile)
      ->system_web_app_manager()
      .InstallSystemAppsForTesting();

  client()->OpenProjectorApp();
  web_app::FlushSystemWebAppLaunchesForTesting(profile);

  // Verify that Projector App is opened.
  Browser* app_browser =
      FindSystemWebAppBrowser(profile, web_app::SystemAppType::PROJECTOR);
  ASSERT_TRUE(app_browser);
  content::WebContents* tab =
      app_browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);
  EXPECT_EQ(tab->GetController().GetVisibleEntry()->GetPageType(),
            content::PAGE_TYPE_NORMAL);

  client()->MinimizeProjectorApp();
  // Verify that Projector App is minimized.
  EXPECT_TRUE(app_browser->window()->IsMinimized());
}

IN_PROC_BROWSER_TEST_F(ProjectorClientTest, CloseProjectorApp) {
  auto* profile = browser()->profile();
  web_app::WebAppProvider::GetForTest(profile)
      ->system_web_app_manager()
      .InstallSystemAppsForTesting();

  client()->OpenProjectorApp();
  web_app::FlushSystemWebAppLaunchesForTesting(profile);

  // Verify that Projector App is opened.
  Browser* app_browser =
      FindSystemWebAppBrowser(profile, web_app::SystemAppType::PROJECTOR);
  ASSERT_TRUE(app_browser);
  content::WebContents* tab =
      app_browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);
  EXPECT_EQ(tab->GetController().GetVisibleEntry()->GetPageType(),
            content::PAGE_TYPE_NORMAL);

  EXPECT_FALSE(app_browser->IsAttemptingToCloseBrowser());
  client()->CloseProjectorApp();
  // Verify that Projector App is closing.
  EXPECT_TRUE(app_browser->IsAttemptingToCloseBrowser());
}

IN_PROC_BROWSER_TEST_F(ProjectorClientTest, GetDriveFsMountPointPath) {
  ASSERT_TRUE(client()->IsDriveFsMounted());
  ASSERT_FALSE(client()->IsDriveFsMountFailed());

  base::FilePath mounted_path;
  ASSERT_TRUE(client()->GetDriveFsMountPointPath(&mounted_path));
  ASSERT_EQ(browser()->profile()->GetPath().Append("drivefs"), mounted_path);
}

IN_PROC_BROWSER_TEST_F(ProjectorClientTest, DriveUnmountedAndRemounted) {
  drive::DriveIntegrationService* service =
      drive::DriveIntegrationServiceFactory::FindForProfile(
          browser()->profile());
  EXPECT_TRUE(service->is_enabled());

  DriveFsMountStatusWaiter observer{service};

  {
    base::RunLoop run_loop;
    observer.SetDriveEnabled(
        /*enabled_drive=*/false, run_loop.QuitClosure());
    run_loop.Run();
  }
  {
    base::RunLoop run_loop;
    observer.SetDriveEnabled(
        /*enabled_drive=*/true, run_loop.QuitClosure());
    run_loop.Run();
  }
}

// Tests Projector client for child and managed users.
class ProjectorClientManagedTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface</*IsChild=*/bool> {
 protected:
  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    logged_in_user_mixin_.LogInUser();
  }

  bool is_child() const { return GetParam(); }

  bool is_managed() const { return !is_child(); }

  ProjectorClient* client() { return ProjectorClient::Get(); }

  std::string GetPolicy() {
    if (is_child())
      return ash::prefs::kProjectorDogfoodForFamilyLinkEnabled;
    return ash::prefs::kProjectorAllowByPolicy;
  }

  apps::Readiness GetAppReadiness(const web_app::AppId& app_id) {
    apps::Readiness readiness;
    bool app_found =
        GetAppServiceProxy(browser()->profile())
            ->AppRegistryCache()
            .ForOneApp(app_id, [&readiness](const apps::AppUpdate& update) {
              readiness = update.Readiness();
            });
    EXPECT_TRUE(app_found);
    return readiness;
  }

  absl::optional<apps::IconKey> GetAppIconKey(const web_app::AppId& app_id) {
    absl::optional<apps::IconKey> icon_key;
    bool app_found =
        GetAppServiceProxy(browser()->profile())
            ->AppRegistryCache()
            .ForOneApp(app_id, [&icon_key](const apps::AppUpdate& update) {
              icon_key = update.IconKey();
            });
    EXPECT_TRUE(app_found);
    return icon_key;
  }

 private:
  LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_,
      is_child() ? LoggedInUserMixin::LogInType::kChild
                 : LoggedInUserMixin::LogInType::kRegular,
      embedded_test_server(),
      this,
      /*should_launch_browser=*/true,
      GetPrimaryAccountId(is_managed())};
};

IN_PROC_BROWSER_TEST_P(ProjectorClientManagedTest,
                       CantOpenProjectorAppWithoutPolicy) {
  auto* profile = browser()->profile();
  web_app::WebAppProvider::GetForTest(profile)
      ->system_web_app_manager()
      .InstallSystemAppsForTesting();

  client()->OpenProjectorApp();
  web_app::FlushSystemWebAppLaunchesForTesting(profile);

  // Verify that Projector App is not opened.
  Browser* app_browser =
      FindSystemWebAppBrowser(profile, web_app::SystemAppType::PROJECTOR);
  EXPECT_FALSE(app_browser);
}

// Prevents a regression to b/230779397.
IN_PROC_BROWSER_TEST_P(ProjectorClientManagedTest, DisableThenEnablePolicy) {
  auto* profile = browser()->profile();
  profile->GetPrefs()->SetBoolean(GetPolicy(), true);
  web_app::WebAppProvider::GetForTest(profile)
      ->system_web_app_manager()
      .InstallSystemAppsForTesting();

  client()->OpenProjectorApp();
  web_app::FlushSystemWebAppLaunchesForTesting(profile);

  // Verify the user can open the Projector App when the policy is enabled.
  Browser* app_browser =
      FindSystemWebAppBrowser(profile, web_app::SystemAppType::PROJECTOR);
  ASSERT_TRUE(app_browser);
  content::WebContents* tab =
      app_browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab);
  EXPECT_EQ(tab->GetController().GetVisibleEntry()->GetPageType(),
            content::PAGE_TYPE_NORMAL);

  // Suppose the policy flips to false while the user is still signed in and has
  // the Projector app open.
  profile->GetPrefs()->SetBoolean(GetPolicy(), false);
  // The Projector app immediately closes to prevent further access.
  EXPECT_TRUE(app_browser->IsAttemptingToCloseBrowser());
  // We can't uninstall the Projector SWA until the next session, but the icon
  // is greyed out and disabled.
  EXPECT_EQ(apps::Readiness::kDisabledByPolicy,
            GetAppReadiness(kChromeUITrustedProjectorSwaAppId));
  EXPECT_TRUE(apps::IconEffects::kBlocked &
              GetAppIconKey(kChromeUITrustedProjectorSwaAppId)->icon_effects);

  // The app can re-enable too if it's already installed and the policy flips to
  // true.
  profile->GetPrefs()->SetBoolean(GetPolicy(), true);
  EXPECT_EQ(apps::Readiness::kReady,
            GetAppReadiness(kChromeUITrustedProjectorSwaAppId));
  EXPECT_FALSE(apps::IconEffects::kBlocked &
               GetAppIconKey(kChromeUITrustedProjectorSwaAppId)->icon_effects);
}

INSTANTIATE_TEST_SUITE_P(,
                         ProjectorClientManagedTest,
                         /*IsChild=*/testing::Bool());

}  // namespace ash
