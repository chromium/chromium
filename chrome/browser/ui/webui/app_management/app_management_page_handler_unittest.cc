// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/app_management/app_management_page_handler.h"

#include "base/test/test_future.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
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

AppManagementPageHandler CreateAppManagementPageHandler(
    Profile* profile,
    TestDelegate& delegate) {
  mojo::PendingReceiver<app_management::mojom::Page> page;
  mojo::Remote<app_management::mojom::PageHandler> handler;
  return AppManagementPageHandler(handler.BindNewPipeAndPassReceiver(),
                                  page.InitWithNewPipeAndPassRemote(), profile,
                                  delegate);
}
}  // namespace

namespace apps {

class AppManagementPageHandlerTest : public testing::Test {
 public:
  void SetUp() override {
    testing::Test::SetUp();

    profile_ = std::make_unique<TestingProfile>();

    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  Profile* profile() { return profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(AppManagementPageHandlerTest, GetApp) {
  std::unique_ptr<TestDelegate> delegate = std::unique_ptr<TestDelegate>();

  auto handler = CreateAppManagementPageHandler(profile(), *delegate);

  // Create a web app entry with scope, which would be recognised
  // as normal web app in the web app system.
  auto web_app_info = std::make_unique<web_app::WebAppInstallInfo>();
  web_app_info->title = u"app_name";
  web_app_info->start_url = GURL("https://example.com/");

  std::string app_id =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info));

  base::test::TestFuture<app_management::mojom::AppPtr> result;

  handler.GetApp(app_id, result.GetCallback());

  EXPECT_EQ(result.Get()->id, app_id);
  EXPECT_EQ(result.Get()->title.value(), "app_name");
  EXPECT_EQ(result.Get()->type, AppType::kWeb);
}

}  // namespace apps
