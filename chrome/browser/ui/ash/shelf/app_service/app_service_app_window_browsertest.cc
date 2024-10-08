// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/components/arc/test/fake_app_instance.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/web_app_id_constants.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_service_factory.h"
#include "chrome/browser/ash/borealis/borealis_window_manager.h"
#include "chrome/browser/ash/borealis/borealis_window_manager_mock.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/components/standalone_browser/feature_refs.h"
#include "components/app_constants/constants.h"
#include "components/exo/shell_surface_util.h"
#include "components/services/app_service/public/cpp/app_instance_waiter.h"
#include "components/services/app_service/public/cpp/instance.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/views/widget/widget.h"

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
constexpr char kTestPaymentAppName[] = "Test ARC Payment App";
constexpr char kTestAppPackage[] = "test.arc.app.package";
constexpr char kTestAppActivity[] = "test.arc.app.package.activity";
constexpr char kTestAppActivity2[] = "test.arc.gitapp.package.activity2";
constexpr char kTestPaymentAppPackage[] = "org.chromium.arc.payment_app";
constexpr char kTestPaymentAppActivity[] =
    "org.chromium.arc.payment_app.InvokePaymentAppActivity";

ash::ShelfAction SelectItem(
    const ash::ShelfID& id,
    ui::EventType event_type = ui::EventType::kMousePressed,
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

std::string GetTestPaymentAppId(const std::string& package_name) {
  return ArcAppListPrefs::GetAppId(package_name, kTestPaymentAppActivity);
}

std::vector<arc::mojom::AppInfoPtr> GetTestAppsList(
    const std::string& package_name,
    bool multi_app,
    bool payment_app) {
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

  if (payment_app) {
    app = arc::mojom::AppInfo::New();
    app->name = kTestPaymentAppName;
    app->package_name = package_name;
    app->activity = kTestPaymentAppActivity;
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
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(5, 5, 20, 20);
  params.context = ash::Shell::GetPrimaryRootWindow();
  views::Widget* widget = new views::Widget();
  widget->Init(std::move(params));
  widget->GetNativeWindow()->SetTitle(u"foo");
  // Set app id before showing the window to be recognized in
  // AppServiceAppWindowShelfController.
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

  void SetUp() override { extensions::PlatformAppBrowserTest::SetUp(); }

  void SetUpOnMainThread() override {
    controller_ = ChromeShelfController::instance();
    ASSERT_TRUE(controller_);
    extensions::PlatformAppBrowserTest::SetUpOnMainThread();

    app_service_proxy_ = apps::AppServiceProxyFactory::GetForProfile(profile());
    ASSERT_TRUE(app_service_proxy_);
  }

  ash::ShelfModel* shelf_model() { return controller_->shelf_model(); }

  // Returns the last item in the shelf.
  const ash::ShelfItem& GetLastShelfItem() {
    return shelf_model()->items()[shelf_model()->item_count() - 1];
  }

  apps::InstanceState GetAppInstanceState(const std::string& app_id,
                                          const aura::Window* window) {
    std::set<apps::InstanceState> states;
    app_service_proxy_->InstanceRegistry().ForInstancesWithWindow(
        window, [&](const apps::InstanceUpdate& update) {
          if (update.AppId() == app_id) {
            states.insert(update.State());
          }
        });
    if (states.size() == 1)
      return *states.begin();
    return apps::InstanceState::kUnknown;
  }

  raw_ptr<ChromeShelfController, DanglingUntriaged> controller_ = nullptr;
  raw_ptr<apps::AppServiceProxy, DanglingUntriaged> app_service_proxy_ =
      nullptr;
};

// Test that we have the correct instance for Chrome apps.
IN_PROC_BROWSER_TEST_F(AppServiceAppWindowBrowserTest, ExtensionAppsWindow) {
  const extensions::Extension* app =
      LoadAndLaunchPlatformApp("launch", "Launched");
  extensions::AppWindow* app_window = CreateAppWindow(profile(), app);
  ASSERT_TRUE(app_window);

  auto instances =
      app_service_proxy_->InstanceRegistry().GetInstances(app->id());
  EXPECT_EQ(1u, instances.size());
  auto* window = (*instances.begin())->Window();

  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kActive | apps::InstanceState::kVisible,
            GetAppInstanceState(app->id(), window));

  const ash::ShelfItem& item = GetLastShelfItem();
  // Since it is already active, clicking it should minimize.
  SelectItem(item.id);
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning,
            GetAppInstanceState(app->id(), window));

  // Click the item again to activate the app.
  SelectItem(item.id);
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kActive | apps::InstanceState::kVisible,
            GetAppInstanceState(app->id(), window));

  CloseAppWindow(app_window);
  instances = app_service_proxy_->InstanceRegistry().GetInstances(app->id());
  EXPECT_TRUE(instances.empty());
}

