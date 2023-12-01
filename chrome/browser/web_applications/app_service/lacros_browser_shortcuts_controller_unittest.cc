// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/app_service/lacros_browser_shortcuts_controller.h"

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_test_util.h"
#include "chrome/browser/apps/app_service/app_icon/web_app_icon_test_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/app_service/web_apps_with_shortcuts_test.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
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
#include "ui/display/types/display_constants.h"
#include "url/gurl.h"

namespace web_app {

class FakeShortcutPublisher : public crosapi::mojom::AppShortcutPublisher {
 public:
  FakeShortcutPublisher() = default;
  ~FakeShortcutPublisher() override = default;

  const std::vector<apps::ShortcutPtr>& get_deltas() const {
    return shortcut_deltas_;
  }

  const std::vector<std::string>& get_removed_ids() const {
    return removed_ids_;
  }

  void clear_deltas() { shortcut_deltas_.clear(); }

  bool controller_registered() { return controller_.is_bound(); }

  mojo::Receiver<crosapi::mojom::AppShortcutPublisher> receiver_{this};
  mojo::Remote<crosapi::mojom::AppShortcutController> controller_;

 private:
  // crosapi::mojom::AppShortcutPublisher:
  void PublishShortcuts(std::vector<apps::ShortcutPtr> deltas,
                        PublishShortcutsCallback callback) override {
    for (auto& delta : deltas) {
      shortcut_deltas_.push_back(std::move(delta));
    }

    std::move(callback).Run();
  }

  void RegisterAppShortcutController(
      mojo::PendingRemote<crosapi::mojom::AppShortcutController> controller,
      RegisterAppShortcutControllerCallback callback) override {
    controller_.Bind(std::move(controller));
    std::move(callback).Run(
        crosapi::mojom::ControllerRegistrationResult::kSuccess);
  }

  void ShortcutRemoved(const std::string& shortcut_id,
                       ShortcutRemovedCallback callback) override {
    removed_ids_.push_back(shortcut_id);
    std::move(callback).Run();
  }

  std::vector<apps::ShortcutPtr> shortcut_deltas_;
  std::vector<std::string> removed_ids_;
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

