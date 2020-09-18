// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_ui_manager_impl.h"

#include "base/barrier_closure.h"
#include "base/test/bind_test_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/built_in_chromeos_apps.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/browser/web_applications/test/web_app_uninstall_waiter.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ui/app_list/test/chrome_app_list_test_support.h"
#endif

namespace web_app {

namespace {

// Waits for |browser| to be removed from BrowserList and then calls |callback|.
class BrowserRemovedWaiter final : public BrowserListObserver {
 public:
  explicit BrowserRemovedWaiter(Browser* browser) : browser_(browser) {}
  ~BrowserRemovedWaiter() override = default;

  void Wait() {
    BrowserList::AddObserver(this);
    run_loop_.Run();
  }

  // BrowserListObserver
  void OnBrowserRemoved(Browser* browser) override {
    if (browser != browser_)
      return;

    BrowserList::RemoveObserver(this);
    // Post a task to ensure the Remove event has been dispatched to all
    // observers.
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  run_loop_.QuitClosure());
  }

 private:
  Browser* browser_;
  base::RunLoop run_loop_;
};

void CloseAndWait(Browser* browser) {
  BrowserRemovedWaiter waiter(browser);
  browser->window()->Close();
  waiter.Wait();
}

// TODO(https://crbug.com/1042727): Fix test GURL scoping and remove this getter
// function.
GURL FooUrl() {
  return GURL("https://foo.example");
}
GURL BarUrl() {
  return GURL("https://bar.example");
}

}  // namespace

class WebAppUiManagerImplBrowserTest : public InProcessBrowserTest {
 protected:
  Profile* profile() { return browser()->profile(); }

  const AppId InstallWebApp(const GURL& start_url) {
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->start_url = start_url;
    web_app_info->open_as_window = true;
    return web_app::InstallWebApp(profile(), std::move(web_app_info));
  }

  Browser* LaunchWebApp(const AppId& app_id) {
    return LaunchWebAppBrowser(profile(), app_id);
  }

  WebAppUiManager& ui_manager() {
    return WebAppProviderBase::GetProviderBase(profile())->ui_manager();
  }
};

IN_PROC_BROWSER_TEST_F(WebAppUiManagerImplBrowserTest,
                       GetNumWindowsForApp_AppWindowsAdded) {
  // Zero apps on start:
  EXPECT_EQ(0u, ui_manager().GetNumWindowsForApp(AppId()));

  AppId foo_app_id = InstallWebApp(FooUrl());
  LaunchWebApp(foo_app_id);
  EXPECT_EQ(1u, ui_manager().GetNumWindowsForApp(foo_app_id));

  LaunchWebApp(foo_app_id);
  EXPECT_EQ(2u, ui_manager().GetNumWindowsForApp(foo_app_id));
}

IN_PROC_BROWSER_TEST_F(WebAppUiManagerImplBrowserTest,
                       GetNumWindowsForApp_AppWindowsRemoved) {
  AppId foo_app_id = InstallWebApp(FooUrl());
  auto* foo_window1 = LaunchWebApp(foo_app_id);
  auto* foo_window2 = LaunchWebApp(foo_app_id);

  AppId bar_app_id = InstallWebApp(BarUrl());
  LaunchWebApp(bar_app_id);

  EXPECT_EQ(2u, ui_manager().GetNumWindowsForApp(foo_app_id));
  EXPECT_EQ(1u, ui_manager().GetNumWindowsForApp(bar_app_id));

  CloseAndWait(foo_window1);

  EXPECT_EQ(1u, ui_manager().GetNumWindowsForApp(foo_app_id));
  EXPECT_EQ(1u, ui_manager().GetNumWindowsForApp(bar_app_id));

  CloseAndWait(foo_window2);

  EXPECT_EQ(0u, ui_manager().GetNumWindowsForApp(foo_app_id));
  EXPECT_EQ(1u, ui_manager().GetNumWindowsForApp(bar_app_id));
}

IN_PROC_BROWSER_TEST_F(WebAppUiManagerImplBrowserTest,
                       NotifyOnAllAppWindowsClosed_NoOpenedWindows) {
  AppId foo_app_id = InstallWebApp(FooUrl());
  AppId bar_app_id = InstallWebApp(BarUrl());
  LaunchWebApp(bar_app_id);

  base::RunLoop run_loop;
  // Should return early; no windows for |foo_app|.
  ui_manager().NotifyOnAllAppWindowsClosed(foo_app_id, run_loop.QuitClosure());
  run_loop.Run();
}

// Tests that the callback is correctly called when there is more than one
// app window.
IN_PROC_BROWSER_TEST_F(WebAppUiManagerImplBrowserTest,
                       NotifyOnAllAppWindowsClosed_MultipleOpenedWindows) {
  AppId foo_app_id = InstallWebApp(FooUrl());
  AppId bar_app_id = InstallWebApp(BarUrl());

  // Test that NotifyOnAllAppWindowsClosed can be called more than once for
  // the same app.
  for (int i = 0; i < 2; i++) {
    auto* foo_window1 = LaunchWebApp(foo_app_id);
    auto* foo_window2 = LaunchWebApp(foo_app_id);
    auto* bar_window = LaunchWebApp(bar_app_id);

    bool callback_ran = false;
    base::RunLoop run_loop;
    ui_manager().NotifyOnAllAppWindowsClosed(foo_app_id,
                                             base::BindLambdaForTesting([&]() {
                                               callback_ran = true;
                                               run_loop.Quit();
                                             }));

    CloseAndWait(foo_window1);
    // The callback shouldn't have run yet because there is still one window
    // opened.
    EXPECT_FALSE(callback_ran);

    CloseAndWait(bar_window);
    EXPECT_FALSE(callback_ran);

    CloseAndWait(foo_window2);
    run_loop.Run();
    EXPECT_TRUE(callback_ran);
  }
}

#if defined(OS_CHROMEOS)
// Tests that app migrations use the UI preferences of the replaced app but only
// if it's present.
IN_PROC_BROWSER_TEST_F(WebAppUiManagerImplBrowserTest, DoubleMigration) {
  app_list::AppListSyncableService* app_list_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(
          browser()->profile());

  // Install an old app to be replaced.
  AppId old_app_id = InstallWebApp(GURL("https://old.app.com"));
  app_list_service->SetPinPosition(old_app_id,
                                   syncer::StringOrdinal("positionold"));

  // Install a new app to migrate the old one to.
  AppId new_app_id = InstallWebApp(GURL("https://new.app.com"));
  {
    WebAppUninstallWaiter waiter(browser()->profile(), old_app_id);
    ui_manager().UninstallAndReplaceIfExists({old_app_id}, new_app_id);
    waiter.Wait();
    apps::AppServiceProxyFactory::GetForProfile(browser()->profile())
        ->FlushMojoCallsForTesting();
  }

  // New app should acquire old app's pin position.
  EXPECT_EQ(app_list_service->GetSyncItem(new_app_id)
                ->item_pin_ordinal.ToDebugString(),
            "positionold");

  // Change the new app's pin position.
  app_list_service->SetPinPosition(new_app_id,
                                   syncer::StringOrdinal("positionnew"));

  // Do migration again. New app should not move.
  ui_manager().UninstallAndReplaceIfExists({old_app_id}, new_app_id);
  apps::AppServiceProxyFactory::GetForProfile(browser()->profile())
      ->FlushMojoCallsForTesting();
  EXPECT_EQ(app_list_service->GetSyncItem(new_app_id)
                ->item_pin_ordinal.ToDebugString(),
            "positionnew");
}
#endif  // defined(OS_CHROMEOS)

}  // namespace web_app
