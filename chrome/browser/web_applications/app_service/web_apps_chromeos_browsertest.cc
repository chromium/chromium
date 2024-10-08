// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>

#include "ash/constants/web_app_id_constants.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_server_mixin.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_constants.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/test_signed_web_bundle_builder.h"
#include "chrome/browser/web_applications/policy/web_app_policy_constants.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/app_menu_constants.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
namespace {

void CheckShortcut(const ui::SimpleMenuModel& model,
                   size_t index,
                   int shortcut_index,
                   const std::u16string& label,
                   std::optional<SkColor> color) {
  EXPECT_EQ(model.GetTypeAt(index), ui::MenuModel::TYPE_COMMAND);
  EXPECT_EQ(model.GetCommandIdAt(index),
            ash::LAUNCH_APP_SHORTCUT_FIRST + shortcut_index);
  EXPECT_EQ(model.GetLabelAt(index), label);

  ui::ImageModel icon = model.GetIconAt(index);
  if (color.has_value()) {
    EXPECT_FALSE(icon.GetImage().IsEmpty());
    EXPECT_EQ(icon.GetImage().AsImageSkia().bitmap()->getColor(15, 15), color);
  } else {
    EXPECT_TRUE(icon.IsEmpty());
  }
}

void CheckSeparator(const ui::SimpleMenuModel& model, size_t index) {
  EXPECT_EQ(model.GetTypeAt(index), ui::MenuModel::TYPE_SEPARATOR);
  EXPECT_EQ(model.GetCommandIdAt(index), -1);
}

}  // namespace

using WebAppsChromeOsBrowserTest = web_app::WebAppBrowserTestBase;

