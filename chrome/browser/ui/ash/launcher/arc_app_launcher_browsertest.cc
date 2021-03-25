// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_app_button.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_service_launcher.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/app_list/app_list_client_impl.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/launcher/arc_app_shelf_id.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_test_util.h"
#include "chrome/browser/ui/ash/launcher/shelf_spinner_controller.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/arc_util.h"
#include "components/arc/metrics/arc_metrics_constants.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/arc/test/fake_app_instance.h"
#include "components/exo/shell_surface.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/test/shell_surface_builder.h"
#include "components/exo/wm_helper.h"
#include "components/exo/wm_helper_chromeos.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/event_constants.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/types/event_type.h"
#include "ui/views/animation/ink_drop.h"

namespace mojo {

template <>
struct TypeConverter<arc::mojom::AppInfoPtr, arc::mojom::AppInfo> {
  static arc::mojom::AppInfoPtr Convert(const arc::mojom::AppInfo& app_info) {
    return app_info.Clone();
  }
};

template <>
struct TypeConverter<arc::mojom::ArcPackageInfoPtr,
                     arc::mojom::ArcPackageInfo> {
  static arc::mojom::ArcPackageInfoPtr Convert(
      const arc::mojom::ArcPackageInfo& package_info) {
    return package_info.Clone();
  }
};

template <>
struct TypeConverter<arc::mojom::ShortcutInfoPtr, arc::mojom::ShortcutInfo> {
  static arc::mojom::ShortcutInfoPtr Convert(
      const arc::mojom::ShortcutInfo& shortcut_info) {
    return shortcut_info.Clone();
  }
};

}  // namespace mojo

namespace {

constexpr char kTestAppName[] = "Test ARC App";
constexpr char kTestAppName2[] = "Test ARC App 2";
constexpr char kTestShortcutName[] = "Test Shortcut";
constexpr char kTestShortcutName2[] = "Test Shortcut 2";
constexpr char kTestAppPackage[] = "test.arc.app.package";
constexpr char kTestAppActivity[] = "test.arc.app.package.activity";
constexpr char kTestAppActivity2[] = "test.arc.gitapp.package.activity2";
constexpr char kTestShelfGroup[] = "shelf_group";
constexpr char kTestShelfGroup2[] = "shelf_group_2";
constexpr char kTestShelfGroup3[] = "shelf_group_3";
constexpr char kTestLogicalWindow[] = "logical_window1";
constexpr char kTestLogicalWindow2[] = "logical_window2";
constexpr char kTestWindowTitle[] = "window1";
constexpr char kTestWindowTitle2[] = "window2";
constexpr char kTestWindowTitle3[] = "window3";
constexpr int kAppAnimatedThresholdMs = 100;
constexpr int kGeneratedIconSize = 32;

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

class AppAnimatedWaiter {
 public:
  explicit AppAnimatedWaiter(const std::string& app_id) : app_id_(app_id) {}

  void Wait() {
    const base::TimeDelta threshold =
        base::TimeDelta::FromMilliseconds(kAppAnimatedThresholdMs);
    ShelfSpinnerController* controller =
        ChromeLauncherController::instance()->GetShelfSpinnerController();
    while (controller->GetActiveTime(app_id_) < threshold) {
      base::RunLoop().RunUntilIdle();
    }
  }

 private:
  const std::string app_id_;
};

enum TestAction {
  TEST_ACTION_START,  // Start app on app appears.
  TEST_ACTION_EXIT,   // Exit Chrome during animation.
  TEST_ACTION_CLOSE,  // Close item during animation.
};

// Test parameters include TestAction and pin/unpin state.
typedef std::tuple<TestAction, bool> TestParameter;

TestParameter build_test_parameter[] = {
    TestParameter(TEST_ACTION_START, false),
    TestParameter(TEST_ACTION_EXIT, false),
    TestParameter(TEST_ACTION_CLOSE, false),
    TestParameter(TEST_ACTION_START, true),
};

std::string CreateIntentUriWithShelfGroup(const std::string& shelf_group_id) {
  return base::StringPrintf("#Intent;S.org.chromium.arc.shelf_group_id=%s;end",
                            shelf_group_id.c_str());
}

std::string CreateIntentUriWithShelfGroupAndLogicalWindow(
    const std::string& shelf_group_id,
    const std::string& logical_window_id) {
  return base::StringPrintf(
      "#Intent;S.org.chromium.arc.logical_window_id=%s;"
      "S.org.chromium.arc.shelf_group_id=%s;end",
      logical_window_id.c_str(), shelf_group_id.c_str());
}

ash::ShelfItemDelegate::AppMenuItems GetAppMenuItems(
    ash::ShelfItemDelegate* delegate,
    int event_flags) {
  return delegate->GetAppMenuItems(event_flags, base::NullCallback());
}

}  // namespace

