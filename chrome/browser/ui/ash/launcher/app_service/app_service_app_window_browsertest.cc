// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/web_application_info.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/arc_util.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/arc/test/fake_app_instance.h"
#include "components/exo/shell_surface_util.h"
#include "components/services/app_service/public/cpp/instance.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/views/widget/widget.h"

using extensions::AppWindow;
using extensions::Extension;

namespace mojo {

template <>
struct TypeConverter<arc::mojom::ArcPackageInfoPtr,
                     arc::mojom::ArcPackageInfo> {
  static arc::mojom::ArcPackageInfoPtr Convert(
      const arc::mojom::ArcPackageInfo& package_info) {
    return package_info.Clone();
  }
};

}  // namespace mojo

namespace {

constexpr char kTestAppName[] = "Test ARC App";
constexpr char kTestAppName2[] = "Test ARC App 2";
constexpr char kTestAppPackage[] = "test.arc.app.package";
constexpr char kTestAppActivity[] = "test.arc.app.package.activity";
constexpr char kTestAppActivity2[] = "test.arc.gitapp.package.activity2";

ash::ShelfAction SelectItem(
    const ash::ShelfID& id,
    ui::EventType event_type = ui::ET_MOUSE_PRESSED,
    int64_t display_id = display::kInvalidDisplayId,
    ash::ShelfLaunchSource source = ash::LAUNCH_FROM_UNKNOWN) {
  return SelectShelfItem(id, event_type, display_id, source);
}

std::string GetTestApp1Id(const std::string& package_name) {
  return ArcAppListPrefs::GetAppId(package_name, kTestAppActivity);
}

std::string GetTestApp2Id(const std::string& package_name) {
  return ArcAppListPrefs::GetAppId(package_name, kTestAppActivity2);
}

std::vector<arc::mojom::AppInfoPtr> GetTestAppsList(
    const std::string& package_name,
    bool multi_app) {
  std::vector<arc::mojom::AppInfoPtr> apps;

  arc::mojom::AppInfoPtr app(arc::mojom::AppInfo::New());
  app->name = kTestAppName;
  app->package_name = package_name;
  app->activity = kTestAppActivity;
  app->sticky = false;
  apps.push_back(std::move(app));

  if (multi_app) {
    app = arc::mojom::AppInfo::New();
    app->name = kTestAppName2;
    app->package_name = package_name;
    app->activity = kTestAppActivity2;
    app->sticky = false;
    apps.push_back(std::move(app));
  }

  return apps;
}

std::string CreateIntentUriWithShelfGroupAndLogicalWindow(
    const std::string& shelf_group_id,
    const std::string& logical_window_id) {
  return base::StringPrintf(
      "#Intent;S.org.chromium.arc.logical_window_id=%s;"
      "S.org.chromium.arc.shelf_group_id=%s;end",
      logical_window_id.c_str(), shelf_group_id.c_str());
}

// Creates an exo app window and sets its shell application id. The returned
// Widget is owned by its NativeWidget (the underlying aura::Window).
views::Widget* CreateExoWindow(const std::string& window_app_id) {
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(5, 5, 20, 20);
  params.context = ash::Shell::GetPrimaryRootWindow();
  views::Widget* widget = new views::Widget();
  widget->Init(std::move(params));
  // Set app id before showing the window to be recognized in
  // AppServiceAppWindowLauncherController.
  exo::SetShellApplicationId(widget->GetNativeWindow(), window_app_id);
  widget->Show();
  widget->Activate();
  return widget;
}

}  // namespace

class AppServiceAppWindowBrowserTest
    : public extensions::PlatformAppBrowserTest {
 protected:
  AppServiceAppWindowBrowserTest() = default;

  ~AppServiceAppWindowBrowserTest() override = default;

  void SetUp() override {
    extensions::PlatformAppBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    controller_ = ChromeLauncherController::instance();
    ASSERT_TRUE(controller_);
    extensions::PlatformAppBrowserTest::SetUpOnMainThread();

    app_service_proxy_ = apps::AppServiceProxyFactory::GetForProfile(profile());
    ASSERT_TRUE(app_service_proxy_);
  }

  ash::ShelfModel* shelf_model() { return controller_->shelf_model(); }

  // Returns the last item in the shelf.
  const ash::ShelfItem& GetLastLauncherItem() {
    return shelf_model()->items()[shelf_model()->item_count() - 1];
  }

  apps::InstanceState GetAppInstanceState(const std::string& app_id,
                                          const aura::Window* window) {
    auto instance_state = apps::InstanceState::kUnknown;
    app_service_proxy_->InstanceRegistry().ForOneInstance(
        window, [&app_id, &instance_state](const apps::InstanceUpdate& inner) {
          if (inner.AppId() == app_id) {
            instance_state = inner.State();
          }
        });
    return instance_state;
  }

  ChromeLauncherController* controller_ = nullptr;
  apps::AppServiceProxy* app_service_proxy_ = nullptr;
};