// Test that we have the correct instances with more than one window.
IN_PROC_BROWSER_TEST_F(AppServiceAppWindowBrowserTest, MultipleWindows) {
  const extensions::Extension* app =
      LoadAndLaunchPlatformApp("launch", "Launched");
  extensions::AppWindow* app_window1 = CreateAppWindow(profile(), app);

  auto instances =
      app_service_proxy_->InstanceRegistry().GetInstances(app->id());
  auto* window1 = (*instances.begin())->Window();

  // Add a second window; confirm the shelf item stays; check the app menu.
  extensions::AppWindow* app_window2 = CreateAppWindow(profile(), app);

  instances = app_service_proxy_->InstanceRegistry().GetInstances(app->id());
  EXPECT_EQ(2u, instances.size());
  aura::Window* window2 = nullptr;
  for (const apps::Instance* instance : instances) {
    if (instance->Window() != window1) {
      window2 = instance->Window();
    }
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
  instances = app_service_proxy_->InstanceRegistry().GetInstances(app->id());
  EXPECT_EQ(1u, instances.size());

  // The window1 is active again.
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kActive | apps::InstanceState::kVisible,
            GetAppInstanceState(app->id(), window1));

  // Close the first window; the shelf item should be removed.
  CloseAppWindow(app_window1);
  instances = app_service_proxy_->InstanceRegistry().GetInstances(app->id());
  EXPECT_TRUE(instances.empty());
}

// Test that we have the correct instances with one HostedApp and one window.
IN_PROC_BROWSER_TEST_F(AppServiceAppWindowBrowserTest,
                       HostedAppandExtensionApp) {
  const extensions::Extension* extension1 = InstallHostedApp();
  LaunchHostedApp(extension1);

  std::string app_id1 = extension1->id();
  auto instances = app_service_proxy_->InstanceRegistry().GetInstances(app_id1);
  EXPECT_EQ(1u, instances.size());
  auto* instance1 = (*instances.begin()).get();
  EXPECT_NE(instance1->Window(), instance1->Window()->GetToplevelWindow());

  // The window1 is active.
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kVisible | apps::InstanceState::kActive,
            GetAppInstanceState(app_id1, instance1->Window()));

  // Add an Extension app.
  const extensions::Extension* extension2 =
      LoadAndLaunchPlatformApp("launch", "Launched");
  auto* app_window = CreateAppWindow(profile(), extension2);

  std::string app_id2 = extension2->id();
  instances = app_service_proxy_->InstanceRegistry().GetInstances(app_id2);
  EXPECT_EQ(1u, instances.size());
  auto* instance2 = (*instances.begin()).get();
  EXPECT_EQ(instance2->Window(), instance2->Window()->GetToplevelWindow());

  // The window1 is inactive.
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kVisible,
            GetAppInstanceState(app_id1, instance1->Window()));

  // The window2 is active.
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kVisible | apps::InstanceState::kActive,
            GetAppInstanceState(app_id2, instance2->Window()));

  // Close the Extension app's window..
  CloseAppWindow(app_window);
  instances = app_service_proxy_->InstanceRegistry().GetInstances(app_id2);
  EXPECT_TRUE(instances.empty());

  // The window1 is active.
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kVisible | apps::InstanceState::kActive,
            GetAppInstanceState(app_id1, instance1->Window()));

  // Close the HostedApp.
  TabStripModel* tab_strip = browser()->tab_strip_model();
  tab_strip->CloseWebContentsAt(tab_strip->active_index(),
                                TabCloseTypes::CLOSE_NONE);

  instances = app_service_proxy_->InstanceRegistry().GetInstances(app_id1);
  EXPECT_TRUE(instances.empty());
}

