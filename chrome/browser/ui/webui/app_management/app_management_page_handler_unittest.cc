// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/app_management/app_management_page_handler.h"

#include <memory>
#include <vector>

#include "base/test/test_future.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom-shared.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/webui/resources/cr_components/app_management/app_management.mojom.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/test/fake_app_instance.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/apps/apk_web_app_service.h"
#include "components/arc/test/fake_intent_helper_instance.h"
#endif

using ::testing::Contains;
using ::testing::ElementsAre;

namespace apps {
namespace {
class TestDelegate : public AppManagementPageHandler::Delegate {
 public:
  TestDelegate() = default;
  TestDelegate(const TestDelegate&) = delete;
  TestDelegate& operator=(const TestDelegate&) = delete;

  // AppManagementPageHandler::Delegate:

  ~TestDelegate() override = default;

  gfx::NativeWindow GetUninstallAnchorWindow() const override {
    return gfx::NativeWindow();
  }
};
}  // namespace
class AppManagementPageHandlerTestBase : public testing::Test {
 public:
  void SetUp() override {
    testing::Test::SetUp();

    profile_ = std::make_unique<TestingProfile>();
    delegate_ = std::unique_ptr<TestDelegate>();

    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());

    mojo::PendingReceiver<app_management::mojom::Page> page;
    mojo::Remote<app_management::mojom::PageHandler> handler;
    handler_ = std::make_unique<AppManagementPageHandler>(
        handler.BindNewPipeAndPassReceiver(),
        page.InitWithNewPipeAndPassRemote(), profile_.get(), *delegate_.get());
  }

  Profile* profile() { return profile_.get(); }
  AppManagementPageHandler* handler() { return handler_.get(); }

 protected:
  void AwaitWebAppCommandsComplete() {
    web_app::WebAppProvider* provider =
        web_app::WebAppProvider::GetForTest(profile());
    provider->command_manager().AwaitAllCommandsCompleteForTesting();
  }

  bool IsAppPreferred(const web_app::AppId& app_id) {
    base::test::TestFuture<app_management::mojom::AppPtr> result;
    handler()->GetApp(app_id, result.GetCallback());
    return result.Get()->is_preferred_app;
  }

  std::vector<std::string> GetOverlappingPreferredApps(
      const web_app::AppId& app_id) {
    base::test::TestFuture<const std::vector<std::string>&> result;
    handler()->GetOverlappingPreferredApps(app_id, result.GetCallback());
    EXPECT_TRUE(result.Wait());
    return result.Get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<TestDelegate> delegate_;
  std::unique_ptr<AppManagementPageHandler> handler_;
};

TEST_F(AppManagementPageHandlerTestBase, GetApp) {
  // Create a web app entry with scope, which would be recognised
  // as normal web app in the web app system.
  auto web_app_info = std::make_unique<web_app::WebAppInstallInfo>();
  web_app_info->title = u"app_name";
  web_app_info->start_url = GURL("https://example.com/");

  std::string app_id =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info));

  base::test::TestFuture<app_management::mojom::AppPtr> result;
  handler()->GetApp(app_id, result.GetCallback());

  EXPECT_EQ(result.Get()->id, app_id);
  EXPECT_EQ(result.Get()->title.value(), "app_name");
  EXPECT_EQ(result.Get()->type, AppType::kWeb);
}