// Test that we have the correct instance for Chrome apps.
IN_PROC_BROWSER_TEST_F(AppServiceAppWindowBrowserTest, ExtensionAppsWindow) {
  const extensions::Extension* app =
      LoadAndLaunchPlatformApp("launch", "Launched");
  extensions::AppWindow* window = CreateAppWindow(profile(), app);
  ASSERT_TRUE(window);

  auto windows = app_service_proxy_->InstanceRegistry().GetWindows(app->id());
  EXPECT_EQ(1u, windows.size());

  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kActive | apps::InstanceState::kVisible,
            GetAppInstanceState(app->id(), *windows.begin()));

  const ash::ShelfItem& item = GetLastLauncherItem();
  // Since it is already active, clicking it should minimize.
  SelectItem(item.id);
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning,
            GetAppInstanceState(app->id(), *windows.begin()));

  // Click the item again to activate the app.
  SelectItem(item.id);
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kActive | apps::InstanceState::kVisible,
            GetAppInstanceState(app->id(), *windows.begin()));

  CloseAppWindow(window);
  windows = app_service_proxy_->InstanceRegistry().GetWindows(app->id());
  EXPECT_EQ(0u, windows.size());
}

// Test that we have the correct instances with more than one window.
IN_PROC_BROWSER_TEST_F(AppServiceAppWindowBrowserTest, MultipleWindows) {
  const extensions::Extension* app =
      LoadAndLaunchPlatformApp("launch", "Launched");
  extensions::AppWindow* app_window1 = CreateAppWindow(profile(), app);

  auto windows = app_service_proxy_->InstanceRegistry().GetWindows(app->id());
  auto* window1 = *windows.begin();

  // Add a second window; confirm the shelf item stays; check the app menu.
  extensions::AppWindow* app_window2 = CreateAppWindow(profile(), app);

  windows = app_service_proxy_->InstanceRegistry().GetWindows(app->id());
  EXPECT_EQ(2u, windows.size());
  aura::Window* window2 = nullptr;
  for (auto* window : windows) {
    if (window != window1)
      window2 = window;
  }

  // The window1 is inactive.
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kVisible,
            GetAppInstanceState(app->id(), window1));

  // The window2 is active.
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kActive | apps::InstanceState::kVisible,
            GetAppInstanceState(app->id(), window2));

  // Close the second window; confirm the shelf item stays; check the app menu.
  CloseAppWindow(app_window2);
  windows = app_service_proxy_->InstanceRegistry().GetWindows(app->id());
  EXPECT_EQ(1u, windows.size());

  // The window1 is active again.
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kActive | apps::InstanceState::kVisible,
            GetAppInstanceState(app->id(), window1));

  // Close the first window; the shelf item should be removed.
  CloseAppWindow(app_window1);
  windows = app_service_proxy_->InstanceRegistry().GetWindows(app->id());
  EXPECT_EQ(0u, windows.size());
}

