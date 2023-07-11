// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <tuple>
#include <vector>

#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/window_properties.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_instance.h"
#include "chrome/browser/apps/app_service/browser_app_instance_observer.h"
#include "chrome/browser/apps/app_service/browser_app_instance_registry.h"
#include "chrome/browser/ash/crosapi/ash_requires_lacros_browsertestbase.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/test/app_registry_cache_waiter.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chromeos/crosapi/mojom/test_controller.mojom-test-utils.h"
#include "components/app_constants/constants.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/base_event_utils.h"

using ::app_constants::kChromeAppId;
using ::app_constants::kLacrosAppId;

void PinApp(const std::string& app_id) {
  auto* shelf_model = ash::ShelfModel::Get();
  if (shelf_model->ItemIndexByAppID(app_id) >= 0) {
    shelf_model->PinExistingItemWithID(app_id);
  } else {
    shelf_model->AddAndPinAppWithFactoryConstructedDelegate(app_id);
  }
}

void UnpinApp(const std::string& app_id) {
  ash::ShelfModel::Get()->UnpinAppWithID(app_id);
}

absl::optional<ash::ShelfItemStatus> ShelfStatus(const std::string& app_id) {
  ash::ShelfModel* model = ChromeShelfController::instance()->shelf_model();
  for (const ash::ShelfItem& item : model->items()) {
    if (item.id.app_id == app_id) {
      return item.status;
    }
  }
  return absl::nullopt;
}

std::string WindowAppId(aura::Window* window) {
  std::string* id = window->GetProperty(ash::kAppIDKey);
  return id ? *id : "";
}

ash::ShelfID WindowShelfId(aura::Window* window) {
  return ash::ShelfID::Deserialize(window->GetProperty(ash::kShelfIDKey));
}

class TestConditionWaiter : public apps::BrowserAppInstanceObserver,
                            public apps::AppRegistryCache::Observer {
 public:
  using Condition = base::RepeatingCallback<bool()>;

  TestConditionWaiter(
      apps::BrowserAppInstanceRegistry& browser_app_instance_registry,
      apps::AppRegistryCache& app_registry_cache,
      Condition condition)
      : condition_(condition) {
    browser_app_instance_registry_observation_.Observe(
        &browser_app_instance_registry);
    app_registry_cache_observation_.Observe(&app_registry_cache);
  }

  // apps::BrowserAppInstanceObserver overrides:
  void OnBrowserWindowAdded(
      const apps::BrowserWindowInstance& instance) override {
    OnAnyEvent();
  }
  void OnBrowserWindowUpdated(
      const apps::BrowserWindowInstance& instance) override {
    OnAnyEvent();
  }
  void OnBrowserWindowRemoved(
      const apps::BrowserWindowInstance& instance) override {
    OnAnyEvent();
  }
  void OnBrowserAppAdded(const apps::BrowserAppInstance& instance) override {
    OnAnyEvent();
  }
  void OnBrowserAppUpdated(const apps::BrowserAppInstance& instance) override {
    OnAnyEvent();
  }
  void OnBrowserAppRemoved(const apps::BrowserAppInstance& instance) override {
    OnAnyEvent();
  }

  // apps::AppRegistryCache::Observer overrides:
  void OnAppUpdate(const apps::AppUpdate& update) override { OnAnyEvent(); }

  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override {
    OnAnyEvent();
  }

  void Wait(const base::Location& from_here, const std::string& message) {
    if (!condition_.Run()) {
      base::test::ScopedRunLoopTimeout timeout(
          from_here, TestTimeouts::action_timeout(),
          base::BindLambdaForTesting(
              [&]() { return "Waiting for: " + message; }));
      run_loop_.Run();
    }
  }

 private:
  void OnAnyEvent() {
    if (condition_.Run()) {
      run_loop_.Quit();
    }
  }

  base::RunLoop run_loop_;
  base::ScopedObservation<apps::BrowserAppInstanceRegistry,
                          apps::BrowserAppInstanceObserver>
      browser_app_instance_registry_observation_{this};
  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observation_{this};
  Condition condition_;
};