IN_PROC_BROWSER_TEST_F(AppServiceAppWindowBrowserTest, AshBrowserWindow) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("chrome://blank")));

  auto instances = app_service_proxy_->InstanceRegistry().GetInstances(
      app_constants::kChromeAppId);
  EXPECT_EQ(1u, instances.size());
  auto* instance = (*instances.begin()).get();
  EXPECT_EQ(instance->Window(), instance->Window()->GetToplevelWindow());
}

class AppServiceAppWindowBorealisBrowserTest
    : public AppServiceAppWindowBrowserTest {
 public:
  ~AppServiceAppWindowBorealisBrowserTest() override = default;

  std::string MakeBorealisApp(const std::string& vm,
                              const std::string& container,
                              const std::string& name) {
    vm_tools::apps::ApplicationList list;
    list.set_vm_name(vm);
    list.set_container_name(container);
    list.set_vm_type(vm_tools::apps::BOREALIS);
    vm_tools::apps::App* app = list.add_apps();
    app->set_desktop_file_id(name);
    app->mutable_name()->add_values()->set_value(name);
    app->set_no_display(false);
    guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile())
        ->UpdateApplicationList(list);

    return guest_os::GuestOsRegistryService::GenerateAppId(name, vm, container);
  }
};

IN_PROC_BROWSER_TEST_F(AppServiceAppWindowBorealisBrowserTest,
                       BorealisKnownApp) {
  // Generate a fake app.
  std::string app_id = MakeBorealisApp("vm", "container", "foo");

  views::Widget* widget =
      CreateExoWindow("org.chromium.guest_os.borealis.wmclass.foo");

  EXPECT_EQ(1u,
            app_service_proxy_->InstanceRegistry().GetInstances(app_id).size());
  EXPECT_NE(-1, shelf_model()->ItemIndexByAppID(app_id));

  widget->CloseNow();
  EXPECT_TRUE(
      app_service_proxy_->InstanceRegistry().GetInstances(app_id).empty());
}

IN_PROC_BROWSER_TEST_F(AppServiceAppWindowBorealisBrowserTest,
                       BorealisUnknownApp) {
  views::Widget* widget =
      CreateExoWindow("org.chromium.guest_os.borealis.wmclass.bar");
  std::string app_id =
      "borealis_anon:org.chromium.guest_os.borealis.wmclass.bar";

  EXPECT_EQ(1u,
            app_service_proxy_->InstanceRegistry().GetInstances(app_id).size());
  ASSERT_NE(-1, shelf_model()->ItemIndexByAppID(app_id));

  // Now that the app is published, it will have a name based on the window title
  EXPECT_EQ(
      "foo",
      base::UTF16ToUTF8(shelf_model()
                            ->items()[shelf_model()->ItemIndexByAppID(app_id)]
                            .title));

  widget->CloseNow();
  EXPECT_TRUE(
      app_service_proxy_->InstanceRegistry().GetInstances(app_id).empty());
}

IN_PROC_BROWSER_TEST_F(AppServiceAppWindowBorealisBrowserTest,
                       BorealisSession) {
  std::string app_id = MakeBorealisApp("vm", "container", "foo");

  testing::StrictMock<borealis::MockLifetimeObserver> observer;
  base::ScopedObservation<
      borealis::BorealisWindowManager,
      borealis::BorealisWindowManager::AppWindowLifetimeObserver>
      observation(&observer);
  observation.Observe(
      &borealis::BorealisServiceFactory::GetForProfile(profile())
           ->WindowManager());

  testing::InSequence sequence;
  EXPECT_CALL(observer, OnSessionStarted());
  EXPECT_CALL(observer, OnAppStarted(app_id));
  EXPECT_CALL(observer, OnWindowStarted(app_id, testing::_));
  views::Widget* widget =
      CreateExoWindow("org.chromium.guest_os.borealis.wmclass.foo");

  EXPECT_CALL(observer, OnWindowFinished(app_id, widget->GetNativeWindow()));
  EXPECT_CALL(observer, OnAppFinished(app_id, widget->GetNativeWindow()));
  EXPECT_CALL(observer, OnSessionFinished());
  widget->CloseNow();
}