// Test that we have the correct instances with one HostedApp and one window.
IN_PROC_BROWSER_TEST_F(AppServiceAppWindowBrowserTest,
                       HostedAppandExtensionApp) {
  const extensions::Extension* extension1 = InstallHostedApp();
  LaunchHostedApp(extension1);

  std::string app_id1 = extension1->id();
  auto windows = app_service_proxy_->InstanceRegistry().GetWindows(app_id1);
  EXPECT_EQ(1u, windows.size());
  auto* window1 = *windows.begin();

  // The window1 is active.
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kVisible | apps::InstanceState::kActive,
            GetAppInstanceState(app_id1, window1));

  // Add an Extension app.
  const extensions::Extension* extension2 =
      LoadAndLaunchPlatformApp("launch", "Launched");
  auto* app_window = CreateAppWindow(profile(), extension2);

  std::string app_id2 = extension2->id();
  windows = app_service_proxy_->InstanceRegistry().GetWindows(app_id2);
  EXPECT_EQ(1u, windows.size());
  auto* window2 = *windows.begin();

  // The window1 is inactive.
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kVisible,
            GetAppInstanceState(app_id1, window1));

  // The window2 is active.
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kVisible | apps::InstanceState::kActive,
            GetAppInstanceState(app_id2, window2));

  // Close the Extension app's window..
  CloseAppWindow(app_window);
  windows = app_service_proxy_->InstanceRegistry().GetWindows(app_id2);
  EXPECT_EQ(0u, windows.size());

  // The window1 is active.
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kVisible | apps::InstanceState::kActive,
            GetAppInstanceState(app_id1, window1));

  // Close the HostedApp.
  TabStripModel* tab_strip = browser()->tab_strip_model();
  tab_strip->CloseWebContentsAt(tab_strip->active_index(),
                                TabStripModel::CLOSE_NONE);

  windows = app_service_proxy_->InstanceRegistry().GetWindows(app_id1);
  EXPECT_EQ(0u, windows.size());
}

class AppServiceAppWindowLacrosBrowserTest
    : public AppServiceAppWindowBrowserTest {
 public:
  AppServiceAppWindowLacrosBrowserTest() {
    feature_list_.InitAndEnableFeature(chromeos::features::kLacrosSupport);
  }
  ~AppServiceAppWindowLacrosBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(AppServiceAppWindowLacrosBrowserTest, LacrosWindow) {
  // Create a fake Lacros window. The native window owns the widget.
  views::Widget* widget = CreateExoWindow("org.chromium.lacros.12345");

  using extension_misc::kLacrosAppId;
  auto windows =
      app_service_proxy_->InstanceRegistry().GetWindows(kLacrosAppId);
  EXPECT_EQ(1u, windows.size());
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kActive | apps::InstanceState::kVisible,
            GetAppInstanceState(kLacrosAppId, *windows.begin()));

  // Find the Lacros shelf item.
  int lacros_index = shelf_model()->ItemIndexByAppID(kLacrosAppId);
  ASSERT_NE(-1, lacros_index);
  const ash::ShelfItem& item = shelf_model()->items()[lacros_index];

  // Since it is already active, clicking it should minimize.
  SelectItem(item.id);
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning,
            GetAppInstanceState(kLacrosAppId, *windows.begin()));

  // Click the item again to activate the window.
  SelectItem(item.id);
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kActive | apps::InstanceState::kVisible,
            GetAppInstanceState(kLacrosAppId, *windows.begin()));

  widget->CloseNow();
  windows = app_service_proxy_->InstanceRegistry().GetWindows(kLacrosAppId);
  EXPECT_EQ(0u, windows.size());
}

class AppServiceAppWindowWebAppBrowserTest
    : public AppServiceAppWindowBrowserTest,
      public ::testing::WithParamInterface<web_app::ProviderType> {
 protected:
  AppServiceAppWindowWebAppBrowserTest() {
    if (GetParam() == web_app::ProviderType::kWebApps) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kDesktopPWAsWithoutExtensions);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kDesktopPWAsWithoutExtensions);
    }
  }
  ~AppServiceAppWindowWebAppBrowserTest() override = default;

  // AppServiceAppWindowBrowserTest:
  void SetUpOnMainThread() override {
    AppServiceAppWindowBrowserTest::SetUpOnMainThread();

    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());
  }

  // |SetUpWebApp()| must be called after |SetUpOnMainThread()| to make sure
  // the Network Service process has been setup properly.
  std::string CreateWebApp() const {
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->start_url = GetAppURL();
    web_app_info->scope = GetAppURL().GetWithoutFilename();

    std::string app_id =
        web_app::InstallWebApp(browser()->profile(), std::move(web_app_info));
    CreateWebAppWindow(app_id);
    return app_id;
  }

  GURL GetAppURL() const {
    return https_server_.GetURL("app.com", "/ssl/google.html");
  }

  void CreateWebAppWindow(const std::string& app_id) const {
    content::TestNavigationObserver navigation_observer(GetAppURL());
    navigation_observer.StartWatchingNewWebContents();
    web_app::LaunchWebAppBrowser(browser()->profile(), app_id);
    navigation_observer.WaitForNavigationFinished();
  }

 private:
  // For mocking a secure site.
  net::EmbeddedTestServer https_server_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that we have the correct instance for Web apps.