#define WAIT_FOR(condition)                                                   \
  WaitForCondition(FROM_HERE,                                                 \
                   base::BindLambdaForTesting([&]() { return (condition); }), \
                   #condition)

struct ExpectedAppMenuItem {
  int command_id;
  std::string title;
  bool operator==(const ExpectedAppMenuItem& other) const {
    return std::tie(command_id, title) ==
           std::tie(other.command_id, other.title);
  }
};

std::ostream& operator<<(std::ostream& os, const ExpectedAppMenuItem& i) {
  return os << i.command_id << ", " << i.title;
}

class BrowserAppShelfControllerBrowserTest
    : public crosapi::AshRequiresLacrosBrowserTestBase {
 protected:
  static constexpr char kURL_A[] = "https://a.example.org";
  static constexpr char kURL_B[] = "https://b.example.org";
  static constexpr char kURL_C[] = "https://c.example.org";
  static constexpr char kURL_D[] = "https://d.example.org";
  // Constants are defined here rather than globally because GURL objects cannot
  // be created in a static initialiser.
  const web_app::AppId kAppId_A = UrlToAppId(kURL_A);
  const web_app::AppId kAppId_B = UrlToAppId(kURL_B);
  const web_app::AppId kAppId_C = UrlToAppId(kURL_C);
  const web_app::AppId kAppId_D = UrlToAppId(kURL_D);

  web_app::AppId UrlToAppId(const std::string& start_url) {
    return web_app::GenerateAppId(/*manifest_id=*/absl::nullopt,
                                  GURL(start_url));
  }

  void SetUpOnMainThread() override {
    crosapi::AshRequiresLacrosBrowserTestBase::SetUpOnMainThread();
    profile_ = ProfileManager::GetActiveUserProfile();
    if (!HasLacrosArgument()) {
      return;
    }

    web_app::AppTypeInitializationWaiter(profile(), apps::AppType::kWeb)
        .Await();

    auto* registry = AppServiceProxy()->BrowserAppInstanceRegistry();
    ASSERT_NE(registry, nullptr);
    registry_ = registry;
  }

  void TearDownOnMainThread() override {
    crosapi::AshRequiresLacrosBrowserTestBase::TearDownOnMainThread();
    if (!HasLacrosArgument()) {
      return;
    }

    std::vector<std::string> app_ids;
    AppServiceProxy()->AppRegistryCache().ForEachApp(
        [&app_ids](const apps::AppUpdate& update) {
          if (update.AppType() == apps::AppType::kWeb) {
            app_ids.push_back(update.AppId());
          }
        });

    for (const std::string& app_id : app_ids) {
      AppServiceProxy()->UninstallSilently(app_id,
                                           apps::UninstallSource::kShelf);
      web_app::AppReadinessWaiter(profile(), app_id,
                                  apps::Readiness::kUninstalledByUser)
          .Await();
    }
  }

  Profile* profile() { return profile_; }

  apps::AppServiceProxy* AppServiceProxy() {
    return apps::AppServiceProxyFactory::GetForProfile(profile());
  }

  void WaitForCondition(const base::Location& from_here,
                        TestConditionWaiter::Condition condition,
                        const std::string& message) {
    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile());
    TestConditionWaiter(*registry_, proxy->AppRegistryCache(),
                        std::move(condition))
        .Wait(from_here, message);
  }

  std::string InstallWebApp(const std::string& start_url,
                            apps::WindowMode mode) {
    crosapi::mojom::StandaloneBrowserTestControllerAsyncWaiter waiter(
        GetStandaloneBrowserTestController());
    std::string app_id;
    waiter.InstallWebApp(start_url, mode, &app_id);

    // Wait until the app is installed: app service publisher updates may arrive
    // out of order with the web app installation reply, so we wait until the
    // state of the app service is consistent.
    web_app::AppReadinessWaiter(profile(), app_id).Await();
    AppServiceProxy()->AppRegistryCache().ForOneApp(
        app_id, [mode](const apps::AppUpdate& update) {
          EXPECT_EQ(update.AppType(), apps::AppType::kWeb);
          EXPECT_EQ(update.WindowMode(), mode);
        });

    return app_id;
  }

  // Launch directly when there is no shelf item.
  void Launch(const std::string& app_id) {
    ChromeShelfController::instance()->LaunchApp(ash::ShelfID(app_id),
                                                 ash::LAUNCH_FROM_UNKNOWN, 0,
                                                 display::kInvalidDisplayId);
    WAIT_FOR(registry_->IsAppRunning(app_id));
  }

  void Stop(const std::string& app_id) {
    AppServiceProxy()->StopApp(app_id);
    WAIT_FOR(!registry_->IsAppRunning(app_id));
  }

  bool IsAppActive(const std::string& app_id) const {
    auto* app = registry_->FindAppInstanceIf(
        [&app_id](const apps::BrowserAppInstance& instance) {
          return instance.app_id == app_id;
        });
    return app && app->is_browser_active && app->is_web_contents_active;
  }

  // Get unique titles of all app instances.
  std::set<std::string> GetAppTitles(const std::string& app_id) const {
    std::set<std::string> result;
    for (const auto* instance : registry_->SelectAppInstances(
             [&app_id](const apps::BrowserAppInstance& instance) {
               return instance.app_id == app_id;
             })) {
      result.insert(instance->title);
    }
    return result;
  }

  int AppInstanceCount(const std::string& app_id) const {
    return registry_
        ->SelectAppInstances(
            [&app_id](const apps::BrowserAppInstance& instance) {
              return instance.app_id == app_id;
            })
        .size();
  }

  using SelectResult =
      std::pair<ash::ShelfAction, std::vector<ExpectedAppMenuItem>>;

  SelectResult SelectShelfItem(
      const std::string& app_id,
      ash::ShelfLaunchSource source = ash::LAUNCH_FROM_UNKNOWN) {
    auto event = std::make_unique<ui::MouseEvent>(
        ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(), ui::EventTimeForNow(),
        ui::EF_NONE, 0);

    base::RunLoop run_loop;
    ash::ShelfModel* model = ash::ShelfModel::Get();
    ash::ShelfItemDelegate* delegate =
        model->GetShelfItemDelegate(ash::ShelfID(app_id));
    ash::ShelfAction action_taken = ash::SHELF_ACTION_NONE;
    std::vector<ExpectedAppMenuItem> app_menu_items;
    delegate->ItemSelected(
        std::move(event), display::kInvalidDisplayId, source,
        base::BindLambdaForTesting(
            [&](ash::ShelfAction action,
                ash::ShelfItemDelegate::AppMenuItems items) {
              action_taken = action;
              for (const auto& item : items) {
                app_menu_items.push_back(
                    {item.command_id, base::UTF16ToUTF8(item.title)});
              }
              run_loop.Quit();
            }),
        base::NullCallback());
    run_loop.Run();
    WAIT_FOR(registry_->IsAppRunning(app_id));
    return SelectResult{action_taken, std::move(app_menu_items)};
  }

  raw_ptr<Profile, ExperimentalAsh> profile_ = nullptr;
  raw_ptr<apps::BrowserAppInstanceRegistry, ExperimentalAsh> registry_{nullptr};
};