TEST_F(AppManagementPageHandlerTestBase, GetPreferredAppTest) {
  auto web_app_info = std::make_unique<web_app::WebAppInstallInfo>();
  web_app_info->title = u"app_name";
  web_app_info->start_url = GURL("https://example.com/index.html");
  web_app_info->scope = GURL("https://example.com/abc/");

  std::string app_id =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info));

  EXPECT_FALSE(IsAppPreferred(app_id));

  handler()->SetPreferredApp(app_id, /*is_preferred_app=*/true);
  AwaitWebAppCommandsComplete();

  base::test::TestFuture<app_management::mojom::AppPtr> updated_result;
  handler()->GetApp(app_id, updated_result.GetCallback());
  EXPECT_TRUE(updated_result.Get()->is_preferred_app);

  EXPECT_THAT(updated_result.Get()->supported_links,
              testing::Contains("example.com/abc/*"));
}

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(AppManagementPageHandlerTestBase, SupportedLinksWithPort) {
  auto web_app_info = std::make_unique<web_app::WebAppInstallInfo>();
  web_app_info->title = u"app_name";
  web_app_info->start_url = GURL("https://example.com/index.html");
  web_app_info->scope = GURL("https://example:8080/abc/");

  std::string app_id =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info));
  base::test::TestFuture<app_management::mojom::AppPtr> result;
  handler()->GetApp(app_id, result.GetCallback());

  EXPECT_THAT(result.Get()->supported_links,
              testing::Contains("example:8080/abc/*"));
}

TEST_F(AppManagementPageHandlerTestBase, PreferredAppNonOverlappingScopePort) {
  auto web_app_info1 = std::make_unique<web_app::WebAppInstallInfo>();
  web_app_info1->title = u"App 1";
  web_app_info1->start_url = GURL("https://example.com/index.html");
  web_app_info1->scope = GURL("https://example:8080/");

  std::string app_id1 =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info1));

  auto web_app_info2 = std::make_unique<web_app::WebAppInstallInfo>();
  web_app_info2->title = u"App 2";
  web_app_info2->start_url = GURL("https://example.com/abc/index.html");
  web_app_info2->scope = GURL("https://example:9090/");

  std::string app_id2 =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info2));
  EXPECT_FALSE(IsAppPreferred(app_id1));
  EXPECT_FALSE(IsAppPreferred(app_id2));

  // app_id1 is set to preferred, app_id2 is not affected.
  handler()->SetPreferredApp(app_id1, /*is_preferred_app=*/true);
  AwaitWebAppCommandsComplete();
  EXPECT_TRUE(IsAppPreferred(app_id1));
  EXPECT_FALSE(IsAppPreferred(app_id2));

  // app_id2 is set as preferred, app_id1 is not affected.
  handler()->SetPreferredApp(app_id2, /*is_preferred_app=*/true);
  AwaitWebAppCommandsComplete();
  EXPECT_TRUE(IsAppPreferred(app_id1));
  EXPECT_TRUE(IsAppPreferred(app_id2));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

TEST_F(AppManagementPageHandlerTestBase, PreferredAppOverlappingScopePort) {
  auto web_app_info1 = std::make_unique<web_app::WebAppInstallInfo>();
  web_app_info1->title = u"App 1";
  web_app_info1->start_url = GURL("https://example.com/index.html");
  web_app_info1->scope = GURL("https://example:8080/");

  std::string app_id1 =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info1));

  auto web_app_info2 = std::make_unique<web_app::WebAppInstallInfo>();
  web_app_info2->title = u"App 2";
  web_app_info2->start_url = GURL("https://example.com/abc/index.html");
  web_app_info2->scope = GURL("https://example:8080/abc");

  std::string app_id2 =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info2));
  EXPECT_FALSE(IsAppPreferred(app_id1));
  EXPECT_FALSE(IsAppPreferred(app_id2));

  // Setting app_id1 as preferred should set app_id2 as not preferred.
  handler()->SetPreferredApp(app_id1, /*is_preferred_app=*/true);
  AwaitWebAppCommandsComplete();
  EXPECT_TRUE(IsAppPreferred(app_id1));
  EXPECT_FALSE(IsAppPreferred(app_id2));

  // Setting app_id2 as preferred should set app_id1 as not preferred.
  handler()->SetPreferredApp(app_id2, /*is_preferred_app=*/true);
  AwaitWebAppCommandsComplete();
  EXPECT_FALSE(IsAppPreferred(app_id1));
  EXPECT_TRUE(IsAppPreferred(app_id2));
}

