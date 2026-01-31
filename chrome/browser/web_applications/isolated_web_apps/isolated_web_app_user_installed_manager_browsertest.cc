// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/gmock_expected_support.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/fake_chrome_iwa_runtime_data_provider.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/fake_iwa_runtime_data_provider_mixin.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "content/public/test/browser_test.h"

namespace web_app {

class IsolatedWebAppUserInstalledManagerBrowserTest
    : public IsolatedWebAppBrowserTestHarness {
 public:
  const WebApp* GetIsolatedWebApp(const webapps::AppId& app_id) {
    return provider().registrar_unsafe().GetAppById(
        app_id, WebAppFilter::IsIsolatedApp());
  }

  FakeIwaRuntimeDataProviderMixin data_provider_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(IsolatedWebAppUserInstalledManagerBrowserTest,
                       AppClosedAndRemovedAfterBlocklisting) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(ManifestBuilder()).BuildBundle();

  WebAppTestInstallObserver install_observer(profile());
  install_observer.BeginListening();

  ASSERT_OK_AND_ASSIGN(
      IsolatedWebAppUrlInfo url_info,
      app->InstallWithSource(
          profile(), &IsolatedWebAppInstallSource::FromGraphicalInstaller));

  EXPECT_EQ(install_observer.Wait(), url_info.app_id());
  EXPECT_TRUE(GetIsolatedWebApp(url_info.app_id()));
  EXPECT_NE(OpenApp(url_info.app_id()), nullptr);

  base::test::TestFuture<void> app_closed_future;
  provider().ui_manager().NotifyOnAllAppWindowsClosed(
      url_info.app_id(), app_closed_future.GetCallback());
  WebAppTestUninstallObserver uninstall_observer(profile());
  uninstall_observer.BeginListening();

  data_provider_->Update(
      [&](auto& update) { update.AddToBlocklist(url_info.web_bundle_id()); });

  EXPECT_TRUE(app_closed_future.Wait());
  EXPECT_EQ(uninstall_observer.Wait(), url_info.app_id());
  EXPECT_FALSE(GetIsolatedWebApp(url_info.app_id()));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppUserInstalledManagerBrowserTest,
                       AppClosedAndRemovedAfterBlocklistingInDevMode) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(ManifestBuilder()).BuildBundle();

  WebAppTestInstallObserver install_observer(profile());
  install_observer.BeginListening();

  ASSERT_OK_AND_ASSIGN(IsolatedWebAppUrlInfo url_info,
                       app->InstallWithSource(
                           profile(), &IsolatedWebAppInstallSource::FromDevUi));

  EXPECT_EQ(install_observer.Wait(), url_info.app_id());
  EXPECT_TRUE(GetIsolatedWebApp(url_info.app_id()));
  EXPECT_NE(OpenApp(url_info.app_id()), nullptr);

  base::test::TestFuture<void> app_closed_future;
  provider().ui_manager().NotifyOnAllAppWindowsClosed(
      url_info.app_id(), app_closed_future.GetCallback());
  WebAppTestUninstallObserver uninstall_observer(profile());
  uninstall_observer.BeginListening();

  data_provider_->Update(
      [&](FakeIwaRuntimeDataProvider::ScopedIwaRuntimeDataUpdate& update) {
        update.AddToBlocklist(url_info.web_bundle_id());
      });

  EXPECT_TRUE(app_closed_future.Wait());
  EXPECT_EQ(uninstall_observer.Wait(), url_info.app_id());
  EXPECT_FALSE(GetIsolatedWebApp(url_info.app_id()));
}

}  // namespace web_app
