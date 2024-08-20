// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/components/arc/mojom/app.mojom.h"
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_app_instance.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_app_icon.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/apps/app_dialog/app_block_dialog_view.h"
#include "chrome/browser/ui/views/apps/app_dialog/app_local_block_dialog_view.h"
#include "chrome/browser/ui/views/apps/app_dialog/app_pause_dialog_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "content/public/test/browser_test.h"
#include "ui/base/mojom/dialog_button.mojom.h"

class AppDialogViewBrowserTest : public DialogBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    arc::SetArcAvailableCommandLineForTesting(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    arc::ArcSessionManager::SetUiEnabledForTesting(false);
  }

  void SetUpOnMainThread() override {
    arc::SetArcPlayStoreEnabledForProfile(browser()->profile(), true);

    // Validating decoded content does not fit well for unit tests.
    ArcAppIcon::DisableSafeDecodingForTesting();

    arc_app_list_pref_ = ArcAppListPrefs::Get(browser()->profile());
    DCHECK(arc_app_list_pref_);
    base::RunLoop run_loop;
    arc_app_list_pref_->SetDefaultAppsReadyCallback(run_loop.QuitClosure());
    run_loop.Run();

    app_instance_ = std::make_unique<arc::FakeAppInstance>(arc_app_list_pref_);
    arc_app_list_pref_->app_connection_holder()->SetInstance(
        app_instance_.get());
    WaitForInstanceReady(arc_app_list_pref_->app_connection_holder());
  }

  void TearDownOnMainThread() override {
    arc_app_list_pref_->app_connection_holder()->CloseInstance(
        app_instance_.get());
    app_instance_.reset();
    arc::ArcSessionManager::Get()->Shutdown();
  }

  AppDialogView* ActiveView(const std::string& name) {
    if (name == "block") {
      return AppBlockDialogView::GetActiveViewForTesting();
    }

    if (name == "localblock") {
      return AppLocalBlockDialogView::GetActiveViewForTesting();
    }

    return AppPauseDialogView::GetActiveViewForTesting();
  }

  const std::string& app_id() const { return app_id_; }

  apps::AppServiceProxy* app_service_proxy() { return app_service_proxy_; }

  bool IsAppPaused() {
    bool is_app_paused = false;
    app_service_proxy()->AppRegistryCache().ForOneApp(
        app_id(), [&is_app_paused](const apps::AppUpdate& update) {
          is_app_paused = (update.Paused().value_or(false));
        });
    return is_app_paused;
  }

  void ShowUi(const std::string& name) override {
    std::vector<arc::mojom::AppInfoPtr> apps;
    apps.emplace_back(arc::mojom::AppInfo::New("Fake App 0", "fake.package.0",
                                               "fake.app.0.activity",
                                               false /* sticky */));
    app_instance_->SendRefreshAppList(apps);
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(1u, arc_app_list_pref_->GetAppIds().size());
    EXPECT_EQ(nullptr, ActiveView(name));

    app_service_proxy_ =
        apps::AppServiceProxyFactory::GetForProfile(browser()->profile());
    ASSERT_TRUE(app_service_proxy_);

    base::RunLoop run_loop;
    app_id_ =
        arc_app_list_pref_->GetAppId(apps[0]->package_name, apps[0]->activity);
    if (name == "block") {
      apps[0]->suspended = true;
      app_service_proxy_->SetDialogCreatedCallbackForTesting(
          run_loop.QuitClosure());
      app_instance_->SendRefreshAppList(apps);
      app_service_proxy_->Launch(app_id_, ui::EF_NONE,
                                 apps::LaunchSource::kFromChromeInternal);
    } else if (name == "localblock") {
      app_service_proxy_->BlockApps({app_id_});
      app_service_proxy_->SetDialogCreatedCallbackForTesting(
          run_loop.QuitClosure());
      app_service_proxy_->Launch(app_id_, ui::EF_NONE,
                                 apps::LaunchSource::kFromChromeInternal);

    } else {
      std::map<std::string, apps::PauseData> pause_data;
      pause_data[app_id_].hours = 3;
      pause_data[app_id_].minutes = 30;
      pause_data[app_id_].should_show_pause_dialog = true;
      app_service_proxy_->SetDialogCreatedCallbackForTesting(
          run_loop.QuitClosure());
      app_service_proxy_->PauseApps(pause_data);
    }
    run_loop.Run();

    ASSERT_NE(nullptr, ActiveView(name));
    EXPECT_EQ(static_cast<int>(ui::mojom::DialogButton::kOk),
              ActiveView(name)->buttons());

    if (name == "block") {
      bool state_is_set = false;
      app_service_proxy_->AppRegistryCache().ForOneApp(
          app_id_, [&state_is_set](const apps::AppUpdate& update) {
            state_is_set =
                (update.Readiness() == apps::Readiness::kDisabledByPolicy);
          });

      EXPECT_TRUE(state_is_set);
    } else if (name == "localblock") {
      bool state_is_set = false;
      app_service_proxy_->AppRegistryCache().ForOneApp(
          app_id_, [&state_is_set](const apps::AppUpdate& update) {
            state_is_set = (update.Readiness() ==
                            apps::Readiness::kDisabledByLocalSettings);
          });

      EXPECT_TRUE(state_is_set);

    } else {
      if (name == "pause_close")
        ActiveView(name)->Close();
      else
        ActiveView(name)->AcceptDialog();
    }
  }

 private:
  std::string app_id_;
  raw_ptr<apps::AppServiceProxy, DanglingUntriaged> app_service_proxy_ =
      nullptr;
  raw_ptr<ArcAppListPrefs, DanglingUntriaged> arc_app_list_pref_ = nullptr;
  std::unique_ptr<arc::FakeAppInstance> app_instance_;
};

IN_PROC_BROWSER_TEST_F(AppDialogViewBrowserTest, InvokeUi_block) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AppDialogViewBrowserTest, InvokeUi_localblock) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AppDialogViewBrowserTest, InvokeUi_pause) {
  ShowAndVerifyUi();
  EXPECT_TRUE(IsAppPaused());
}

IN_PROC_BROWSER_TEST_F(AppDialogViewBrowserTest, InvokeUi_pause_close) {
  ShowAndVerifyUi();
  EXPECT_TRUE(IsAppPaused());
}
