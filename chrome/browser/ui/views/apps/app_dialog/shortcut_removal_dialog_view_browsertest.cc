// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_dialog/shortcut_removal_dialog_view.h"

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/app_constants/constants.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut_registry_cache.h"
#include "components/services/app_service/public/cpp/stub_icon_loader.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/view.h"

class ShortcutRemovalDialogViewBrowserTest
    : public InProcessBrowserTest,
      public apps::ShortcutRegistryCache::Observer {
 public:
  ShortcutRemovalDialogViewBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        chromeos::features::kCrosWebAppShortcutUiUpdate);
  }
  ~ShortcutRemovalDialogViewBrowserTest() override = default;
  AppDialogView* LastCreatedView() {
    return ShortcutRemovalDialogView::GetLastCreatedViewForTesting();
  }

  apps::ShortcutId CreateWebAppBasedShortcut(
      const GURL& shortcut_url,
      const std::u16string& shortcut_name) {
    // Create web app based shortcut.
    auto web_app_info = std::make_unique<web_app::WebAppInstallInfo>();
    web_app_info->start_url = shortcut_url;
    web_app_info->title = shortcut_name;
    auto local_shortcut_id = web_app::test::InstallWebApp(
        browser()->profile(), std::move(web_app_info));
    return apps::GenerateShortcutId(app_constants::kChromeAppId,
                                    local_shortcut_id);
  }

  void ObserveAndSetShortcutRemovedCallback(
      base::OnceCallback<void(const apps::ShortcutId&)> callback) {
    obs_.Observe(cache());
    shortcut_removed_callback_ = std::move(callback);
  }
  apps::AppServiceProxy* proxy() {
    apps::AppServiceProxy* app_service_proxy =
        apps::AppServiceProxyFactory::GetForProfile(browser()->profile());
    EXPECT_TRUE(app_service_proxy);
    return app_service_proxy;
  }

  apps::ShortcutRegistryCache* cache() {
    return proxy()->ShortcutRegistryCache();
  }

  void SetStubIconLoaders(const apps::ShortcutId& shortcut_id,
                          const std::string& host_app_id) {
    proxy()->OverrideShortcutInnerIconLoaderForTesting(
        &shortcut_stub_icon_loader_);
    shortcut_stub_icon_loader_.update_version_by_app_id_[shortcut_id.value()] =
        1;
    proxy()->OverrideInnerIconLoaderForTesting(&app_stub_icon_loader_);
    app_stub_icon_loader_.update_version_by_app_id_[host_app_id] = 1;
  }

  int NumLoadShortcutIcon() {
    return shortcut_stub_icon_loader_.NumLoadIconFromIconKeyCalls();
  }

  int NumLoadBadgeIcon() {
    return app_stub_icon_loader_.NumLoadIconFromIconKeyCalls();
  }

 private:
  void OnShortcutRemoved(const apps::ShortcutId& shortcut_id) override {
    std::move(shortcut_removed_callback_).Run(shortcut_id);
  }
  void OnShortcutRegistryCacheWillBeDestroyed(
      apps::ShortcutRegistryCache* cache) override {
    obs_.Reset();
  }

  apps::StubIconLoader shortcut_stub_icon_loader_;
  apps::StubIconLoader app_stub_icon_loader_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::OnceCallback<void(const apps::ShortcutId&)> shortcut_removed_callback_;
  base::ScopedObservation<apps::ShortcutRegistryCache,
                          apps::ShortcutRegistryCache::Observer>
      obs_{this};
};

IN_PROC_BROWSER_TEST_F(ShortcutRemovalDialogViewBrowserTest, InvokeUi) {
  GURL app_url = GURL("https://example.org/");
  std::u16string shortcut_name = u"Example";
  apps::ShortcutId shortcut_id =
      CreateWebAppBasedShortcut(app_url, shortcut_name);
  SetStubIconLoaders(shortcut_id, app_constants::kChromeAppId);
  EXPECT_EQ(0, NumLoadShortcutIcon());
  EXPECT_EQ(0, NumLoadBadgeIcon());

  EXPECT_FALSE(LastCreatedView());
  proxy()->RemoveShortcut(shortcut_id, apps::UninstallSource::kUnknown,
                          nullptr);

  ASSERT_TRUE(LastCreatedView());
  EXPECT_TRUE(LastCreatedView()->GetVisible());

  EXPECT_EQ(ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL,
            LastCreatedView()->GetDialogButtons());

  std::u16string host_app_name;
  proxy()->AppRegistryCache().ForOneApp(
      app_constants::kChromeAppId,
      [&host_app_name](const apps::AppUpdate& update) {
        host_app_name = base::UTF8ToUTF16(update.ShortName());
      });

  std::u16string expected_title =
      u"Remove \"" + shortcut_name + u" - " + host_app_name + u"\" shortcut?";
  EXPECT_EQ(expected_title, LastCreatedView()->GetWindowTitle());

  EXPECT_EQ(1, NumLoadShortcutIcon());
  EXPECT_EQ(1, NumLoadBadgeIcon());
}

