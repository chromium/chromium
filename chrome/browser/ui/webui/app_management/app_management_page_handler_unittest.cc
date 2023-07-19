// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/app_management/app_management_page_handler.h"

#include <memory>

#include "ash/components/arc/test/fake_app_instance.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/apps/apk_web_app_service.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/test/fake_intent_helper_instance.h"
#include "content/public/test/browser_task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/webui/resources/cr_components/app_management/app_management.mojom.h"

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
namespace apps {

class AppManagementPageHandlerTest : public testing::Test {
 public:
  void SetUp() override {
    testing::Test::SetUp();

    profile_ = std::make_unique<TestingProfile>();
    delegate_ = std::unique_ptr<TestDelegate>();

    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());

    // We want to set up the real ArcIntentHelper KeyedService with a fake
    // ArcIntentHelperBridge, so that it's the same object that ArcApps
    // uses to launch apps.
    arc_test_.set_initialize_real_intent_helper_bridge(true);
    arc_test_.SetUp(profile());

    mojo::PendingReceiver<app_management::mojom::Page> page;
    mojo::Remote<app_management::mojom::PageHandler> handler;
    handler_ = std::make_unique<AppManagementPageHandler>(
        handler.BindNewPipeAndPassReceiver(),
        page.InitWithNewPipeAndPassRemote(), profile_.get(), *delegate_.get());
  }

  void TearDown() override {
    arc_test_.StopArcInstance();
    arc_test_.TearDown();
  }

  Profile* profile() { return profile_.get(); }
  ArcAppTest* arc_test() { return &arc_test_; }
  AppManagementPageHandler* handler() { return handler_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  std::unique_ptr<TestingProfile> profile_;
  ArcAppTest arc_test_;
  std::unique_ptr<TestDelegate> delegate_;
  std::unique_ptr<AppManagementPageHandler> handler_;
};

TEST_F(AppManagementPageHandlerTest, GetApp) {
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

TEST_F(AppManagementPageHandlerTest, OpenStorePageArcAppPlayStore) {
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

TEST_F(AppManagementPageHandlerTest, OpenStorePageWebAppPlayStore) {
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

}  // namespace apps