TEST_F(AppManagementPageHandlerTestBase,
       GetPreferredAppDifferentScopesNotReset) {
  // Install app1 and mark it as preferred.
  auto web_app_info1 = std::make_unique<web_app::WebAppInstallInfo>();
  web_app_info1->title = u"app_name";
  web_app_info1->start_url = GURL("https://example.com/index.html");
  web_app_info1->scope = GURL("https://example.com/");

  std::string app_id1 =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info1));

  handler()->SetPreferredApp(app_id1, /*is_preferred_app=*/true);
  AwaitWebAppCommandsComplete();

  // Install app2 with same scope as app1.
  auto web_app_info2 = std::make_unique<web_app::WebAppInstallInfo>();
  web_app_info2->title = u"app_name2";
  web_app_info2->start_url = GURL("https://example.com/index_abc.html");
  web_app_info2->scope = GURL("https://example.com/");

  std::string app_id2 =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info2));

  // Install app3 with a completely different scope than app1 and app2.
  auto web_app_info3 = std::make_unique<web_app::WebAppInstallInfo>();
  web_app_info3->title = u"app_name3";
  web_app_info3->start_url = GURL("https://abc.com/index.html");
  web_app_info3->scope = GURL("https://abc.com/def/");

  std::string app_id3 =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info3));

  // Set app2 and app3 as preferred
  handler()->SetPreferredApp(app_id2, /*is_preferred_app=*/true);
  AwaitWebAppCommandsComplete();
  handler()->SetPreferredApp(app_id3, /*is_preferred_app=*/true);
  AwaitWebAppCommandsComplete();

  // Verify the preferred app status of app_id1, app_id2 and app_id3. app_id1's
  // preferred app status should have been reset to false, while app_id2 and
  // app_id3 should still be true.
  EXPECT_FALSE(IsAppPreferred(app_id1));
  EXPECT_TRUE(IsAppPreferred(app_id2));
  EXPECT_TRUE(IsAppPreferred(app_id3));
}

TEST_F(AppManagementPageHandlerTestBase, GetPreferredAppTestInvalidAppId) {
  auto web_app_info = std::make_unique<web_app::WebAppInstallInfo>();
  web_app_info->title = u"app_name";
  web_app_info->start_url = GURL("https://example.com/index.html");
  web_app_info->scope = GURL("https://example.com/");

  std::string app_id =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info));

  EXPECT_FALSE(IsAppPreferred(app_id));
  handler()->SetPreferredApp("def", /*is_preferred_app=*/true);
  EXPECT_FALSE(IsAppPreferred(app_id));
}

TEST_F(AppManagementPageHandlerTestBase,
       GetPreferredAppTestInvalidSupportedLink) {
  auto web_app_info = std::make_unique<web_app::WebAppInstallInfo>();
  web_app_info->title = u"app_name";
  web_app_info->start_url = GURL("https://example.com/index.html");
  web_app_info->scope = GURL("abc://example.com/");

  std::string app_id =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info));

  EXPECT_FALSE(IsAppPreferred(app_id));

  handler()->SetPreferredApp(app_id, /*is_preferred_app=*/true);

  base::test::TestFuture<app_management::mojom::AppPtr> updated_result;
  handler()->GetApp(app_id, updated_result.GetCallback());
  EXPECT_FALSE(updated_result.Get()->is_preferred_app);
  EXPECT_TRUE(updated_result.Get()->supported_links.empty());
}