IN_PROC_BROWSER_TEST_F(ShortcutRemovalDialogViewBrowserTest, Accept) {
  apps::ShortcutId shortcut_id =
      CreateWebAppBasedShortcut(GURL("https://example.org/"), u"Example");
  SetStubIconLoaders(shortcut_id, app_constants::kChromeAppId);

  proxy()->RemoveShortcut(shortcut_id, apps::UninstallSource::kUnknown,
                          nullptr);

  ASSERT_TRUE(LastCreatedView()->GetVisible());

  base::test::TestFuture<const apps::ShortcutId&> future;
  ObserveAndSetShortcutRemovedCallback(future.GetCallback());
  LastCreatedView()->AcceptDialog();

  EXPECT_EQ(shortcut_id, future.Get());
}

IN_PROC_BROWSER_TEST_F(ShortcutRemovalDialogViewBrowserTest, Cancel) {
  apps::ShortcutId shortcut_id =
      CreateWebAppBasedShortcut(GURL("https://example.org/"), u"Example");
  SetStubIconLoaders(shortcut_id, app_constants::kChromeAppId);

  proxy()->RemoveShortcut(shortcut_id, apps::UninstallSource::kUnknown,
                          nullptr);

  ASSERT_TRUE(LastCreatedView()->GetVisible());

  LastCreatedView()->CancelDialog();
  EXPECT_TRUE(cache()->HasShortcut(shortcut_id));
}

IN_PROC_BROWSER_TEST_F(ShortcutRemovalDialogViewBrowserTest, InvokeUiTwice) {
  apps::ShortcutId shortcut_id_1 =
      CreateWebAppBasedShortcut(GURL("https://example.org/"), u"Example");
  SetStubIconLoaders(shortcut_id_1, app_constants::kChromeAppId);

  proxy()->RemoveShortcut(shortcut_id_1, apps::UninstallSource::kUnknown,
                          nullptr);

  ASSERT_TRUE(LastCreatedView());
  auto* first_dialog = LastCreatedView();
  EXPECT_TRUE(first_dialog->GetOkButton()->HasFocus());

  apps::ShortcutId shortcut_id_2 = CreateWebAppBasedShortcut(
      GURL("https://more-example.org/"), u"MoreExample");
  SetStubIconLoaders(shortcut_id_2, app_constants::kChromeAppId);
  proxy()->RemoveShortcut(shortcut_id_2, apps::UninstallSource::kUnknown,
                          nullptr);
  ASSERT_TRUE(LastCreatedView());
  auto* second_dialog = LastCreatedView();

  // Trigger removal dialog for a second shortcut without close the first one.
  // The removal dialog for second shortcut is different from the first one.
  EXPECT_NE(second_dialog, first_dialog);
  EXPECT_TRUE(second_dialog->GetOkButton()->HasFocus());
  EXPECT_FALSE(first_dialog->GetOkButton()->HasFocus());

  // Trigger remove for first shortcut again, the first shortcut dialog should
  // come up again. The last created view should still be the second dialog as
  // we didn't create a new one for first shortcut.
  proxy()->RemoveShortcut(shortcut_id_1, apps::UninstallSource::kUnknown,
                          nullptr);
  EXPECT_EQ(second_dialog, LastCreatedView());
  EXPECT_TRUE(first_dialog->GetOkButton()->HasFocus());
  EXPECT_FALSE(second_dialog->GetOkButton()->HasFocus());

  // Close the first dialog, then invoke a removal dialog for first shortcut
  // again should create another dialog.
  first_dialog->CancelDialog();

  proxy()->RemoveShortcut(shortcut_id_1, apps::UninstallSource::kUnknown,
                          nullptr);
  ASSERT_TRUE(LastCreatedView());
  EXPECT_NE(first_dialog, LastCreatedView());
  EXPECT_NE(second_dialog, LastCreatedView());
  EXPECT_TRUE(LastCreatedView()->GetOkButton()->HasFocus());
}

IN_PROC_BROWSER_TEST_F(ShortcutRemovalDialogViewBrowserTest,
                       ShortcutRemovedClosesDialog) {
  apps::ShortcutId shortcut_id =
      CreateWebAppBasedShortcut(GURL("https://example.org/"), u"Example");
  SetStubIconLoaders(shortcut_id, app_constants::kChromeAppId);

  proxy()->RemoveShortcut(shortcut_id, apps::UninstallSource::kUnknown,
                          nullptr);

  ASSERT_TRUE(LastCreatedView()->GetVisible());

  proxy()->ShortcutRemoved(shortcut_id);

  EXPECT_TRUE(LastCreatedView()->GetWidget()->IsClosed());
}