IN_PROC_BROWSER_TEST_F(BrowserAppShelfControllerBrowserTest, TabbedApps) {
  if (!HasLacrosArgument()) {
    return;
  }

  {
    SCOPED_TRACE("initial state");

    // StartLacros opens one Ash and one Lacros window.
    WAIT_FOR(registry_->IsAshBrowserRunning() &&
             registry_->IsLacrosBrowserRunning());
    EXPECT_EQ(ShelfStatus(kLacrosAppId), ash::STATUS_RUNNING);
    EXPECT_EQ(ShelfStatus(kChromeAppId), ash::STATUS_RUNNING);
  }

  const apps::BrowserWindowInstance* lacros =
      *std::begin(registry_->GetLacrosBrowserWindowInstances());
  ASSERT_TRUE(lacros);
  EXPECT_EQ(WindowAppId(lacros->window), kLacrosAppId);
  // Shelf ID property tells us which shelf item will be marked as active by
  // ShelfWindowWatcher, so we just watch that.
  EXPECT_EQ(WindowShelfId(lacros->window), ash::ShelfID(kLacrosAppId));

  {
    SCOPED_TRACE("launch unpinned, stop");

    ASSERT_EQ(kAppId_A, InstallWebApp(kURL_A, apps::WindowMode::kBrowser));
    Launch(kAppId_A);

    // App A is unpinned, so no new item.
    EXPECT_EQ(ShelfStatus(kAppId_A), absl::nullopt);
    // App ID of the window is now set to app A, but shelf ID maps to the
    // browser shelf item because there is no pinned item for app A.
    EXPECT_EQ(WindowAppId(lacros->window), kAppId_A);
    EXPECT_EQ(WindowShelfId(lacros->window), ash::ShelfID(kLacrosAppId));

    // Close just the app tab.
    Stop(kAppId_A);

    EXPECT_EQ(WindowAppId(lacros->window), kLacrosAppId);
    EXPECT_EQ(WindowShelfId(lacros->window), ash::ShelfID(kLacrosAppId));
  }

  {
    SCOPED_TRACE("launch, pin, stop");

    ASSERT_EQ(kAppId_B, InstallWebApp(kURL_B, apps::WindowMode::kBrowser));
    Launch(kAppId_B);
    PinApp(kAppId_B);

    // App B has a pinned item, so it's marked running.
    EXPECT_EQ(ShelfStatus(kAppId_B), ash::STATUS_RUNNING);
    // Both app/shelf ID of the browser window now point to the shelf item for
    // app B.
    EXPECT_EQ(WindowAppId(lacros->window), kAppId_B);
    EXPECT_EQ(WindowShelfId(lacros->window), ash::ShelfID(kAppId_B));

    // Close just the app tab.
    Stop(kAppId_B);

    // Pinned item remains.
    EXPECT_EQ(ShelfStatus(kAppId_B), ash::STATUS_CLOSED);
    EXPECT_EQ(WindowAppId(lacros->window), kLacrosAppId);
    EXPECT_EQ(WindowShelfId(lacros->window), ash::ShelfID(kLacrosAppId));
  }

  {
    SCOPED_TRACE("pin, launch, stop");

    ASSERT_EQ(kAppId_C, InstallWebApp(kURL_C, apps::WindowMode::kBrowser));
    PinApp(kAppId_C);
    EXPECT_EQ(SelectShelfItem(kAppId_C),
              (SelectResult{ash::SHELF_ACTION_NEW_WINDOW_CREATED, {}}));

    // App C has a pinned item, so it's marked running and active.
    EXPECT_EQ(ShelfStatus(kAppId_C), ash::STATUS_RUNNING);
    // Both app/shelf ID of the browser window now point to the shelf item for
    // app C.
    EXPECT_EQ(WindowAppId(lacros->window), kAppId_C);
    EXPECT_EQ(WindowShelfId(lacros->window), ash::ShelfID(kAppId_C));

    // Close just the app tab.
    Stop(kAppId_C);

    // Pinned item remains.
    EXPECT_EQ(ShelfStatus(kAppId_C), ash::STATUS_CLOSED);
    EXPECT_EQ(WindowAppId(lacros->window), kLacrosAppId);
    EXPECT_EQ(WindowShelfId(lacros->window), ash::ShelfID(kLacrosAppId));
  }

  {
    SCOPED_TRACE("pin, launch, unpin, stop");

    ASSERT_EQ(kAppId_D, InstallWebApp(kURL_D, apps::WindowMode::kBrowser));
    PinApp(kAppId_D);
    EXPECT_EQ(SelectShelfItem(kAppId_D),
              (SelectResult{ash::SHELF_ACTION_NEW_WINDOW_CREATED, {}}));
    UnpinApp(kAppId_D);

    // Unpinned app tabs don't get a shelf item, but if the app is still
    // running, it stays.
    EXPECT_EQ(ShelfStatus(kAppId_D), ash::STATUS_RUNNING);
    EXPECT_EQ(WindowAppId(lacros->window), kAppId_D);
    EXPECT_EQ(WindowShelfId(lacros->window), ash::ShelfID(kAppId_D));

    // Close just the app tab.
    Stop(kAppId_D);

    EXPECT_EQ(ShelfStatus(kAppId_D), absl::nullopt);
    EXPECT_EQ(WindowAppId(lacros->window), kLacrosAppId);
    EXPECT_EQ(WindowShelfId(lacros->window), ash::ShelfID(kLacrosAppId));
  }
}