class AppServiceAppWindowWebAppBrowserTest
    : public AppServiceAppWindowBrowserTest {
 protected:
  AppServiceAppWindowWebAppBrowserTest() = default;
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
    auto web_app_info =
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(GetAppURL());
    web_app_info->scope = GetAppURL().GetWithoutFilename();

    std::string app_id = web_app::test::InstallWebApp(browser()->profile(),
                                                      std::move(web_app_info));
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
};

// Test that we have the correct instance for Web apps.
IN_PROC_BROWSER_TEST_F(AppServiceAppWindowWebAppBrowserTest, WebAppsWindow) {
  std::string app_id = CreateWebApp();

  auto instances = app_service_proxy_->InstanceRegistry().GetInstances(app_id);
  EXPECT_EQ(1u, instances.size());
  EXPECT_NE((*instances.begin())->Window(),
            (*instances.begin())->Window()->GetToplevelWindow());
  const auto* instance = (*instances.begin()).get();
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kActive | apps::InstanceState::kVisible,
            GetAppInstanceState(app_id, instance->Window()));

  const ash::ShelfItem& item = GetLastShelfItem();
  // Since it is already active, clicking it should minimize.
  SelectItem(item.id);
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning,
            GetAppInstanceState(app_id, instance->Window()));

  // Click the item again to activate the app.
  SelectItem(item.id);
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kActive | apps::InstanceState::kVisible,
            GetAppInstanceState(app_id, instance->Window()));

  controller_->Close(item.id);
  // Make sure that the window is closed.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(
      app_service_proxy_->InstanceRegistry().GetInstances(app_id).empty());
}

// Tests that web app with multiple open windows can be activated from the app
// list.
IN_PROC_BROWSER_TEST_F(AppServiceAppWindowWebAppBrowserTest,
                       LaunchFromAppList) {
  std::string app_id = CreateWebApp();

  auto instances = app_service_proxy_->InstanceRegistry().GetInstances(app_id);
  ASSERT_EQ(1u, instances.size());
  const auto* instance1 = (*instances.begin()).get();
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kActive | apps::InstanceState::kVisible,
            GetAppInstanceState(app_id, instance1->Window()));

  const ash::ShelfItem item = GetLastShelfItem();
  // Since it is already active, clicking it should minimize.
  SelectItem(item.id);
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning,
            GetAppInstanceState(app_id, instance1->Window()));

  // Create another window.
  CreateWebAppWindow(app_id);
  instances = app_service_proxy_->InstanceRegistry().GetInstances(app_id);
  ASSERT_EQ(2u, instances.size());
  const auto* instance2 = *instances.begin() == instance1
                              ? (*instances.rbegin()).get()
                              : (*instances.begin()).get();

  ASSERT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kActive | apps::InstanceState::kVisible,
            GetAppInstanceState(app_id, instance2->Window()));

  // Bring the browser window to foreground.
  browser()->window()->Show();

  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kVisible,
            GetAppInstanceState(app_id, instance2->Window()));

  // Launching the first app from the app list should activate it.
  SelectItem(item.id, ui::EventType::kMousePressed, display::kInvalidDisplayId,
             ash::LAUNCH_FROM_APP_LIST);

  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kActive | apps::InstanceState::kVisible,
            GetAppInstanceState(app_id, instance1->Window()));

  // Selecting an active app from the app list should not minimize it.
  SelectItem(item.id, ui::EventType::kMousePressed, display::kInvalidDisplayId,
             ash::LAUNCH_FROM_APP_LIST);

  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kActive | apps::InstanceState::kVisible,
            GetAppInstanceState(app_id, instance1->Window()));
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

  void InstallTestApps(const std::string& package_name,
                       bool multi_app,
                       bool payment_app) {
    app_host()->OnAppListRefreshed(
        GetTestAppsList(package_name, multi_app, payment_app));

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
    if (payment_app) {
      std::unique_ptr<ArcAppListPrefs::AppInfo> payment_app_info =
          app_prefs()->GetApp(GetTestPaymentAppId(package_name));
      ASSERT_TRUE(payment_app_info);
      EXPECT_TRUE(payment_app_info->ready);
    }
  }

  void SendPackageAdded(const std::string& package_name, bool package_synced) {
    arc::mojom::ArcPackageInfo package_info;
    package_info.package_name = package_name;
    package_info.package_version = 1;
    package_info.last_backup_android_id = 1;
    package_info.last_backup_time = 1;
    package_info.sync = package_synced;
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
  InstallTestApps(kTestAppPackage, true, false);
  SendPackageAdded(kTestAppPackage, false);

  // Create the window for app1.
  views::Widget* arc_window1 = CreateExoWindow("org.chromium.arc.1");
  const std::string app_id1 = GetTestApp1Id(kTestAppPackage);

  // Simulate task creation so the app is marked as running/open.
  std::unique_ptr<ArcAppListPrefs::AppInfo> info = app_prefs()->GetApp(app_id1);
  app_host()->OnTaskCreated(1, info->package_name, info->activity, info->name,
                            info->intent_uri, /*session_id=*/0);
  EXPECT_TRUE(controller_->GetItem(ash::ShelfID(app_id1)));

  // Check the window state in instance for app1
  auto instances = app_service_proxy_->InstanceRegistry().GetInstances(app_id1);
  EXPECT_EQ(1u, instances.size());
  aura::Window* window1 = (*instances.begin())->Window();
  apps::InstanceState latest_state =
      app_service_proxy_->InstanceRegistry().GetState(window1);
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning,
            latest_state);

  app_host()->OnTaskSetActive(1);
  latest_state = app_service_proxy_->InstanceRegistry().GetState(window1);
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kActive | apps::InstanceState::kVisible,
            latest_state);
  controller_->shelf_model()->PinExistingItemWithID(app_id1);

  // Create the task id for app2 first, then create the window.
  const std::string app_id2 = GetTestApp2Id(kTestAppPackage);
  info = app_prefs()->GetApp(app_id2);
  app_host()->OnTaskCreated(2, info->package_name, info->activity, info->name,
                            info->intent_uri, /*session_id=*/0);
  views::Widget* arc_window2 = CreateExoWindow("org.chromium.arc.2");
  EXPECT_TRUE(controller_->GetItem(ash::ShelfID(app_id2)));

  // Check the window state in instance for app2
  instances = app_service_proxy_->InstanceRegistry().GetInstances(app_id2);
  EXPECT_EQ(1u, instances.size());
  aura::Window* window2 = (*instances.begin())->Window();
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
  EXPECT_TRUE(
      app_service_proxy_->InstanceRegistry().GetInstances(app_id1).empty());

  // App2 is activated.
  latest_state = app_service_proxy_->InstanceRegistry().GetState(window2);
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kActive | apps::InstanceState::kVisible,
            latest_state);

  // destroy the task for app2 and close the window.
  app_host()->OnTaskDestroyed(2);
  arc_window2->CloseNow();
  EXPECT_TRUE(
      app_service_proxy_->InstanceRegistry().GetInstances(app_id2).empty());

  StopInstance();
}