TEST_F(AppManagementPageHandlerTestBase, GetOverlappingPreferredApps) {
  // First install an app that has some scope set in it.
  auto web_app_info1 = std::make_unique<web_app::WebAppInstallInfo>();
  web_app_info1->title = u"app_name";
  web_app_info1->start_url = GURL("https://example.com/index.html");
  web_app_info1->scope = GURL("https://example.com/");

  std::string app_id1 =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info1));

  // 2nd app has the same scope, but different app_id and opens in a new window.
  auto web_app_info2 = std::make_unique<web_app::WebAppInstallInfo>();
  web_app_info2->title = u"app_name2";
  web_app_info2->start_url = GURL("https://example.com/index_abc.html");
  web_app_info2->user_display_mode =
      web_app::mojom::UserDisplayMode::kStandalone;
  web_app_info2->scope = GURL("https://example.com/");

  std::string app_id2 =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info2));

  // Set app_id1 as a preferred app.
  handler()->SetPreferredApp(app_id1, /*is_preferred_app=*/true);
  AwaitWebAppCommandsComplete();

  std::vector<std::string> overlapping_apps =
      GetOverlappingPreferredApps(app_id1);
  EXPECT_TRUE(overlapping_apps.empty());
}

TEST_F(AppManagementPageHandlerTestBase, GetOverlappingPreferredAppsTwice) {
  // First install an app that has some scope set in it.
  auto web_app_info1 = std::make_unique<web_app::WebAppInstallInfo>();
  web_app_info1->title = u"app_name";
  web_app_info1->start_url = GURL("https://example.com/index.html");
  web_app_info1->scope = GURL("https://example.com/");

  std::string app_id1 =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info1));

  // 2nd app has the same scope, but different app_id and opens in a new window.
  auto web_app_info2 = std::make_unique<web_app::WebAppInstallInfo>();
  web_app_info2->title = u"app_name2";
  web_app_info2->start_url = GURL("https://example.com/index_abc.html");
  web_app_info2->user_display_mode =
      web_app::mojom::UserDisplayMode::kStandalone;
  web_app_info2->scope = GURL("https://example.com/");

  std::string app_id2 =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info2));

  // Set app_id1 as a preferred app.
  handler()->SetPreferredApp(app_id1, /*is_preferred_app=*/true);
  AwaitWebAppCommandsComplete();

  std::vector<std::string> overlapping_apps =
      GetOverlappingPreferredApps(app_id1);
  EXPECT_TRUE(overlapping_apps.empty());

  // Set app_id2 as a preferred app.
  handler()->SetPreferredApp(app_id2, /*is_preferred_app=*/true);
  AwaitWebAppCommandsComplete();

  // app_id1 should be returned here.
  overlapping_apps = GetOverlappingPreferredApps(app_id1);
  EXPECT_THAT(overlapping_apps, testing::ElementsAre(app_id2));
}

TEST_F(AppManagementPageHandlerTestBase,
       GetOverlappingPreferredAppsTwiceNonPreferred) {
  // First install an app that has some scope set in it.
  auto web_app_info1 = std::make_unique<web_app::WebAppInstallInfo>();
  web_app_info1->title = u"app_name";
  web_app_info1->start_url = GURL("https://example.com/index.html");
  web_app_info1->scope = GURL("https://example.com/");

  std::string app_id1 =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info1));

  // 2nd app has the same scope, but different app_id and opens in a new window.
  auto web_app_info2 = std::make_unique<web_app::WebAppInstallInfo>();
  web_app_info2->title = u"app_name2";
  web_app_info2->start_url = GURL("https://example.com/index_abc.html");
  web_app_info2->user_display_mode =
      web_app::mojom::UserDisplayMode::kStandalone;
  web_app_info2->scope = GURL("https://example.com/");

  std::string app_id2 =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info2));

  // Set app_id1 as a preferred app.
  handler()->SetPreferredApp(app_id1, /*is_preferred_app=*/true);
  AwaitWebAppCommandsComplete();

  std::vector<std::string> overlapping_apps =
      GetOverlappingPreferredApps(app_id1);
  EXPECT_TRUE(overlapping_apps.empty());

  // Set app_id2 as a preferred app.
  handler()->SetPreferredApp(app_id2, /*is_preferred_app=*/true);
  AwaitWebAppCommandsComplete();

  // Since app_id2 is already a preferred app, there should not be any other
  // preferred apps.
  overlapping_apps = GetOverlappingPreferredApps(app_id2);
  EXPECT_TRUE(overlapping_apps.empty());
}