class ArcAppLauncherBrowserTest : public extensions::ExtensionBrowserTest {
 public:
  ArcAppLauncherBrowserTest() {}
  ~ArcAppLauncherBrowserTest() override {}

 protected:
  // content::BrowserTestBase:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionBrowserTest::SetUpCommandLine(command_line);
    arc::SetArcAvailableCommandLineForTesting(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    extensions::ExtensionBrowserTest::SetUpInProcessBrowserTestFixture();
    arc::ArcSessionManager::SetUiEnabledForTesting(false);
  }

  void SetUpOnMainThread() override {
    arc::SetArcPlayStoreEnabledForProfile(profile(), true);

    // This ensures app_prefs()->GetApp() below never returns nullptr.
    base::RunLoop run_loop;
    app_prefs()->SetDefaultAppsReadyCallback(run_loop.QuitClosure());
    run_loop.Run();

    // Allows creation of windows.
    wm_helper_ = std::make_unique<exo::WMHelperChromeOS>();
  }

  void TearDownOnMainThread() override {
    wm_helper_.reset();
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

  std::string InstallShortcut(const std::string& name,
                              const std::string& shelf_group) {
    arc::mojom::ShortcutInfo shortcut;
    shortcut.name = name;
    shortcut.package_name = kTestAppPackage;
    shortcut.intent_uri = CreateIntentUriWithShelfGroup(shelf_group);
    const std::string shortcut_id =
        ArcAppListPrefs::GetAppId(shortcut.package_name, shortcut.intent_uri);
    app_host()->OnInstallShortcut(arc::mojom::ShortcutInfo::From(shortcut));
    base::RunLoop().RunUntilIdle();

    std::unique_ptr<ArcAppListPrefs::AppInfo> shortcut_info =
        app_prefs()->GetApp(shortcut_id);

    CHECK(shortcut_info);
    EXPECT_TRUE(shortcut_info->shortcut);
    EXPECT_EQ(kTestAppPackage, shortcut_info->package_name);
    EXPECT_EQ(shortcut.intent_uri, shortcut_info->intent_uri);
    return shortcut_id;
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

  void SendPackageUpdated(const std::string& package_name, bool multi_app) {
    app_host()->OnPackageAppListRefreshed(
        package_name, GetTestAppsList(package_name, multi_app));

    // Ensure async callbacks from the resulting observer calls are run.
    base::RunLoop().RunUntilIdle();
  }

  void SendPackageRemoved(const std::string& package_name) {
    app_host()->OnPackageRemoved(package_name);

    // Ensure async callbacks from the resulting observer calls are run.
    base::RunLoop().RunUntilIdle();
  }

  void SendInstallationStarted(const std::string& package_name) {
    app_host()->OnInstallationStarted(package_name);
    base::RunLoop().RunUntilIdle();
  }

  void SendInstallationFinished(const std::string& package_name, bool success) {
    arc::mojom::InstallationResult result;
    result.package_name = package_name;
    result.success = success;
    app_host()->OnInstallationFinished(
        arc::mojom::InstallationResultPtr(result.Clone()));
    base::RunLoop().RunUntilIdle();
  }

  void StartInstance() {
    if (!arc_session_manager()->profile()) {
      // This situation happens when StartInstance() is called after
      // StopInstance().
      // TODO(hidehiko): The emulation is not implemented correctly. Fix it.
      arc_session_manager()->SetProfile(profile());
      arc::ArcServiceLauncher::Get()->OnPrimaryUserProfilePrepared(profile());
    }
    app_instance_ = std::make_unique<arc::FakeAppInstance>(app_host());
    arc_brige_service()->app()->SetInstance(app_instance_.get());
  }

  void StopInstance() {
    if (app_instance_)
      arc_brige_service()->app()->CloseInstance(app_instance_.get());
    arc_session_manager()->Shutdown();
  }

  ash::ShelfItemDelegate* GetShelfItemDelegate(const std::string& id) {
    auto* model = ChromeLauncherController::instance()->shelf_model();
    return model->GetShelfItemDelegate(ash::ShelfID(id));
  }

  void WaitForDecompressTask() {
    base::ThreadPoolInstance::Get()->FlushForTesting();
    base::RunLoop().RunUntilIdle();
  }

  ArcAppListPrefs* app_prefs() { return ArcAppListPrefs::Get(profile()); }

  // Returns as AppHost interface in order to access to private implementation
  // of the interface.
  arc::mojom::AppHost* app_host() { return app_prefs(); }

  // Returns as AppInstance observer interface in order to access to private
  // implementation of the interface.
  arc::ConnectionObserver<arc::mojom::AppInstance>* app_connection_observer() {
    return app_prefs();
  }

  arc::ArcSessionManager* arc_session_manager() {
    return arc::ArcSessionManager::Get();
  }

  arc::ArcBridgeService* arc_brige_service() {
    return arc::ArcServiceManager::Get()->arc_bridge_service();
  }

  arc::FakeAppInstance* arc_instance() { return app_instance_.get(); }

 private:
  std::unique_ptr<arc::FakeAppInstance> app_instance_;
  std::unique_ptr<exo::WMHelper> wm_helper_;

  DISALLOW_COPY_AND_ASSIGN(ArcAppLauncherBrowserTest);
};

class ArcAppDeferredLauncherBrowserTest : public ArcAppLauncherBrowserTest {
 public:
  ArcAppDeferredLauncherBrowserTest() = default;
  ~ArcAppDeferredLauncherBrowserTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcAppDeferredLauncherBrowserTest);
};

IN_PROC_BROWSER_TEST_F(ArcAppDeferredLauncherBrowserTest,
                       StartAppDeferredFromShelfButton) {
  StartInstance();
  InstallTestApps(kTestAppPackage, false);
  SendPackageAdded(kTestAppPackage, false);

  // Restart ARC and ARC apps are in disabled state.
  StopInstance();
  StartInstance();

  ChromeLauncherController* const controller =
      ChromeLauncherController::instance();
  const std::string app_id = GetTestApp1Id(kTestAppPackage);
  controller->PinAppWithID(app_id);

  aura::Window* const root_window = ash::Shell::GetPrimaryRootWindow();
  ash::ShelfViewTestAPI test_api(
      ash::Shelf::ForWindow(root_window)->GetShelfViewForTesting());

  // In this test, we need the shelf button's bounds. The scrollable shelf
  // is notified of the added shelf button and layouts its child views
  // during the bounds animation. So wait for the bounds animation to finish
  // then get the final bounds of the shelf button.
  test_api.RunMessageLoopUntilAnimationsDone();
  ash::StatusAreaWidgetTestHelper::WaitForAnimationEnd(
      ash::Shelf::ForWindow(root_window)->GetStatusAreaWidget());

  const int item_index =
      controller->shelf_model()->ItemIndexByID(ash::ShelfID(app_id));
  ASSERT_GE(item_index, 0);

  ash::ShelfAppButton* const button = test_api.GetButton(item_index);
  ASSERT_TRUE(button);

  views::InkDrop* const ink_drop = button->GetInkDropForTesting();
  ASSERT_TRUE(ink_drop);

  EXPECT_EQ(views::InkDropState::HIDDEN, ink_drop->GetTargetInkDropState());

  ui::test::EventGenerator event_generator(root_window);
  event_generator.MoveMouseTo(button->GetBoundsInScreen().CenterPoint());
  base::RunLoop().RunUntilIdle();
  event_generator.ClickLeftButton();

  EXPECT_EQ(views::InkDropState::ACTION_TRIGGERED,
            ink_drop->GetTargetInkDropState());
}

class ArcAppDeferredLauncherWithParamsBrowserTest
    : public ArcAppDeferredLauncherBrowserTest,
      public testing::WithParamInterface<TestParameter> {
 public:
  ArcAppDeferredLauncherWithParamsBrowserTest() = default;
  ~ArcAppDeferredLauncherWithParamsBrowserTest() override = default;

 protected:
  bool is_pinned() const { return std::get<1>(GetParam()); }

  TestAction test_action() const { return std::get<0>(GetParam()); }

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcAppDeferredLauncherWithParamsBrowserTest);
};