IN_PROC_BROWSER_TEST_F(BrowserAppShelfControllerBrowserTest, WindowedApps) {
  if (!HasLacrosArgument()) {
    return;
  }

  {
    SCOPED_TRACE("initial state");

    // StartLacros opens one Ash and one Lacros window.
    WAIT_FOR(registry_->IsAshBrowserRunning() &&
             registry_->IsLacrosBrowserRunning());
    EXPECT_EQ(ShelfStatus(kLacrosAppId), ash::STATUS_RUNNING);
    EXPECT_EQ(ShelfStatus(kChromeAppId), ash::STATUS_RUNNING);
  }

  {
    SCOPED_TRACE("launch unpinned, stop");

    ASSERT_EQ(kAppId_A, InstallWebApp(kURL_A, apps::WindowMode::kWindow));
    Launch(kAppId_A);
    const apps::BrowserAppInstance* appA = registry_->FindAppInstanceIf(
        [&](const auto& instance) { return instance.app_id == kAppId_A; });

    EXPECT_EQ(ShelfStatus(kAppId_A), ash::STATUS_RUNNING);
    ASSERT_TRUE(appA);
    EXPECT_EQ(WindowAppId(appA->window), kAppId_A);
    EXPECT_EQ(WindowShelfId(appA->window), ash::ShelfID(kAppId_A));

    // Close the app window.
    Stop(kAppId_A);

    EXPECT_EQ(ShelfStatus(kAppId_A), absl::nullopt);
  }

  {
    SCOPED_TRACE("launch, pin, stop");
    ASSERT_EQ(kAppId_B, InstallWebApp(kURL_B, apps::WindowMode::kWindow));
    Launch(kAppId_B);
    PinApp(kAppId_B);
    const apps::BrowserAppInstance* appB = registry_->FindAppInstanceIf(
        [&](const auto& instance) { return instance.app_id == kAppId_B; });

    EXPECT_EQ(ShelfStatus(kAppId_B), ash::STATUS_RUNNING);
    ASSERT_TRUE(appB);
    EXPECT_EQ(WindowAppId(appB->window), kAppId_B);
    EXPECT_EQ(WindowShelfId(appB->window), ash::ShelfID(kAppId_B));

    // Close the app window.
    Stop(kAppId_B);

    EXPECT_EQ(ShelfStatus(kAppId_B), ash::STATUS_CLOSED);
  }

  {
    SCOPED_TRACE("pin, launch, stop");
    ASSERT_EQ(kAppId_C, InstallWebApp(kURL_C, apps::WindowMode::kWindow));
    PinApp(kAppId_C);
    EXPECT_EQ(SelectShelfItem(kAppId_C),
              (SelectResult{ash::SHELF_ACTION_NEW_WINDOW_CREATED, {}}));
    const apps::BrowserAppInstance* appC = registry_->FindAppInstanceIf(
        [&](const auto& instance) { return instance.app_id == kAppId_C; });

    EXPECT_EQ(ShelfStatus(kAppId_C), ash::STATUS_RUNNING);
    ASSERT_TRUE(appC);
    EXPECT_EQ(WindowAppId(appC->window), kAppId_C);
    EXPECT_EQ(WindowShelfId(appC->window), ash::ShelfID(kAppId_C));

    // Close the app window.
    Stop(kAppId_C);

    EXPECT_EQ(ShelfStatus(kAppId_C), ash::STATUS_CLOSED);
  }

  {
    SCOPED_TRACE("pin, launch, unpin, stop");
    ASSERT_EQ(kAppId_D, InstallWebApp(kURL_D, apps::WindowMode::kWindow));
    PinApp(kAppId_D);
    EXPECT_EQ(SelectShelfItem(kAppId_D),
              (SelectResult{ash::SHELF_ACTION_NEW_WINDOW_CREATED, {}}));
    UnpinApp(kAppId_D);
    const apps::BrowserAppInstance* appD = registry_->FindAppInstanceIf(
        [&](const auto& instance) { return instance.app_id == kAppId_D; });

    EXPECT_EQ(ShelfStatus(kAppId_D), ash::STATUS_RUNNING);
    ASSERT_TRUE(appD);
    EXPECT_EQ(WindowAppId(appD->window), kAppId_D);
    EXPECT_EQ(WindowShelfId(appD->window), ash::ShelfID(kAppId_D));

    // Close the app window.
    Stop(kAppId_D);

    EXPECT_EQ(ShelfStatus(kAppId_D), absl::nullopt);
  }
}