TEST_F(AppManagementPageHandlerTestBase,
       GetOverlappingPreferredAppsShortcutApp) {
  auto web_app_info1 = std::make_unique<web_app::WebAppInstallInfo>();
  web_app_info1->title = u"app_name";
  web_app_info1->start_url = GURL("https://example.com/index.html");
  web_app_info1->scope = GURL("https://example.com/");

  std::string app_id1 =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info1));

  // The WebAppRegistrar treats an app without a scope as a shortcut app.
  auto web_app_info2 = std::make_unique<web_app::WebAppInstallInfo>();
  web_app_info2->title = u"app_name2";
  web_app_info2->start_url = GURL("https://example.com/index_abc.html");
  web_app_info2->user_display_mode =
      web_app::mojom::UserDisplayMode::kStandalone;

  std::string app_id2 =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info2));

  // The result should be empty since app_id2 is a shortcut app.
  std::vector<std::string> overlapping_apps =
      GetOverlappingPreferredApps(app_id1);
  EXPECT_TRUE(overlapping_apps.empty());
}

TEST_F(AppManagementPageHandlerTestBase, DifferentScopeNoOverlap) {
  auto web_app_info1 = std::make_unique<web_app::WebAppInstallInfo>();
  web_app_info1->title = u"app_name";
  web_app_info1->start_url = GURL("https://example.com/index.html");
  web_app_info1->scope = GURL("invalid");

  std::string app_id1 =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info1));

  auto web_app_info2 = std::make_unique<web_app::WebAppInstallInfo>();
  web_app_info2->title = u"app_name2";
  web_app_info2->start_url = GURL("https://example_2.com/index_abc.html");
  web_app_info2->user_display_mode =
      web_app::mojom::UserDisplayMode::kStandalone;
  web_app_info2->scope = GURL("https://example_2.com/");

  std::string app_id2 =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info2));

  // The result should be empty since none of the apps have the same scope.
  std::vector<std::string> overlapping_apps =
      GetOverlappingPreferredApps(app_id1);
  EXPECT_TRUE(overlapping_apps.empty());
}

TEST_F(AppManagementPageHandlerTestBase, UseCase_ADisabledBDisabled) {
  auto web_app_info1 = std::make_unique<web_app::WebAppInstallInfo>();
  web_app_info1->title = u"A";
  web_app_info1->start_url = GURL("https://example.com/index.html");
  web_app_info1->scope = GURL("https://example.com/");

  std::string appA =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info1));

  auto web_app_info2 = std::make_unique<web_app::WebAppInstallInfo>();
  web_app_info2->title = u"B";
  web_app_info2->start_url = GURL("https://example.com/index_abc.html");
  web_app_info2->user_display_mode =
      web_app::mojom::UserDisplayMode::kStandalone;
  web_app_info2->scope = GURL("https://example.com/abc/");

  std::string appB =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info2));

  std::vector<std::string> overlapping_apps_a =
      GetOverlappingPreferredApps(appA);
  EXPECT_TRUE(overlapping_apps_a.empty());

  std::vector<std::string> overlapping_apps_b =
      GetOverlappingPreferredApps(appB);
  EXPECT_TRUE(overlapping_apps_b.empty());
}