// This tests simulates normal workflow for starting ARC app in deferred mode.
IN_PROC_BROWSER_TEST_P(ArcAppDeferredLauncherWithParamsBrowserTest,
                       StartAppDeferred) {
  // Install app to remember existing apps.
  StartInstance();
  InstallTestApps(kTestAppPackage, false);
  SendPackageAdded(kTestAppPackage, false);

  ChromeLauncherController* controller = ChromeLauncherController::instance();
  const std::string app_id = GetTestApp1Id(kTestAppPackage);
  const ash::ShelfID shelf_id(app_id);
  if (is_pinned()) {
    controller->PinAppWithID(app_id);
    const ash::ShelfItem* item = controller->GetItem(shelf_id);
    EXPECT_EQ(base::UTF8ToUTF16(kTestAppName), item->title);
  } else {
    EXPECT_FALSE(controller->GetItem(shelf_id));
  }

  StopInstance();
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      app_prefs()->GetApp(app_id);
  EXPECT_FALSE(app_info);

  // Restart instance. App should be taken from prefs but its state is non-ready
  // currently.
  StartInstance();
  app_info = app_prefs()->GetApp(app_id);
  ASSERT_TRUE(app_info);
  EXPECT_FALSE(app_info->ready);
  EXPECT_EQ(is_pinned(), controller->GetItem(shelf_id) != nullptr);

  // Launching non-ready ARC app creates item on shelf and spinning animation.
  if (is_pinned()) {
    EXPECT_EQ(
        ash::SHELF_ACTION_NEW_WINDOW_CREATED,
        SelectShelfItem(shelf_id, ui::ET_MOUSE_PRESSED,
                        display::kInvalidDisplayId, ash::LAUNCH_FROM_SHELF));
  } else {
    arc::LaunchApp(profile(), app_id, ui::EF_LEFT_MOUSE_BUTTON,
                   arc::UserInteractionType::NOT_USER_INITIATED);
  }

  const ash::ShelfItem* item = controller->GetItem(shelf_id);
  EXPECT_EQ(base::UTF8ToUTF16(kTestAppName), item->title);
  AppAnimatedWaiter(app_id).Wait();

  switch (test_action()) {
    case TEST_ACTION_START:
      // Now simulates that ARC is started and app list is refreshed. This
      // should stop animation and delete icon from the shelf.
      InstallTestApps(kTestAppPackage, false);
      SendPackageAdded(kTestAppPackage, false);
      EXPECT_TRUE(controller->GetShelfSpinnerController()
                      ->GetActiveTime(app_id)
                      .is_zero());
      EXPECT_EQ(is_pinned(), controller->GetItem(shelf_id) != nullptr);
      break;
    case TEST_ACTION_EXIT:
      // Just exit Chrome.
      break;
    case TEST_ACTION_CLOSE: {
      // Close item during animation.
      ash::ShelfItemDelegate* delegate = GetShelfItemDelegate(app_id);
      ASSERT_TRUE(delegate);
      delegate->Close();
      EXPECT_TRUE(controller->GetShelfSpinnerController()
                      ->GetActiveTime(app_id)
                      .is_zero());
      EXPECT_EQ(is_pinned(), controller->GetItem(shelf_id) != nullptr);
      break;
    }
  }
}