// Flakily fails: https://crbug.com/1373054
IN_PROC_BROWSER_TEST_F(BrowserAppShelfControllerBrowserTest,
                       DISABLED_ActivateAndMinimizeTabs) {
  if (!HasLacrosArgument()) {
    return;
  }

  {
    SCOPED_TRACE("initial state");

    // StartLacros opens one Ash and one Lacros window.
    WAIT_FOR(registry_->IsAshBrowserRunning() &&
             registry_->IsLacrosBrowserRunning());
    EXPECT_EQ(ShelfStatus(kLacrosAppId), ash::STATUS_RUNNING);
    EXPECT_EQ(ShelfStatus(kChromeAppId), ash::STATUS_RUNNING);
  }

  const apps::BrowserWindowInstance* lacros =
      *std::begin(registry_->GetLacrosBrowserWindowInstances());

  {
    // Install, pin, and launch two apps (A and B) in the same order. Both apps
    // will be running in two tabs in one window, app B is active.
    ASSERT_EQ(kAppId_A, InstallWebApp(kURL_A, apps::WindowMode::kBrowser));
    ASSERT_EQ(kAppId_B, InstallWebApp(kURL_B, apps::WindowMode::kBrowser));
    PinApp(kAppId_A);
    PinApp(kAppId_B);
    ASSERT_EQ(SelectShelfItem(kAppId_A),
              (SelectResult{ash::SHELF_ACTION_NEW_WINDOW_CREATED, {}}));
    ASSERT_EQ(SelectShelfItem(kAppId_B),
              (SelectResult{ash::SHELF_ACTION_NEW_WINDOW_CREATED, {}}));
    ASSERT_EQ(registry_->GetLacrosBrowserWindowInstances().size(), 1u);
    ASSERT_EQ(ShelfStatus(kAppId_A), ash::STATUS_RUNNING);
    ASSERT_EQ(ShelfStatus(kAppId_B), ash::STATUS_RUNNING);
    ASSERT_EQ(WindowAppId(lacros->window), kAppId_B);
    ASSERT_EQ(WindowShelfId(lacros->window), ash::ShelfID(kAppId_B));

    // Activate the inactive app A tab.
    ASSERT_EQ(SelectShelfItem(kAppId_A),
              (SelectResult{ash::SHELF_ACTION_WINDOW_ACTIVATED, {}}));
    WAIT_FOR(IsAppActive(kAppId_A) && !IsAppActive(kAppId_B));
    EXPECT_EQ(WindowAppId(lacros->window), kAppId_A);
    EXPECT_EQ(WindowShelfId(lacros->window), ash::ShelfID(kAppId_A));

    // Re-activate app B tab again.
    ASSERT_EQ(SelectShelfItem(kAppId_B),
              (SelectResult{ash::SHELF_ACTION_WINDOW_ACTIVATED, {}}));
    WAIT_FOR(!IsAppActive(kAppId_A) && IsAppActive(kAppId_B));
    EXPECT_EQ(WindowAppId(lacros->window), kAppId_B);
    EXPECT_EQ(WindowShelfId(lacros->window), ash::ShelfID(kAppId_B));

    // Selecting app B again minimises it.
    ASSERT_EQ(SelectShelfItem(kAppId_B),
              (SelectResult{ash::SHELF_ACTION_WINDOW_MINIMIZED, {}}));
    WAIT_FOR(!IsAppActive(kAppId_A) && !IsAppActive(kAppId_B));
    // Window properties should not change for the minimised window.
    EXPECT_FALSE(lacros->window->IsVisible());
    EXPECT_EQ(WindowAppId(lacros->window), kAppId_B);
    EXPECT_EQ(WindowShelfId(lacros->window), ash::ShelfID(kAppId_B));

    // Selecting app B again restores and activates it.
    ASSERT_EQ(SelectShelfItem(kAppId_B),
              (SelectResult{ash::SHELF_ACTION_WINDOW_ACTIVATED, {}}));
    WAIT_FOR(!IsAppActive(kAppId_A) && IsAppActive(kAppId_B));
    // Window properties should not change for the window.
    EXPECT_TRUE(lacros->window->IsVisible());
    EXPECT_EQ(WindowAppId(lacros->window), kAppId_B);
    EXPECT_EQ(WindowShelfId(lacros->window), ash::ShelfID(kAppId_B));

    // Selecting app B again from the app list should do nothing.
    ASSERT_EQ(SelectShelfItem(kAppId_B, ash::LAUNCH_FROM_APP_LIST),
              (SelectResult{ash::SHELF_ACTION_WINDOW_ACTIVATED, {}}));
    WAIT_FOR(!IsAppActive(kAppId_A) && IsAppActive(kAppId_B));
    // Window properties should not change for the window.
    EXPECT_TRUE(lacros->window->IsVisible());
    EXPECT_EQ(WindowAppId(lacros->window), kAppId_B);
    EXPECT_EQ(WindowShelfId(lacros->window), ash::ShelfID(kAppId_B));
  }
}