TEST_F(AppManagementPageHandlerTestBase, UseCase_ADisabledBEnabled) {
  auto web_app_info1 = std::make_unique<web_app::WebAppInstallInfo>();
  web_app_info1->title = u"A";
  web_app_info1->start_url = GURL("https://example.com/index.html");
  web_app_info1->scope = GURL("https://example.com/");

  std::string appA =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info1));

  auto web_app_info2 = std::make_unique<web_app::WebAppInstallInfo>();
  web_app_info2->title = u"B";
  web_app_info2->start_url = GURL("https://example.com/index_abc.html");
  web_app_info2->user_display_mode =
      web_app::mojom::UserDisplayMode::kStandalone;
  web_app_info2->scope = GURL("https://example.com/abc/");

  std::string appB =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info2));

  handler()->SetPreferredApp(appB, /*is_preferred_app=*/true);
  AwaitWebAppCommandsComplete();

  // B is set as preferred app and its scope matches the prefix of A's scope.
  std::vector<std::string> overlapping_apps_a =
      GetOverlappingPreferredApps(appA);
  EXPECT_THAT(overlapping_apps_a, testing::ElementsAre(appB));

  std::vector<std::string> overlapping_apps_b =
      GetOverlappingPreferredApps(appB);
  EXPECT_TRUE(overlapping_apps_b.empty());
}

TEST_F(AppManagementPageHandlerTestBase, UseCase_AEnabledBDisabled) {
  auto web_app_info1 = std::make_unique<web_app::WebAppInstallInfo>();
  web_app_info1->title = u"A";
  web_app_info1->start_url = GURL("https://example.com/index.html");
  web_app_info1->scope = GURL("https://example.com/");

  std::string appA =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info1));

  auto web_app_info2 = std::make_unique<web_app::WebAppInstallInfo>();
  web_app_info2->title = u"B";
  web_app_info2->start_url = GURL("https://example.com/index_abc.html");
  web_app_info2->user_display_mode =
      web_app::mojom::UserDisplayMode::kStandalone;
  web_app_info2->scope = GURL("https://example.com/abc/");

  std::string appB =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info2));

  handler()->SetPreferredApp(appA, /*is_preferred_app=*/true);
  AwaitWebAppCommandsComplete();

  std::vector<std::string> overlapping_apps_a =
      GetOverlappingPreferredApps(appA);
  EXPECT_TRUE(overlapping_apps_a.empty());

  // A is set as preferred app and its scope matches the prefix of B's scope.
  std::vector<std::string> overlapping_apps_b =
      GetOverlappingPreferredApps(appB);
  EXPECT_THAT(overlapping_apps_b, testing::ElementsAre(appA));
}

TEST_F(AppManagementPageHandlerTestBase, UseCase_AEnabledBEnabled) {
  auto web_app_info1 = std::make_unique<web_app::WebAppInstallInfo>();
  web_app_info1->title = u"A";
  web_app_info1->start_url = GURL("https://example.com/index.html");
  web_app_info1->scope = GURL("https://example.com/");

  std::string appA =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info1));

  auto web_app_info2 = std::make_unique<web_app::WebAppInstallInfo>();
  web_app_info2->title = u"B";
  web_app_info2->start_url = GURL("https://example.com/index_abc.html");
  web_app_info2->user_display_mode =
      web_app::mojom::UserDisplayMode::kStandalone;
  web_app_info2->scope = GURL("https://example.com/abc/");

  std::string appB =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info2));

  handler()->SetPreferredApp(appA, /*is_preferred_app=*/true);
  AwaitWebAppCommandsComplete();
  handler()->SetPreferredApp(appB, /*is_preferred_app=*/true);
  AwaitWebAppCommandsComplete();

  // Since both are enabled, B's scope prefix matches A's scope and is longer,
  // so that is returned for A.
  std::vector<std::string> overlapping_apps_a =
      GetOverlappingPreferredApps(appA);
  EXPECT_THAT(overlapping_apps_a, testing::ElementsAre(appB));

  // While A and B are both enabled, and their scopes prefix match, B should not
  // return A to prevent document links for being captured by A, who has a
  // shorter scope.
  std::vector<std::string> overlapping_apps_b =
      GetOverlappingPreferredApps(appB);
  EXPECT_TRUE(overlapping_apps_b.empty());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