// Test what happens when the logical window ID is provided, and some window
// might be hidden in the shelf.
IN_PROC_BROWSER_TEST_F(AppServiceAppWindowArcAppBrowserTest, LogicalWindowId) {
  // Install app to remember existing apps.
  StartInstance();
  InstallTestApps(kTestAppPackage, true, false);
  SendPackageAdded(kTestAppPackage, false);

  // Create the windows for the app.
  views::Widget* arc_window1 = CreateExoWindow("org.chromium.arc.1");
  views::Widget* arc_window2 = CreateExoWindow("org.chromium.arc.2");

  // Simulate task creation so the app is marked as running/open.
  const std::string app_id = GetTestApp1Id(kTestAppPackage);
  std::unique_ptr<ArcAppListPrefs::AppInfo> info = app_prefs()->GetApp(app_id);
  app_host()->OnTaskCreated(1, info->package_name, info->activity, info->name,
                            CreateIntentUriWithShelfGroupAndLogicalWindow(
                                "shelf_group_1", "logical_window_1"),
                            /*session_id=*/0);
  app_host()->OnTaskCreated(2, info->package_name, info->activity, info->name,
                            CreateIntentUriWithShelfGroupAndLogicalWindow(
                                "shelf_group_1", "logical_window_1"),
                            /*session_id=*/0);

  // Both windows should show up in the instance registry.
  auto instances = app_service_proxy_->InstanceRegistry().GetInstances(app_id);
  EXPECT_EQ(2u, instances.size());

  // Of those two, one should be hidden.
  auto is_hidden = [](const apps::Instance* instance) {
    return instance->Window()->GetProperty(ash::kHideInShelfKey);
  };
  EXPECT_EQ(1, base::ranges::count_if(instances, is_hidden));

  // The hidden window should be task_id 2.
  aura::Window* window1 =
      (*(base::ranges::find_if_not(instances, is_hidden)))->Window();
  aura::Window* window2 =
      (*(base::ranges::find_if(instances, is_hidden)))->Window();

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
  instances = app_service_proxy_->InstanceRegistry().GetInstances(app_id);
  EXPECT_EQ(1u, instances.size());
  EXPECT_EQ(0, base::ranges::count_if(instances, is_hidden));

  // Close second window.
  app_host()->OnTaskDestroyed(2);
  arc_window2->CloseNow();
  EXPECT_TRUE(
      app_service_proxy_->InstanceRegistry().GetInstances(app_id).empty());
}