// Flakily fails: https://crbug.com/1373054
IN_PROC_BROWSER_TEST_F(BrowserAppShelfControllerBrowserTest,
                       DISABLED_ActivateAndMinimizeWindows) {
  if (!HasLacrosArgument()) {
    return;
  }

  {
    SCOPED_TRACE("initial state");

    // StartLacros opens one Ash and one Lacros window.
    WAIT_FOR(registry_->IsAshBrowserRunning() &&
             registry_->IsLacrosBrowserRunning());
    EXPECT_EQ(ShelfStatus(kLacrosAppId), ash::STATUS_RUNNING);
    EXPECT_EQ(ShelfStatus(kChromeAppId), ash::STATUS_RUNNING);
  }

  ASSERT_EQ(kAppId_A, InstallWebApp(kURL_A, apps::WindowMode::kWindow));
  ASSERT_EQ(kAppId_B, InstallWebApp(kURL_B, apps::WindowMode::kWindow));
  Launch(kAppId_A);
  Launch(kAppId_B);
  const apps::BrowserAppInstance* appA = registry_->FindAppInstanceIf(
      [&](const auto& instance) { return instance.app_id == kAppId_A; });
  const apps::BrowserAppInstance* appB = registry_->FindAppInstanceIf(
      [&](const auto& instance) { return instance.app_id == kAppId_B; });

  // Both are pinned.
  EXPECT_EQ(ShelfStatus(kAppId_A), ash::STATUS_RUNNING);
  EXPECT_EQ(ShelfStatus(kAppId_B), ash::STATUS_RUNNING);

  // App B window is activated.
  ASSERT_EQ(SelectShelfItem(kAppId_B),
            (SelectResult{ash::SHELF_ACTION_WINDOW_ACTIVATED, {}}));
  WAIT_FOR(!IsAppActive(kAppId_A) && IsAppActive(kAppId_B));

  // App A window is activated.
  ASSERT_EQ(SelectShelfItem(kAppId_A),
            (SelectResult{ash::SHELF_ACTION_WINDOW_ACTIVATED, {}}));
  WAIT_FOR(IsAppActive(kAppId_A) && !IsAppActive(kAppId_B));
  EXPECT_TRUE(appA->window->IsVisible());
  EXPECT_TRUE(appB->window->IsVisible());

  // App A window is minimised.
  ASSERT_EQ(SelectShelfItem(kAppId_A),
            (SelectResult{ash::SHELF_ACTION_WINDOW_MINIMIZED, {}}));
  WAIT_FOR(!IsAppActive(kAppId_A) && IsAppActive(kAppId_B));
  EXPECT_FALSE(appA->window->IsVisible());
  EXPECT_TRUE(appB->window->IsVisible());

  // App A window is restored.
  ASSERT_EQ(SelectShelfItem(kAppId_A),
            (SelectResult{ash::SHELF_ACTION_WINDOW_ACTIVATED, {}}));
  WAIT_FOR(IsAppActive(kAppId_A) && !IsAppActive(kAppId_B));
  EXPECT_TRUE(appA->window->IsVisible());
  EXPECT_TRUE(appB->window->IsVisible());
}