IN_PROC_BROWSER_TEST_P(AppServiceAppWindowWebAppBrowserTest, WebAppsWindow) {
  std::string app_id = CreateWebApp();

  auto windows = app_service_proxy_->InstanceRegistry().GetWindows(app_id);
  EXPECT_EQ(1u, windows.size());
  aura::Window* window = *windows.begin();
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kActive | apps::InstanceState::kVisible,
            GetAppInstanceState(app_id, window));

  const ash::ShelfItem& item = GetLastLauncherItem();
  // Since it is already active, clicking it should minimize.
  SelectItem(item.id);
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning,
            GetAppInstanceState(app_id, window));

  // Click the item again to activate the app.
  SelectItem(item.id);
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kActive | apps::InstanceState::kVisible,
            GetAppInstanceState(app_id, window));

  controller_->Close(item.id);
  // Make sure that the window is closed.
  base::RunLoop().RunUntilIdle();
  windows = app_service_proxy_->InstanceRegistry().GetWindows(app_id);
  EXPECT_EQ(0u, windows.size());
}

// Tests that web app with multiple open windows can be activated from the app
// list.
IN_PROC_BROWSER_TEST_P(AppServiceAppWindowWebAppBrowserTest,
                       LaunchFromAppList) {
  std::string app_id = CreateWebApp();

  auto windows = app_service_proxy_->InstanceRegistry().GetWindows(app_id);
  ASSERT_EQ(1u, windows.size());
  aura::Window* window1 = *windows.begin();
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kActive | apps::InstanceState::kVisible,
            GetAppInstanceState(app_id, window1));

  const ash::ShelfItem item = GetLastLauncherItem();
  // Since it is already active, clicking it should minimize.
  SelectItem(item.id);
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning,
            GetAppInstanceState(app_id, window1));

  // Create another window.
  CreateWebAppWindow(app_id);
  windows = app_service_proxy_->InstanceRegistry().GetWindows(app_id);
  ASSERT_EQ(2u, windows.size());
  aura::Window* window2 =
      *windows.begin() == window1 ? *windows.rbegin() : *windows.begin();

  ASSERT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kActive | apps::InstanceState::kVisible,
            GetAppInstanceState(app_id, window2));

  // Bring the browser window to foreground.
  browser()->window()->Show();

  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kVisible,
            GetAppInstanceState(app_id, window2));

  // Launching the first app from the app list should activate it.
  SelectItem(item.id, ui::ET_MOUSE_PRESSED, display::kInvalidDisplayId,
             ash::LAUNCH_FROM_APP_LIST);

  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kActive | apps::InstanceState::kVisible,
            GetAppInstanceState(app_id, window1));

  // Selecting an active app from the app list should not minimize it.
  SelectItem(item.id, ui::ET_MOUSE_PRESSED, display::kInvalidDisplayId,
             ash::LAUNCH_FROM_APP_LIST);

  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kActive | apps::InstanceState::kVisible,
            GetAppInstanceState(app_id, window1));
}