IN_PROC_BROWSER_TEST_F(WebAppsChromeOsBrowserTest, ShortcutIcons) {
  const GURL app_url =
      https_server()->GetURL("/web_app_shortcuts/shortcuts.html");
  const webapps::AppId app_id =
      web_app::InstallWebAppFromPage(browser(), app_url);
  LaunchWebAppBrowser(app_id);

  std::unique_ptr<ui::SimpleMenuModel> menu_model;
  {
    ash::ShelfModel* const shelf_model = ash::ShelfModel::Get();
    PinAppWithIDToShelf(app_id);
    ash::ShelfItemDelegate* const delegate =
        shelf_model->GetShelfItemDelegate(ash::ShelfID(app_id));
    base::RunLoop run_loop;
    delegate->GetContextMenu(
        display::Display::GetDefaultDisplay().id(),
        base::BindLambdaForTesting(
            [&run_loop,
             &menu_model](std::unique_ptr<ui::SimpleMenuModel> model) {
              menu_model = std::move(model);
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  // Shortcuts appear last in the context menu.
  // See /web_app_shortcuts/shortcuts.json for shortcut icon definitions.
  size_t index = menu_model->GetItemCount() - 11;

  // Purpose |any| by default.
  CheckShortcut(*menu_model, index++, 0, u"One", SK_ColorGREEN);
  CheckSeparator(*menu_model, index++);
  // Purpose |maskable| takes precedence over |any|.
  CheckShortcut(*menu_model, index++, 1, u"Two", SK_ColorBLUE);
  CheckSeparator(*menu_model, index++);
  // Purpose |any|.
  CheckShortcut(*menu_model, index++, 2, u"Three", SK_ColorYELLOW);
  CheckSeparator(*menu_model, index++);
  // Purpose |any| and |maskable|.
  CheckShortcut(*menu_model, index++, 3, u"Four", SK_ColorCYAN);
  CheckSeparator(*menu_model, index++);
  // Purpose |maskable|.
  CheckShortcut(*menu_model, index++, 4, u"Five", SK_ColorMAGENTA);
  CheckSeparator(*menu_model, index++);
  // No icons.
  CheckShortcut(*menu_model, index++, 5, u"Six", std::nullopt);
  EXPECT_EQ(index, menu_model->GetItemCount());

  const int command_id = ash::LAUNCH_APP_SHORTCUT_FIRST + 3;
  ui_test_utils::UrlLoadObserver url_observer(
      https_server()->GetURL("/web_app_shortcuts/shortcuts.html#four"));
  menu_model->ActivatedAt(menu_model->GetIndexOfCommandId(command_id).value(),
                          ui::EF_LEFT_MOUSE_BUTTON);
  url_observer.Wait();
}

namespace {

bool HasMenuModelCommandId(ui::MenuModel* model, ash::CommandId command_id) {
  size_t index = 0;
  return ui::MenuModel::GetModelAndIndexForCommandId(command_id, &model,
                                                     &index);
}

}  // namespace

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {
constexpr char kCalculatorAppUrl[] = "https://calculator.apps.chrome/";

enum class AppType { kProgressiveWebApp, kIsolatedWebApp };
struct PreventCloseTestParams {
  bool is_prevent_close_enabled;
  AppType app_type;
};

}  // namespace

class WebAppsPreventCloseChromeOsBrowserTest
    : public web_app::IsolatedWebAppBrowserTestHarness,
      public ::testing::WithParamInterface<PreventCloseTestParams> {
 public:
  WebAppsPreventCloseChromeOsBrowserTest() = default;

  WebAppsPreventCloseChromeOsBrowserTest(
      const WebAppsPreventCloseChromeOsBrowserTest&) = delete;
  WebAppsPreventCloseChromeOsBrowserTest& operator=(
      const WebAppsPreventCloseChromeOsBrowserTest&) = delete;
  ~WebAppsPreventCloseChromeOsBrowserTest() override = default;

  void TearDownOnMainThread() override {
    // Clear policy values, otherwise we won't be able to gracefully close stop
    // browser test.
    ResetPolicies();
    web_app::IsolatedWebAppBrowserTestHarness::TearDownOnMainThread();
  }

  bool IsPreventCloseEnabled() const {
    return GetParam().is_prevent_close_enabled;
  }

  AppType GetAppType() const { return GetParam().app_type; }

  void InstallApp() {
    const webapps::AppId installed_app_id = GetInstalledAppId();
    web_app::WebAppTestInstallObserver observer(profile());
    observer.BeginListening({installed_app_id});
    switch (GetAppType()) {
      case AppType::kProgressiveWebApp:
        installed_app_url_ = kCalculatorAppUrl;
        profile()->GetPrefs()->SetList(
            prefs::kWebAppInstallForceList,
            base::Value::List().Append(
                base::Value::Dict()
                    .Set(web_app::kUrlKey, kCalculatorAppUrl)
                    .Set(web_app::kDefaultLaunchContainerKey,
                         web_app::kDefaultLaunchContainerWindowValue)));
        break;

      case AppType::kIsolatedWebApp:
        auto web_bundle_id = web_app::test::GetDefaultEd25519WebBundleId();

        isolated_web_app_update_server_mixin_.AddBundle(
            web_app::IsolatedWebAppBuilder(
                web_app::ManifestBuilder().SetVersion("1.0.0"))
                .BuildBundle(web_bundle_id,
                             {web_app::test::GetDefaultEd25519KeyPair()}));

        installed_app_url_ =
            web_app::IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
                web_bundle_id)
                .origin()
                .GetURL()
                .spec();
        profile()->GetPrefs()->SetList(
            prefs::kIsolatedWebAppInstallForceList,
            base::Value::List().Append(
                base::Value::Dict()
                    .Set(web_app::kPolicyWebBundleIdKey, web_bundle_id.id())
                    .Set(web_app::kPolicyUpdateManifestUrlKey,
                         isolated_web_app_update_server_mixin_
                             .GetUpdateManifestUrl(web_bundle_id)
                             .spec())));
        break;
    }
    ASSERT_EQ(installed_app_id, observer.Wait());
  }

  webapps::AppId GetInstalledAppId() const {
    return (GetAppType() == AppType::kIsolatedWebApp)
               ? web_app::IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
                     web_app::test::GetDefaultEd25519WebBundleId())
                     .app_id()
               : web_app::kCalculatorAppId;
  }

  void ResetPolicies() {
    profile()->GetPrefs()->SetList(prefs::kWebAppSettings, base::Value::List());
    profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                   base::Value::List());
    web_app::WebAppProvider::GetForTest(profile())
        ->command_manager()
        .AwaitAllCommandsCompleteForTesting();
  }

  bool IsToastShown(const std::string& toast_id) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    return ash::ToastManager::Get()->IsToastShown(toast_id);
#else
    base::test::TestFuture<bool> future;
    chromeos::LacrosService::Get()
        ->GetRemote<crosapi::mojom::TestController>()
        ->IsToastShown(toast_id, future.GetCallback());
    EXPECT_TRUE(future.Wait());
    return future.Get<bool>();
#endif
  }

 protected:
  std::optional<std::string> installed_app_url_;
  web_app::IsolatedWebAppUpdateServerMixin
      isolated_web_app_update_server_mixin_{&mixin_host_};
};