// Flakily fails: https://crbug.com/1373054
IN_PROC_BROWSER_TEST_F(BrowserAppShelfControllerBrowserTest,
                       DISABLED_MultipleInstancesShowMenu) {
  if (!HasLacrosArgument()) {
    return;
  }

  {
    SCOPED_TRACE("initial state");

    // StartLacros opens one Ash and one Lacros window.
    WAIT_FOR(registry_->IsAshBrowserRunning() &&
             registry_->IsLacrosBrowserRunning());
    EXPECT_EQ(ShelfStatus(kLacrosAppId), ash::STATUS_RUNNING);
    EXPECT_EQ(ShelfStatus(kChromeAppId), ash::STATUS_RUNNING);
  }

  ASSERT_EQ(kAppId_A, InstallWebApp(kURL_A, apps::WindowMode::kBrowser));
  PinApp(kAppId_A);
  Launch(kAppId_A);
  Launch(kAppId_A);
  WAIT_FOR(AppInstanceCount(kAppId_A) == 2 &&
           GetAppTitles(kAppId_A) == std::set<std::string>{"a.example.org"});
  EXPECT_EQ(SelectShelfItem(kAppId_A), (SelectResult{ash::SHELF_ACTION_NONE,
                                                     {
                                                         {1, "a.example.org"},
                                                         {2, "a.example.org"},
                                                     }}));

  ASSERT_EQ(kAppId_B, InstallWebApp(kURL_B, apps::WindowMode::kWindow));
  Launch(kAppId_B);
  Launch(kAppId_B);
  WAIT_FOR(AppInstanceCount(kAppId_B) == 2 &&
           GetAppTitles(kAppId_B) == std::set<std::string>{"b.example.org"});
  EXPECT_EQ(SelectShelfItem(kAppId_B), (SelectResult{ash::SHELF_ACTION_NONE,
                                                     {
                                                         {1, "b.example.org"},
                                                         {2, "b.example.org"},
                                                     }}));
}