class AppManagementPageHandlerArcTest
    : public AppManagementPageHandlerTestBase {
 public:
  void SetUp() override {
    AppManagementPageHandlerTestBase::SetUp();
    // We want to set up the real ArcIntentHelper KeyedService with a fake
    // ArcIntentHelperBridge, so that it's the same object that ArcApps
    // uses to launch apps.
    arc_test_.set_initialize_real_intent_helper_bridge(true);
    arc_test_.SetUp(profile());
  }

  void TearDown() override {
    arc_test_.StopArcInstance();
    arc_test_.TearDown();
  }

 protected:
  ArcAppTest* arc_test() { return &arc_test_; }

 private:
  ArcAppTest arc_test_;
};

TEST_F(AppManagementPageHandlerArcTest, OpenStorePageArcAppPlayStore) {
  const auto& fake_apps = arc_test()->fake_apps();
  std::string package_name = fake_apps[1]->package_name;
  std::string app_id = ArcAppListPrefs::GetAppId(fake_apps[1]->package_name,
                                                 fake_apps[1]->activity);

  std::vector<arc::mojom::AppInfoPtr> apps;
  apps.push_back(arc::mojom::AppInfo::New("Play Store", arc::kPlayStorePackage,
                                          arc::kPlayStoreActivity));
  apps.push_back(fake_apps[1]->Clone());
  arc_test()->app_instance()->SendRefreshAppList(apps);

  handler()->OpenStorePage(app_id);

  auto* intent_helper = arc_test()->intent_helper_instance();
  const std::vector<arc::FakeIntentHelperInstance::HandledIntent>& intents =
      intent_helper->handled_intents();
  EXPECT_EQ(intents.size(), 1U);
  EXPECT_EQ(intents[0].activity->package_name, arc::kPlayStorePackage);
  EXPECT_EQ(intents[0].intent->data.value(),
            "https://play.google.com/store/apps/details?id=" +
                fake_apps[1]->package_name);
}

TEST_F(AppManagementPageHandlerArcTest, OpenStorePageWebAppPlayStore) {
  std::vector<arc::mojom::ArcPackageInfoPtr> packages;
  auto package = arc::mojom::ArcPackageInfo::New();
  package->package_name = "package_name";
  package->package_version = 1;
  package->last_backup_android_id = 1;
  package->last_backup_time = 1;
  package->sync = true;
  package->web_app_info = arc::mojom::WebAppInfo::New(
      "Fake App Title", "https://www.google.com/index.html",
      "https://www.google.com/", 0xFFAABBCC);
  packages.push_back(std::move(package));

  std::vector<arc::mojom::AppInfoPtr> apps;
  apps.push_back(arc::mojom::AppInfo::New("Play Store", arc::kPlayStorePackage,
                                          arc::kPlayStoreActivity));

  arc_test()->app_instance()->SendRefreshAppList(apps);
  ash::ApkWebAppService* service = ash::ApkWebAppService::Get(profile());

  base::test::TestFuture<const std::string&, const web_app::AppId&>
      installed_result;

  service->SetWebAppInstalledCallbackForTesting(installed_result.GetCallback());
  arc_test()->app_instance()->SendRefreshPackageList(std::move(packages));

  web_app::AppId app_id = installed_result.Get<1>();
  handler()->OpenStorePage(app_id);

  auto* intent_helper = arc_test()->intent_helper_instance();
  const std::vector<arc::FakeIntentHelperInstance::HandledIntent>& intents =
      intent_helper->handled_intents();
  EXPECT_EQ(intents.size(), 1U);
  EXPECT_EQ(intents[0].activity->package_name, arc::kPlayStorePackage);
  EXPECT_EQ(intents[0].intent->data.value(),
            "https://play.google.com/store/apps/details?id=package_name");
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace apps
