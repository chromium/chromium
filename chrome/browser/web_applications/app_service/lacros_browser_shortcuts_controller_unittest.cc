// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/app_service/lacros_browser_shortcuts_controller.h"

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/app_service/web_apps_with_shortcuts_test.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/crosapi/mojom/app_service.mojom.h"
#include "chromeos/crosapi/mojom/app_service_types.mojom-forward.h"
#include "chromeos/crosapi/mojom/app_service_types.mojom-shared.h"
#include "chromeos/crosapi/mojom/app_service_types.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/lacros/lacros_test_helper.h"
#include "components/app_constants/constants.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

class FakeShortcutPublisher : public crosapi::mojom::AppShortcutPublisher {
 public:
  FakeShortcutPublisher() = default;
  ~FakeShortcutPublisher() override = default;

  const std::vector<apps::ShortcutPtr>& get_deltas() const {
    return shortcut_deltas_;
  }

  void clear_deltas() { shortcut_deltas_.clear(); }

  mojo::Receiver<crosapi::mojom::AppShortcutPublisher> receiver_{this};

 private:
  // crosapi::mojom::AppShortcutPublisher:
  void PublishShortcuts(std::vector<apps::ShortcutPtr> deltas,
                        PublishShortcutsCallback callback) override {
    for (auto& delta : deltas) {
      shortcut_deltas_.push_back(std::move(delta));
    }

    std::move(callback).Run();
  }

  std::vector<apps::ShortcutPtr> shortcut_deltas_;
};

class LacrosBrowserShortcutsControllerTest : public testing::Test,
                                             public WebAppsWithShortcutsTest {
 public:
  void SetUp() override {
    EnableCrosWebAppShortcutUiUpdate(true);
    profile_ = std::make_unique<TestingProfile>();
    profile_->SetIsMainProfile(true);
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  apps::ShortcutId CreateWebAppBasedShortcut(
      const GURL& shortcut_url,
      const std::u16string& shortcut_name) {
    // Create web app based shortcut.
    auto web_app_info = std::make_unique<web_app::WebAppInstallInfo>();
    web_app_info->start_url = shortcut_url;
    web_app_info->title = shortcut_name;
    auto local_shortcut_id = web_app::test::InstallWebApp(
        profile(), std::move(web_app_info),
        /*overwrite_existing_manifest_fields=*/true);
    return apps::GenerateShortcutId(app_constants::kLacrosAppId,
                                    local_shortcut_id);
  }

  std::string CreateWebApp(const GURL& app_url,
                           const std::u16string& app_name) {
    // Create web app.
    auto web_app_info = std::make_unique<web_app::WebAppInstallInfo>();
    web_app_info->start_url = app_url;
    web_app_info->title = app_name;
    web_app_info->scope = app_url;
    auto web_app_id = web_app::test::InstallWebApp(
        profile(), std::move(web_app_info),
        /*overwrite_existing_manifest_fields=*/true);
    return web_app_id;
  }

  void InitializeLacrosBrowserShortcutsController() {
    fake_publisher_ = std::make_unique<FakeShortcutPublisher>();
    lacros_browser_shortcuts_controller_ =
        std::make_unique<LacrosBrowserShortcutsController>(profile());
    chromeos::LacrosService::Get()->InjectRemoteForTesting(
        fake_publisher_->receiver_.BindNewPipeAndPassRemote());
    base::RunLoop run_loop;
    LacrosBrowserShortcutsController::SetInitializedCallbackForTesting(
        run_loop.QuitClosure());
    lacros_browser_shortcuts_controller_->Initialize();
    run_loop.Run();
  }

  Profile* profile() { return profile_.get(); }
  FakeShortcutPublisher* fake_publisher() { return fake_publisher_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<FakeShortcutPublisher> fake_publisher_;
  std::unique_ptr<LacrosBrowserShortcutsController>
      lacros_browser_shortcuts_controller_;
  chromeos::ScopedLacrosServiceTestHelper lacros_service_test_helper_;
};

TEST_F(LacrosBrowserShortcutsControllerTest, PublishShortcuts) {
  auto shortcut_id_1 = CreateWebAppBasedShortcut(
      GURL("https://www.example.com/"), u"shortcut name");

  InitializeLacrosBrowserShortcutsController();
  ASSERT_EQ(fake_publisher()->get_deltas().size(), 1U);
  EXPECT_EQ(fake_publisher()->get_deltas().back()->shortcut_id, shortcut_id_1);

  auto shortcut_id_2 = CreateWebAppBasedShortcut(
      GURL("https://www.another-example.com/"), u"another shortcut name");

  EXPECT_EQ(fake_publisher()->get_deltas().size(), 2U);
  EXPECT_EQ(fake_publisher()->get_deltas().back()->shortcut_id, shortcut_id_2);
}

TEST_F(LacrosBrowserShortcutsControllerTest, WebAppNotPublished) {
  auto app_id_1 = CreateWebApp(GURL("https://www.example.com/"), u"app name");

  InitializeLacrosBrowserShortcutsController();

  ASSERT_EQ(fake_publisher()->get_deltas().size(), 0U);

  auto app_id_2 = CreateWebApp(GURL("https://www.another-example.com/"),
                               u"another app name");

  EXPECT_EQ(fake_publisher()->get_deltas().size(), 0U);
}

}  // namespace web_app