INSTANTIATE_TEST_SUITE_P(ArcAppDeferredLauncherWithParamsBrowserTestInstance,
                         ArcAppDeferredLauncherWithParamsBrowserTest,
                         ::testing::ValuesIn(build_test_parameter));

// This tests validates pin state on package update and remove.
IN_PROC_BROWSER_TEST_F(ArcAppLauncherBrowserTest, PinOnPackageUpdateAndRemove) {
  StartInstance();

  // Make use app list sync service is started. Normally it is started when
  // sycing is initialized.
  app_list::AppListSyncableServiceFactory::GetForProfile(profile())
      ->GetModelUpdater();

  InstallTestApps(kTestAppPackage, true);
  SendPackageAdded(kTestAppPackage, false);

  const ash::ShelfID shelf_id1(GetTestApp1Id(kTestAppPackage));
  const ash::ShelfID shelf_id2(GetTestApp2Id(kTestAppPackage));
  ChromeLauncherController* controller = ChromeLauncherController::instance();
  controller->PinAppWithID(shelf_id1.app_id);
  controller->PinAppWithID(shelf_id2.app_id);
  EXPECT_TRUE(controller->GetItem(shelf_id1));
  EXPECT_TRUE(controller->GetItem(shelf_id2));

  // Package contains only one app. App list is not shown for updated package.
  SendPackageUpdated(kTestAppPackage, false);
  // Second pin should gone.
  EXPECT_TRUE(controller->GetItem(shelf_id1));
  EXPECT_FALSE(controller->GetItem(shelf_id2));

  // Package contains two apps. App list is not shown for updated package.
  SendPackageUpdated(kTestAppPackage, true);
  // Second pin should not appear.
  EXPECT_TRUE(controller->GetItem(shelf_id1));
  EXPECT_FALSE(controller->GetItem(shelf_id2));

  // Package removed.
  SendPackageRemoved(kTestAppPackage);
  // No pin is expected.
  EXPECT_FALSE(controller->GetItem(shelf_id1));
  EXPECT_FALSE(controller->GetItem(shelf_id2));
}