// Test what happens when ARC is used to launch a payment task for a PWA app,
// as the ARC task should not be shown on the shelf.
IN_PROC_BROWSER_TEST_F(AppServiceAppWindowArcAppBrowserTest, PaymentApp) {
  // Install app to remember existing apps.
  StartInstance();
  InstallTestApps(kTestPaymentAppPackage, false, true);
  SendPackageAdded(kTestPaymentAppPackage, false);

  // Create the windows for the payment app.
  views::Widget* payment_window = CreateExoWindow("org.chromium.arc.1");

  // Simulate task creation so the app is marked as running/open.
  const std::string payment_app_id =
      GetTestPaymentAppId(kTestPaymentAppPackage);
  std::unique_ptr<ArcAppListPrefs::AppInfo> info =
      app_prefs()->GetApp(payment_app_id);
  app_host()->OnTaskCreated(1, info->package_name, info->activity, info->name,
                            info->intent_uri, /*session_id=*/0);

  // There should NOT be an entry on the shelf for the payment task
  EXPECT_FALSE(controller_->GetItem(ash::ShelfID(payment_app_id)));

  // The payment window should still show up in the instance registry.
  auto instances =
      app_service_proxy_->InstanceRegistry().GetInstances(payment_app_id);
  EXPECT_EQ(1u, instances.size());

  // The payment window should be hidden
  auto is_hidden = [](const apps::Instance* instance) {
    return instance->Window()->GetProperty(ash::kHideInShelfKey);
  };
  EXPECT_EQ(1, base::ranges::count_if(instances, is_hidden));

  // No windows should remain if we close the payment window
  payment_window->CloseNow();
  app_host()->OnTaskDestroyed(1);
  EXPECT_TRUE(app_service_proxy_->InstanceRegistry()
                  .GetInstances(payment_app_id)
                  .empty());
}

using AppServiceAppWindowSystemWebAppBrowserTest =
    AppServiceAppWindowWebAppBrowserTest;

IN_PROC_BROWSER_TEST_F(AppServiceAppWindowSystemWebAppBrowserTest,
                       SystemWebAppWindow) {
  ash::SystemWebAppManager::GetForTest(browser()->profile())
      ->InstallSystemAppsForTesting();

  const std::string app_id = web_app::kOsSettingsAppId;
  web_app::LaunchWebAppBrowser(browser()->profile(), app_id);

  auto instances = app_service_proxy_->InstanceRegistry().GetInstances(app_id);
  EXPECT_EQ(1u, instances.size());
  const auto* instance = (*instances.begin()).get();
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kActive | apps::InstanceState::kVisible,
            GetAppInstanceState(app_id, instance->Window()));

  const ash::ShelfItem& item = GetLastShelfItem();
  // Since it is already active, clicking it should minimize.
  SelectItem(item.id);
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning,
            GetAppInstanceState(app_id, instance->Window()));

  // Click the item again to activate the app.
  SelectItem(item.id);
  EXPECT_EQ(apps::InstanceState::kStarted | apps::InstanceState::kRunning |
                apps::InstanceState::kActive | apps::InstanceState::kVisible,
            GetAppInstanceState(app_id, instance->Window()));

  ui_test_utils::BrowserChangeObserver browser_close_observer(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kRemoved);
  controller_->Close(item.id);
  // Make sure that the window is closed.
  browser_close_observer.Wait();
  instances = app_service_proxy_->InstanceRegistry().GetInstances(app_id);
  EXPECT_TRUE(instances.empty());
}