class AppServiceAppWindowArcAppBrowserTest
    : public AppServiceAppWindowBrowserTest {
 protected:
  // AppServiceAppWindowBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    AppServiceAppWindowBrowserTest::SetUpCommandLine(command_line);
    arc::SetArcAvailableCommandLineForTesting(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    AppServiceAppWindowBrowserTest::SetUpInProcessBrowserTestFixture();
    arc::ArcSessionManager::SetUiEnabledForTesting(false);
  }

  void SetUpOnMainThread() override {
    AppServiceAppWindowBrowserTest::SetUpOnMainThread();
    arc::SetArcPlayStoreEnabledForProfile(profile(), true);

    // This ensures app_prefs()->GetApp() below never returns nullptr.
    base::RunLoop run_loop;
    app_prefs()->SetDefaultAppsReadyCallback(run_loop.QuitClosure());
    run_loop.Run();
  }

  void InstallTestApps(const std::string& package_name, bool multi_app) {
    app_host()->OnAppListRefreshed(GetTestAppsList(package_name, multi_app));

    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
        app_prefs()->GetApp(GetTestApp1Id(package_name));
    ASSERT_TRUE(app_info);
    EXPECT_TRUE(app_info->ready);
    if (multi_app) {
      std::unique_ptr<ArcAppListPrefs::AppInfo> app_info2 =
          app_prefs()->GetApp(GetTestApp2Id(package_name));
      ASSERT_TRUE(app_info2);
      EXPECT_TRUE(app_info2->ready);
    }
  }

  void SendPackageAdded(const std::string& package_name, bool package_synced) {
    arc::mojom::ArcPackageInfo package_info;
    package_info.package_name = package_name;
    package_info.package_version = 1;
    package_info.last_backup_android_id = 1;
    package_info.last_backup_time = 1;
    package_info.sync = package_synced;
    package_info.system = false;
    app_host()->OnPackageAdded(arc::mojom::ArcPackageInfo::From(package_info));

    base::RunLoop().RunUntilIdle();
  }

  void StartInstance() {
    app_instance_ = std::make_unique<arc::FakeAppInstance>(app_host());
    arc_brige_service()->app()->SetInstance(app_instance_.get());
  }

  void StopInstance() {
    if (app_instance_)
      arc_brige_service()->app()->CloseInstance(app_instance_.get());
    arc_session_manager()->Shutdown();
  }

  ArcAppListPrefs* app_prefs() { return ArcAppListPrefs::Get(profile()); }

  // Returns as AppHost interface in order to access to private implementation
  // of the interface.
  arc::mojom::AppHost* app_host() { return app_prefs(); }

 private:
  arc::ArcSessionManager* arc_session_manager() {
    return arc::ArcSessionManager::Get();
  }

  arc::ArcBridgeService* arc_brige_service() {
    return arc::ArcServiceManager::Get()->arc_bridge_service();
  }

  std::unique_ptr<arc::FakeAppInstance> app_instance_;
};

// Test that we have the correct instance for ARC apps.
IN_PROC_BROWSER_TEST_F(AppServiceAppWindowArcAppBrowserTest, ArcAppsWindow) {
  // Install app to remember existing apps.
  StartInstance();
  InstallTestApps(kTestAppPackage, true);
  SendPackageAdded(kTestAppPackage, false);

  // Create the window for app1.
  views::Widget* arc_window1 = CreateExoWindow("org.chromium.arc.1");
  const std::string app_id1 = GetTestApp1Id(kTestAppPackage);

  // Simulate task creation so the app is marked as running/open.
  std::unique_ptr<ArcAppListPrefs::AppInfo> info = app_prefs()->GetApp(app_id1);
  app_host()->OnTaskCreated(1, info->package_name, info->activity, info->name,
                            info->intent_uri);
  EXPECT_TRUE(controller_->GetItem(ash::ShelfID(app_id1)));

  // Check the window state in instance for app1
  auto windows = app_service_proxy_->InstanceRegistry().GetWindows(app_id1);
  EXPECT_EQ(1u, windows.size());
  aura::Window* window1 = *windows.begin();
  apps::InstanceState latest_state =
      app_service_proxy_->InstanceRegistry().GetState(window1);
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning,
            latest_state);

  app_host()->OnTaskSetActive(1);
  latest_state = app_service_proxy_->InstanceRegistry().GetState(window1);
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kActive | apps::InstanceState::kVisible,
            latest_state);
  controller_->PinAppWithID(app_id1);

  // Create the task id for app2 first, then create the window.
  const std::string app_id2 = GetTestApp2Id(kTestAppPackage);
  info = app_prefs()->GetApp(app_id2);
  app_host()->OnTaskCreated(2, info->package_name, info->activity, info->name,
                            info->intent_uri);
  views::Widget* arc_window2 = CreateExoWindow("org.chromium.arc.2");
  EXPECT_TRUE(controller_->GetItem(ash::ShelfID(app_id2)));

  // Check the window state in instance for app2
  windows = app_service_proxy_->InstanceRegistry().GetWindows(app_id2);
  EXPECT_EQ(1u, windows.size());
  aura::Window* window2 = *windows.begin();
  latest_state = app_service_proxy_->InstanceRegistry().GetState(window2);
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kActive | apps::InstanceState::kVisible,
            latest_state);

  // App1 is inactive.
  latest_state = app_service_proxy_->InstanceRegistry().GetState(window1);
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kVisible,
            latest_state);

  // Select the app1
  SelectItem(ash::ShelfID(app_id1));
  latest_state = app_service_proxy_->InstanceRegistry().GetState(window1);
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kActive | apps::InstanceState::kVisible,
            latest_state);
  latest_state = app_service_proxy_->InstanceRegistry().GetState(window2);
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kVisible,
            latest_state);

  // Close the window for app1, and destroy the task.
  arc_window1->CloseNow();
  app_host()->OnTaskDestroyed(1);
  windows = app_service_proxy_->InstanceRegistry().GetWindows(app_id1);
  EXPECT_EQ(0u, windows.size());

  // App2 is activated.
  latest_state = app_service_proxy_->InstanceRegistry().GetState(window2);
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kActive | apps::InstanceState::kVisible,
            latest_state);

  // destroy the task for app2 and close the window.
  app_host()->OnTaskDestroyed(2);
  arc_window2->CloseNow();
  windows = app_service_proxy_->InstanceRegistry().GetWindows(app_id2);
  EXPECT_EQ(0u, windows.size());

  StopInstance();
}