// Test AppListControllerDelegate::IsAppOpen for ARC apps.
IN_PROC_BROWSER_TEST_F(ArcAppLauncherBrowserTest, IsAppOpen) {
  StartInstance();
  InstallTestApps(kTestAppPackage, false);
  SendPackageAdded(kTestAppPackage, true);
  const std::string app_id = GetTestApp1Id(kTestAppPackage);

  AppListClientImpl* client = AppListClientImpl::GetInstance();
  AppListControllerDelegate* delegate = client;
  EXPECT_FALSE(delegate->IsAppOpen(app_id));
  arc::LaunchApp(profile(), app_id, ui::EF_LEFT_MOUSE_BUTTON,
                 arc::UserInteractionType::NOT_USER_INITIATED);
  EXPECT_FALSE(delegate->IsAppOpen(app_id));
  // Simulate task creation so the app is marked as running/open.
  std::unique_ptr<ArcAppListPrefs::AppInfo> info = app_prefs()->GetApp(app_id);
  app_host()->OnTaskCreated(0, info->package_name, info->activity, info->name,
                            info->intent_uri, 0 /* session_id */);
  EXPECT_TRUE(delegate->IsAppOpen(app_id));
}

// Test Shelf Groups
IN_PROC_BROWSER_TEST_F(ArcAppLauncherBrowserTest, ShelfGroup) {
  StartInstance();
  InstallTestApps(kTestAppPackage, false);
  SendPackageAdded(kTestAppPackage, true);
  const std::string shorcut_id1 =
      InstallShortcut(kTestShortcutName, kTestShelfGroup);
  const std::string shorcut_id2 =
      InstallShortcut(kTestShortcutName2, kTestShelfGroup2);

  const std::string app_id = GetTestApp1Id(kTestAppPackage);
  std::unique_ptr<ArcAppListPrefs::AppInfo> info = app_prefs()->GetApp(app_id);
  ASSERT_TRUE(info);

  const std::string shelf_id1 =
      arc::ArcAppShelfId(kTestShelfGroup, app_id).ToString();
  const std::string shelf_id2 =
      arc::ArcAppShelfId(kTestShelfGroup2, app_id).ToString();
  const std::string shelf_id3 =
      arc::ArcAppShelfId(kTestShelfGroup3, app_id).ToString();

  // 1 task for group 1
  app_host()->OnTaskCreated(1, info->package_name, info->activity, info->name,
                            CreateIntentUriWithShelfGroup(kTestShelfGroup),
                            0 /* session_id */);

  ash::ShelfItemDelegate* delegate1 = GetShelfItemDelegate(shelf_id1);
  ASSERT_TRUE(delegate1);

  // 2 tasks for group 2
  app_host()->OnTaskCreated(2, info->package_name, info->activity, info->name,
                            CreateIntentUriWithShelfGroup(kTestShelfGroup2),
                            0 /* session_id */);

  ash::ShelfItemDelegate* delegate2 = GetShelfItemDelegate(shelf_id2);
  ASSERT_TRUE(delegate2);
  ASSERT_NE(delegate1, delegate2);

  app_host()->OnTaskCreated(3, info->package_name, info->activity, info->name,
                            CreateIntentUriWithShelfGroup(kTestShelfGroup2),
                            0 /* session_id */);

  ASSERT_EQ(delegate2, GetShelfItemDelegate(shelf_id2));

  // 2 tasks for group 3 which does not have shortcut.
  app_host()->OnTaskCreated(4, info->package_name, info->activity, info->name,
                            CreateIntentUriWithShelfGroup(kTestShelfGroup3),
                            0 /* session_id */);

  ash::ShelfItemDelegate* delegate3 = GetShelfItemDelegate(shelf_id3);
  ASSERT_TRUE(delegate3);
  ASSERT_NE(delegate1, delegate3);
  ASSERT_NE(delegate2, delegate3);

  app_host()->OnTaskCreated(5, info->package_name, info->activity, info->name,
                            CreateIntentUriWithShelfGroup(kTestShelfGroup3),
                            0 /* session_id */);

  ASSERT_EQ(delegate3, GetShelfItemDelegate(shelf_id3));

  ChromeLauncherController* controller = ChromeLauncherController::instance();
  const ash::ShelfItem* item1 = controller->GetItem(ash::ShelfID(shelf_id1));
  ASSERT_TRUE(item1);

  // The shelf group item's title should be the title of the referenced ARC app.
  EXPECT_EQ(base::UTF8ToUTF16(kTestAppName), item1->title);

  // Destroy task #0, this kills shelf group 1
  app_host()->OnTaskDestroyed(1);
  EXPECT_FALSE(GetShelfItemDelegate(shelf_id1));

  // Destroy task #1, shelf group 2 is still alive
  app_host()->OnTaskDestroyed(2);
  EXPECT_EQ(delegate2, GetShelfItemDelegate(shelf_id2));
  // Destroy task #2, this kills shelf group 2
  app_host()->OnTaskDestroyed(3);
  EXPECT_FALSE(GetShelfItemDelegate(shelf_id2));

  // Disable ARC, this removes app and as result kills shelf group 3.
  arc::SetArcPlayStoreEnabledForProfile(profile(), false);
  EXPECT_FALSE(GetShelfItemDelegate(shelf_id3));
}