  std::string CreateWebAppBasedShortcut(const GURL& shortcut_url,
                                        const std::u16string& shortcut_name,
                                        bool with_icon = false) {
    // Create web app based shortcut.
    auto web_app_info = std::make_unique<web_app::WebAppInstallInfo>();
    web_app_info->start_url = shortcut_url;
    web_app_info->title = shortcut_name;

    if (with_icon) {
      const GeneratedIconsInfo icon_info(
          IconPurpose::ANY, {web_app::icon_size::k32}, {SK_ColorBLACK});
      web_app::AddIconsToWebAppInstallInfo(
          web_app_info.get(), GURL(shortcut_url.spec() + "/icon"), {icon_info});
    }

    auto local_shortcut_id = web_app::test::InstallWebApp(
        profile(), std::move(web_app_info),
        /*overwrite_existing_manifest_fields=*/true);
    return local_shortcut_id;
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
  auto local_id_1 = CreateWebAppBasedShortcut(GURL("https://www.example.com/"),
                                              u"shortcut name");

  InitializeLacrosBrowserShortcutsController();
  ASSERT_TRUE(fake_publisher()->controller_registered());
  ASSERT_EQ(fake_publisher()->get_deltas().size(), 1U);
  EXPECT_EQ(fake_publisher()->get_deltas().back()->local_id, local_id_1);
  EXPECT_EQ(fake_publisher()->get_deltas().back()->host_app_id,
            app_constants::kLacrosAppId);
  EXPECT_EQ(fake_publisher()->get_deltas().back()->shortcut_id,
            apps::GenerateShortcutId(app_constants::kLacrosAppId, local_id_1));
  EXPECT_EQ(fake_publisher()->get_deltas().back()->name, "shortcut name");
  EXPECT_EQ(fake_publisher()->get_deltas().back()->shortcut_source,
            apps::ShortcutSource::kUser);
  EXPECT_TRUE(fake_publisher()->get_deltas().back()->icon_key.has_value());
  EXPECT_EQ(
      fake_publisher()->get_deltas().back()->icon_key->icon_effects,
      apps::IconEffects::kRoundCorners | apps::IconEffects::kCrOsStandardMask);
  EXPECT_TRUE(fake_publisher()->get_deltas().back()->allow_removal);

  auto local_id_2 = CreateWebAppBasedShortcut(
      GURL("https://www.another-example.com/"), u"another shortcut name",
      /*with_icon = */ true);

  EXPECT_EQ(fake_publisher()->get_deltas().size(), 2U);
  EXPECT_EQ(fake_publisher()->get_deltas().back()->local_id, local_id_2);
  EXPECT_EQ(fake_publisher()->get_deltas().back()->host_app_id,
            app_constants::kLacrosAppId);
  EXPECT_EQ(fake_publisher()->get_deltas().back()->shortcut_id,
            apps::GenerateShortcutId(app_constants::kLacrosAppId, local_id_2));
  EXPECT_EQ(fake_publisher()->get_deltas().back()->name,
            "another shortcut name");
  EXPECT_EQ(fake_publisher()->get_deltas().back()->shortcut_source,
            apps::ShortcutSource::kUser);
  EXPECT_TRUE(fake_publisher()->get_deltas().back()->icon_key.has_value());
  EXPECT_EQ(
      fake_publisher()->get_deltas().back()->icon_key->icon_effects,
      apps::IconEffects::kRoundCorners | apps::IconEffects::kCrOsStandardIcon);
  EXPECT_TRUE(fake_publisher()->get_deltas().back()->allow_removal);
}

TEST_F(LacrosBrowserShortcutsControllerTest, WebAppNotPublished) {
  auto app_id_1 = CreateWebApp(GURL("https://www.example.com/"), u"app name");

  InitializeLacrosBrowserShortcutsController();

  ASSERT_EQ(fake_publisher()->get_deltas().size(), 0U);

  auto app_id_2 = CreateWebApp(GURL("https://www.another-example.com/"),
                               u"another app name");

  EXPECT_EQ(fake_publisher()->get_deltas().size(), 0U);
}

TEST_F(LacrosBrowserShortcutsControllerTest, LaunchShortcut) {
  InitializeLacrosBrowserShortcutsController();

  auto shortcut_id = CreateWebAppBasedShortcut(GURL("https://www.example.com/"),
                                               u"shortcut name");

  base::RunLoop runloop;
  fake_publisher()->controller_->LaunchShortcut(
      app_constants::kLacrosAppId, shortcut_id, display::kDefaultDisplayId,
      runloop.QuitClosure());
  runloop.Run();
}

TEST_F(LacrosBrowserShortcutsControllerTest, GetCompressedIcon) {
  InitializeLacrosBrowserShortcutsController();

  auto shortcut_id = CreateWebAppBasedShortcut(GURL("https://www.example.com/"),
                                               u"shortcut name");
  const float scale1 = 1.0;
  const float scale2 = 2.0;
  const int kIconSize1 = 64 * scale1;
  const int kIconSize2 = 64 * scale2;
  const std::vector<int> sizes_px{kIconSize1, kIconSize2};
  const std::vector<SkColor> colors{SK_ColorGREEN, SK_ColorYELLOW};
  apps::WebAppIconTestHelper(profile()).WriteIcons(
      shortcut_id, {IconPurpose::ANY}, sizes_px, colors);
  FakeWebAppProvider* fake_provider =
      static_cast<FakeWebAppProvider*>(WebAppProvider::GetForTest(profile()));
  WebApp* web_app =
      fake_provider->GetRegistrarMutable().GetAppByIdMutable(shortcut_id);
  web_app->SetDownloadedIconSizes(IconPurpose::ANY, sizes_px);
  ASSERT_TRUE(fake_provider->icon_manager().HasIcons(
      shortcut_id, IconPurpose::ANY, sizes_px));

  apps::ScaleToSize scale_to_size_in_px = {{1.0, kIconSize1},
                                           {2.0, kIconSize2}};

  std::vector<uint8_t> src_data =
      apps::WebAppIconTestHelper(profile()).GenerateWebAppCompressedIcon(
          shortcut_id, IconPurpose::ANY, apps::IconEffects::kNone, sizes_px,
          scale_to_size_in_px, scale1);
  base::test::TestFuture<apps::IconValuePtr> future;
  fake_publisher()->controller_->GetCompressedIcon(
      app_constants::kLacrosAppId, shortcut_id, 64,
      ui::GetSupportedResourceScaleFactor(scale1), future.GetCallback());
  apps::IconValuePtr icon = future.Take();
  VerifyCompressedIcon(src_data, *icon);
}

TEST_F(LacrosBrowserShortcutsControllerTest, RemoveShortcut) {
  InitializeLacrosBrowserShortcutsController();

  auto shortcut_id = CreateWebAppBasedShortcut(GURL("https://www.example.com/"),
                                               u"shortcut name");

  base::RunLoop runloop;
  fake_publisher()->controller_->RemoveShortcut(
      app_constants::kLacrosAppId, shortcut_id, apps::UninstallSource::kUnknown,
      runloop.QuitClosure());
  runloop.Run();

  ASSERT_EQ(fake_publisher()->get_removed_ids().size(), 1U);
  EXPECT_EQ(fake_publisher()->get_removed_ids().back(), shortcut_id);
}

}  // namespace web_app