#if BUILDFLAG(IS_CHROMEOS_ASH)

IN_PROC_BROWSER_TEST_P(WebAppsPreventCloseChromeOsBrowserTest, CheckMenuModel) {
  // Set up policy values.
  InstallApp();
  profile()->GetPrefs()->SetList(
      prefs::kWebAppSettings,
      base::Value::List().Append(
          base::Value::Dict()
              .Set(web_app::kManifestId, *installed_app_url_)
              .Set(web_app::kRunOnOsLogin, web_app::kRunWindowed)
              .Set(web_app::kPreventClose, IsPreventCloseEnabled())));

  // Wait until prefs are propagated and App `allow_close` field is updated to
  // expected value.
  const webapps::AppId installed_app_id = GetInstalledAppId();
  apps::AppUpdateWaiter waiter(
      profile(), installed_app_id,
      base::BindRepeating(
          [](bool expected_allow_close, const apps::AppUpdate& update) {
            return update.AllowClose().has_value() &&
                   update.AllowClose().value() == expected_allow_close;
          },
          !IsPreventCloseEnabled()));
  waiter.Await();

  PinAppWithIDToShelf(installed_app_id);

  Browser* const browser = LaunchWebAppBrowser(installed_app_id);
  ASSERT_TRUE(browser);

  ash::ShelfModel* const shelf_model = ash::ShelfModel::Get();
  ASSERT_TRUE(shelf_model);

  ash::ShelfItemDelegate* const delegate =
      shelf_model->GetShelfItemDelegate(ash::ShelfID(installed_app_id));
  ASSERT_TRUE(delegate);

  base::test::TestFuture<std::unique_ptr<ui::SimpleMenuModel>> model_future;
  delegate->GetContextMenu(display::Display::GetDefaultDisplay().id(),
                           model_future.GetCallback());
  std::unique_ptr<ui::SimpleMenuModel> menu_model(model_future.Take());
  ASSERT_TRUE(menu_model);

  // Check close button.
  EXPECT_EQ(HasMenuModelCommandId(menu_model.get(), ash::MENU_CLOSE),
            !IsPreventCloseEnabled());

  // Check new window and new tab buttons.
  EXPECT_EQ(HasMenuModelCommandId(menu_model.get(), ash::LAUNCH_NEW),
            !IsPreventCloseEnabled());

  // Isolated web apps don't support this selection (they are always opened in
  // windows).
  if (GetAppType() != AppType::kIsolatedWebApp) {
    EXPECT_EQ(
        HasMenuModelCommandId(menu_model.get(), ash::USE_LAUNCH_TYPE_REGULAR),
        !IsPreventCloseEnabled());

    EXPECT_EQ(
        HasMenuModelCommandId(menu_model.get(), ash::USE_LAUNCH_TYPE_WINDOW),
        !IsPreventCloseEnabled());
  }
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

IN_PROC_BROWSER_TEST_P(WebAppsPreventCloseChromeOsBrowserTest,
                       CloseTabAttemptShowsToast) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If ash does not contain the relevant test controller functionality,
  // then there's nothing to do for this test.
  auto* lacros_service = chromeos::LacrosService::Get();
  if (lacros_service->GetInterfaceVersion<crosapi::mojom::TestController>() <
      static_cast<int>(crosapi::mojom::TestController::MethodMinVersions::
                           kIsToastShownMinVersion)) {
    GTEST_SKIP() << "Unsupported ash version for IsToastShown";
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  const webapps::AppId installed_app_id = GetInstalledAppId();
  InstallApp();

  profile()->GetPrefs()->SetList(
      prefs::kWebAppSettings,
      base::Value::List().Append(
          base::Value::Dict()
              .Set(web_app::kManifestId, *installed_app_url_)
              .Set(web_app::kRunOnOsLogin, web_app::kRunWindowed)
              .Set(web_app::kPreventClose, IsPreventCloseEnabled())));

  Browser* const browser = LaunchWebAppBrowserAndWait(installed_app_id);
  ASSERT_TRUE(browser);

  chrome::CloseTab(browser);

  if (IsPreventCloseEnabled()) {
    EXPECT_EQ(1, browser->tab_strip_model()->count());
    EXPECT_TRUE(base::test::RunUntil([&] {
      return IsToastShown(
          base::StrCat({"prevent_close_toast_id-", installed_app_id}));
    }));
  } else {
    EXPECT_EQ(0, browser->tab_strip_model()->count());
  }
}

IN_PROC_BROWSER_TEST_P(WebAppsPreventCloseChromeOsBrowserTest,
                       CloseWindowAttemptShowsToast) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If ash does not contain the relevant test controller functionality,
  // then there's nothing to do for this test.
  auto* lacros_service = chromeos::LacrosService::Get();
  if (lacros_service->GetInterfaceVersion<crosapi::mojom::TestController>() <
      static_cast<int>(crosapi::mojom::TestController::MethodMinVersions::
                           kIsToastShownMinVersion)) {
    GTEST_SKIP() << "Unsupported ash version for IsToastShown";
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  const webapps::AppId installed_app_id = GetInstalledAppId();
  InstallApp();

  profile()->GetPrefs()->SetList(
      prefs::kWebAppSettings,
      base::Value::List().Append(
          base::Value::Dict()
              .Set(web_app::kManifestId, *installed_app_url_)
              .Set(web_app::kRunOnOsLogin, web_app::kRunWindowed)
              .Set(web_app::kPreventClose, IsPreventCloseEnabled())));

  Browser* const browser = LaunchWebAppBrowserAndWait(installed_app_id);
  ASSERT_TRUE(browser);

  chrome::CloseWindow(browser);

  if (IsPreventCloseEnabled()) {
    EXPECT_EQ(1, browser->tab_strip_model()->count());
    EXPECT_TRUE(base::test::RunUntil([&] {
      return IsToastShown(
          base::StrCat({"prevent_close_toast_id-", installed_app_id}));
    }));
  } else {
    EXPECT_EQ(0, browser->tab_strip_model()->count());
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WebAppsPreventCloseChromeOsBrowserTest,
    testing::Values(
        PreventCloseTestParams{.is_prevent_close_enabled = true,
                               .app_type = AppType::kProgressiveWebApp},
        PreventCloseTestParams{.is_prevent_close_enabled = false,
                               .app_type = AppType::kProgressiveWebApp},
        PreventCloseTestParams{.is_prevent_close_enabled = true,
                               .app_type = AppType::kIsolatedWebApp},
        PreventCloseTestParams{.is_prevent_close_enabled = false,
                               .app_type = AppType::kIsolatedWebApp}));

#if BUILDFLAG(IS_CHROMEOS_ASH)
class IsolatedWebAppChromeOsBrowserTest
    : public web_app::IsolatedWebAppBrowserTestHarness {
 public:
  void SetUp() override {
    app_ = web_app::IsolatedWebAppBuilder(web_app::ManifestBuilder())
               .BuildBundle();
    InProcessBrowserTest::SetUp();
  }

  web_app::ScopedBundledIsolatedWebApp* app() { return app_.get(); }

 private:
  std::unique_ptr<web_app::ScopedBundledIsolatedWebApp> app_;
};

IN_PROC_BROWSER_TEST_F(IsolatedWebAppChromeOsBrowserTest,
                       ContextMenuOnlyHasLaunchNew) {
  app()->TrustSigningKey();
  web_app::IsolatedWebAppUrlInfo url_info =
      app()->InstallChecked(browser()->profile());

  PinAppWithIDToShelf(url_info.app_id());

  ash::ShelfModel* const shelf_model = ash::ShelfModel::Get();
  ASSERT_TRUE(shelf_model);

  ash::ShelfItemDelegate* const delegate =
      shelf_model->GetShelfItemDelegate(ash::ShelfID(url_info.app_id()));
  ASSERT_TRUE(delegate);

  base::test::TestFuture<std::unique_ptr<ui::SimpleMenuModel>> model_future;
  delegate->GetContextMenu(display::Display::GetDefaultDisplay().id(),
                           model_future.GetCallback());
  std::unique_ptr<ui::SimpleMenuModel> menu_model(model_future.Take());
  ASSERT_TRUE(menu_model);

  // Isolated web apps context menu should have an "Open in new Window" command
  // instead of a open mode selector submenu.
  EXPECT_NE(menu_model->GetTypeAt(0), ui::MenuModel::ItemType::TYPE_SUBMENU);
  EXPECT_EQ(menu_model->GetTypeAt(0), ui::MenuModel::ItemType::TYPE_COMMAND);
  EXPECT_EQ(menu_model->GetCommandIdAt(0), ash::LAUNCH_NEW);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