// Test Logical Windows: Among a group of windows that have the same group ID
// and logical window ID, only one should be represented in the shelf at any
// time. If that window is closed, a different window of the logical window
// should be shown instead.
IN_PROC_BROWSER_TEST_F(ArcAppLauncherBrowserTest, LogicalWindow) {
  StartInstance();
  InstallTestApps(kTestAppPackage, false);
  SendPackageAdded(kTestAppPackage, true);

  const std::string app_id = GetTestApp1Id(kTestAppPackage);
  std::unique_ptr<ArcAppListPrefs::AppInfo> info = app_prefs()->GetApp(app_id);
  ASSERT_TRUE(info);

  const std::string shelf_id1 =
      arc::ArcAppShelfId(kTestShelfGroup, app_id).ToString();
  const std::string shelf_id2 =
      arc::ArcAppShelfId(kTestShelfGroup2, app_id).ToString();

  // We will use the following 7 windows. Index 0 is skipped because task_ids
  // start at 1.
  const char* kTestWindowTitles[8] = {"",
                                      kTestWindowTitle,
                                      kTestWindowTitle2,
                                      kTestWindowTitle,
                                      kTestWindowTitle2,
                                      kTestWindowTitle3,
                                      kTestWindowTitle,
                                      kTestWindowTitle2};
  const char* kTestShelfGroups[8] = {"",
                                     kTestShelfGroup,
                                     kTestShelfGroup,
                                     kTestShelfGroup,
                                     kTestShelfGroup,
                                     kTestShelfGroup,
                                     kTestShelfGroup2,
                                     kTestShelfGroup2};
  const char* kTestLogicalWindows[8] = {"",
                                        kTestLogicalWindow,
                                        kTestLogicalWindow,
                                        kTestLogicalWindow2,
                                        kTestLogicalWindow2,
                                        kTestLogicalWindow2,
                                        kTestLogicalWindow,
                                        kTestLogicalWindow};
  const std::u16string kTestWindowUTF16Title =
      base::ASCIIToUTF16(kTestWindowTitle);
  const std::u16string kTestWindowUTF16Title2 =
      base::ASCIIToUTF16(kTestWindowTitle2);
  const std::u16string kTestWindowUTF16Title3 =
      base::ASCIIToUTF16(kTestWindowTitle3);

  // Create windows that will be associated with the tasks. Without this,
  // GetAppMenuItems() will only return an empty list.
  std::vector<std::unique_ptr<exo::ShellSurface>> test_windows;

  for (int task_id = 1; task_id <= 7; task_id++) {
    test_windows.push_back(exo::test::ShellSurfaceBuilder({640, 480})
                               .SetCentered()
                               .BuildShellSurface());

    aura::Window* aura_window = test_windows[task_id - 1]
                                    ->GetWidget()
                                    ->GetNativeWindow();
    ASSERT_TRUE(aura_window);
    exo::SetShellApplicationId(
        aura_window, base::StringPrintf("org.chromium.arc.%d", task_id));
  }

  // Group 1 with two logical windows: one with 2, and one with 3 tasks.
  // First logical window
  app_host()->OnTaskCreated(1, info->package_name, info->activity, info->name,
                            CreateIntentUriWithShelfGroupAndLogicalWindow(
                                kTestShelfGroups[1], kTestLogicalWindows[1]),
                            0 /* session_id */);
  arc_instance()->set_icon_response_type(
      arc::FakeAppInstance::IconResponseType::ICON_RESPONSE_SEND_EMPTY);
  app_host()->OnTaskDescriptionChanged(
      1, kTestWindowTitles[1],
      arc_instance()->GenerateIconResponse(kGeneratedIconSize,
                                           false /* app_icon */));
  WaitForDecompressTask();
  ash::ShelfItemDelegate* delegate1 = GetShelfItemDelegate(shelf_id1);

  ASSERT_TRUE(delegate1);
  ASSERT_EQ(1u, GetAppMenuItems(delegate1, 0).size());
  ASSERT_EQ(kTestWindowUTF16Title, GetAppMenuItems(delegate1, 0)[0].title);

  app_host()->OnTaskCreated(2, info->package_name, info->activity, info->name,
                            CreateIntentUriWithShelfGroupAndLogicalWindow(
                                kTestShelfGroups[2], kTestLogicalWindows[2]),
                            0 /* session_id */);
  app_host()->OnTaskDescriptionChanged(
      2, kTestWindowTitles[2],
      arc_instance()->GenerateIconResponse(kGeneratedIconSize,
                                           false /* app_icon */));

  WaitForDecompressTask();
  ASSERT_EQ(delegate1, GetShelfItemDelegate(shelf_id1));
  ASSERT_EQ(1u, GetAppMenuItems(delegate1, 0).size());
  ASSERT_EQ(kTestWindowUTF16Title, GetAppMenuItems(delegate1, 0)[0].title);

  // Second logical window
  for (int task_id = 3; task_id <= 5; task_id++) {
    app_host()->OnTaskCreated(
        task_id, info->package_name, info->activity, info->name,
        CreateIntentUriWithShelfGroupAndLogicalWindow(
            kTestShelfGroups[task_id], kTestLogicalWindows[task_id]),
        0 /* session_id */);
    app_host()->OnTaskDescriptionChanged(
        task_id, kTestWindowTitles[task_id],
        arc_instance()->GenerateIconResponse(kGeneratedIconSize,
                                             false /* app_icon */));
  }

  WaitForDecompressTask();
  ASSERT_EQ(delegate1, GetShelfItemDelegate(shelf_id1));
  ASSERT_EQ(2u, GetAppMenuItems(delegate1, 0).size());
  ASSERT_EQ(kTestWindowUTF16Title, GetAppMenuItems(delegate1, 0)[1].title);

  // Group 2 with one logical window out of 2 tasks. Same logical window id as
  // tasks 1 and 2, but different group.
  app_host()->OnTaskCreated(6, info->package_name, info->activity, info->name,
                            CreateIntentUriWithShelfGroupAndLogicalWindow(
                                kTestShelfGroups[6], kTestLogicalWindows[6]),
                            0 /* session_id */);
  app_host()->OnTaskDescriptionChanged(
      6, kTestWindowTitles[6],
      arc_instance()->GenerateIconResponse(kGeneratedIconSize,
                                           false /* app_icon */));
  ash::ShelfItemDelegate* delegate2 = GetShelfItemDelegate(shelf_id2);

  WaitForDecompressTask();
  ASSERT_TRUE(delegate2);
  ASSERT_NE(delegate1, delegate2);
  ASSERT_EQ(1u, GetAppMenuItems(delegate2, 0).size());
  ASSERT_EQ(kTestWindowUTF16Title, GetAppMenuItems(delegate2, 0)[0].title);

  app_host()->OnTaskCreated(7, info->package_name, info->activity, info->name,
                            CreateIntentUriWithShelfGroupAndLogicalWindow(
                                kTestShelfGroups[7], kTestLogicalWindows[7]),
                            0 /* session_id */);
  app_host()->OnTaskDescriptionChanged(
      7, kTestWindowTitles[7],
      arc_instance()->GenerateIconResponse(kGeneratedIconSize,
                                           false /* app_icon */));

  WaitForDecompressTask();
  ASSERT_EQ(delegate2, GetShelfItemDelegate(shelf_id2));
  ASSERT_EQ(1u, GetAppMenuItems(delegate2, 0).size());
  ASSERT_EQ(kTestWindowUTF16Title, GetAppMenuItems(delegate2, 0)[0].title);

  // Group 1 should be unchanged.
  ASSERT_EQ(2u, GetAppMenuItems(delegate1, 0).size());
  ASSERT_EQ(kTestWindowUTF16Title, GetAppMenuItems(delegate1, 0)[0].title);
  ASSERT_EQ(kTestWindowUTF16Title, GetAppMenuItems(delegate1, 0)[1].title);

  // Start closing, and see if the other parts of the logical windows show up.
  // Group 1:
  // Task 1 closes, task 2 should become visible:
  app_host()->OnTaskDestroyed(1);
  ASSERT_EQ(2u, GetAppMenuItems(delegate1, 0).size());
  ASSERT_EQ(kTestWindowUTF16Title2, GetAppMenuItems(delegate1, 0)[0].title);
  ASSERT_EQ(kTestWindowUTF16Title, GetAppMenuItems(delegate1, 0)[1].title);
  // Task 4 is hidden, so should not change its entry's title.
  app_host()->OnTaskDestroyed(4);
  ASSERT_EQ(2u, GetAppMenuItems(delegate1, 0).size());
  ASSERT_EQ(kTestWindowUTF16Title2, GetAppMenuItems(delegate1, 0)[0].title);
  ASSERT_EQ(kTestWindowUTF16Title, GetAppMenuItems(delegate1, 0)[1].title);
  // Task 3 closes, leaving only task 5 of this entry. This swaps the two
  // entries.
  app_host()->OnTaskDestroyed(3);
  ASSERT_EQ(2u, GetAppMenuItems(delegate1, 0).size());
  ASSERT_EQ(kTestWindowUTF16Title3, GetAppMenuItems(delegate1, 0)[0].title);
  ASSERT_EQ(kTestWindowUTF16Title2, GetAppMenuItems(delegate1, 0)[1].title);
  // Task 5 closes, close this entry fully.
  app_host()->OnTaskDestroyed(5);
  ASSERT_EQ(1u, GetAppMenuItems(delegate1, 0).size());
  ASSERT_EQ(kTestWindowUTF16Title2, GetAppMenuItems(delegate1, 0)[0].title);
  // Task 2 closes, the full shelf group is closed now.
  ASSERT_EQ(delegate1, GetShelfItemDelegate(shelf_id1));
  app_host()->OnTaskDestroyed(2);
  EXPECT_FALSE(GetShelfItemDelegate(shelf_id1));

  // Group 2:
  ASSERT_EQ(delegate2, GetShelfItemDelegate(shelf_id2));
  ASSERT_EQ(1u, GetAppMenuItems(delegate2, 0).size());
  // Task 7 is hidden, so should not change the entry:
  app_host()->OnTaskDestroyed(7);
  ASSERT_EQ(1u, GetAppMenuItems(delegate2, 0).size());
  ASSERT_EQ(kTestWindowUTF16Title, GetAppMenuItems(delegate2, 0)[0].title);
  // Task 6 is the last task, close group:
  app_host()->OnTaskDestroyed(6);
  EXPECT_FALSE(GetShelfItemDelegate(shelf_id2));
}