// Test what happens when the logical window ID is provided, and some window
// might be hidden in the shelf.
IN_PROC_BROWSER_TEST_F(AppServiceAppWindowArcAppBrowserTest, LogicalWindowId) {
  // Install app to remember existing apps.
  StartInstance();
  InstallTestApps(kTestAppPackage, true);
  SendPackageAdded(kTestAppPackage, false);

  // Create the windows for the app.
  views::Widget* arc_window1 = CreateExoWindow("org.chromium.arc.1");
  views::Widget* arc_window2 = CreateExoWindow("org.chromium.arc.2");

  // Simulate task creation so the app is marked as running/open.
  const std::string app_id = GetTestApp1Id(kTestAppPackage);
  std::unique_ptr<ArcAppListPrefs::AppInfo> info = app_prefs()->GetApp(app_id);
  app_host()->OnTaskCreated(1, info->package_name, info->activity, info->name,
                            CreateIntentUriWithShelfGroupAndLogicalWindow(
                                "shelf_group_1", "logical_window_1"));
  app_host()->OnTaskCreated(2, info->package_name, info->activity, info->name,
                            CreateIntentUriWithShelfGroupAndLogicalWindow(
                                "shelf_group_1", "logical_window_1"));

  // Both windows should show up in the instance registry.
  auto windows = app_service_proxy_->InstanceRegistry().GetWindows(app_id);
  EXPECT_EQ(2u, windows.size());

  // Of those two, one should be hidden.
  auto is_hidden = [](aura::Window* w) {
    return w->GetProperty(ash::kHideInShelfKey);
  };
  EXPECT_EQ(1, std::count_if(windows.begin(), windows.end(), is_hidden));

  // The hidden window should be task_id 2.
  aura::Window* window1 =
      *(std::find_if_not(windows.begin(), windows.end(), is_hidden));
  aura::Window* window2 =
      *(std::find_if(windows.begin(), windows.end(), is_hidden));

  apps::InstanceState latest_state =
      app_service_proxy_->InstanceRegistry().GetState(window1);
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning,
            latest_state);
  latest_state = app_service_proxy_->InstanceRegistry().GetState(window2);
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning,
            latest_state);

  // If the user focuses window 2, it should become active, but still hidden in
  // the shelf.
  app_host()->OnTaskSetActive(2);
  latest_state = app_service_proxy_->InstanceRegistry().GetState(window2);
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kActive | apps::InstanceState::kVisible,
            latest_state);
  EXPECT_TRUE(window2->GetProperty(ash::kHideInShelfKey));

  // Close first window. No window should be hidden anymore.
  arc_window1->CloseNow();
  app_host()->OnTaskDestroyed(1);
  windows = app_service_proxy_->InstanceRegistry().GetWindows(app_id);
  EXPECT_EQ(1u, windows.size());
  EXPECT_EQ(0, std::count_if(windows.begin(), windows.end(), is_hidden));

  // Close second window.
  app_host()->OnTaskDestroyed(2);
  arc_window2->CloseNow();
  windows = app_service_proxy_->InstanceRegistry().GetWindows(app_id);
  EXPECT_EQ(0u, windows.size());
}

INSTANTIATE_TEST_SUITE_P(All,
                         AppServiceAppWindowWebAppBrowserTest,
                         ::testing::Values(web_app::ProviderType::kBookmarkApps,
                                           web_app::ProviderType::kWebApps),
                         web_app::ProviderTypeParamToString);
