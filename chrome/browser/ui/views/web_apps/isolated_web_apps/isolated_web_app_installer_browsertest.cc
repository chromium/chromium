// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string_view>
#include <variant>

#include "base/files/scoped_temp_dir.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_coordinator.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_model.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_view_controller.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_user_installability_checker.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/test_isolated_web_app_installer_model_observer.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/install_isolated_web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/fake_chrome_iwa_runtime_data_provider.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/fake_iwa_runtime_data_provider_mixin.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_pref_names.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace web_app {
namespace {

void AcceptDialogAndAwaitDestruction(views::Widget* widget) {
  views::test::AcceptDialog(widget);
}

void AcceptDialogAndContinue(views::Widget* widget) {
  auto* delegate = widget->widget_delegate()->AsDialogDelegate();
  delegate->AcceptDialog();
}

}  // namespace

class IsolatedWebAppInstallerBrowserTest : public WebAppBrowserTestBase {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kIsolatedWebApps, features::kIsolatedWebAppDevMode,
         features::kIsolatedWebAppUnmanagedInstall},
        {});
    WebAppBrowserTestBase::SetUp();
  }

  void SetUpOnMainThread() override {
    WebAppBrowserTestBase::SetUpOnMainThread();
#if BUILDFLAG(IS_CHROMEOS)
    profile()->GetPrefs()->SetBoolean(ash::prefs::kIsolatedWebAppsEnabled,
                                      true);
#endif
  }

 protected:
  web_app::FakeIwaRuntimeDataProviderMixin data_provider_{&mixin_host_};

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(IsolatedWebAppInstallerBrowserTest,
                       ValidBundleInstallAndLaunch) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(ManifestBuilder()).BuildBundle();
  app->TrustSigningKey();
  webapps::AppId app_id =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(app->web_bundle_id())
          .app_id();

  data_provider_->Update([&](auto& update) {
    update.AddToUserInstallAllowlist(
        app->web_bundle_id(),
        ChromeIwaRuntimeDataProvider::UserInstallAllowlistItemData(
            /*enterprise_name=*/"fancy comp"));
  });

  base::test::TestFuture<void> on_closed_future;

  IsolatedWebAppInstallerCoordinator* coordinator =
      IsolatedWebAppInstallerCoordinator::CreateAndStart(
          profile(), app->path(), on_closed_future.GetCallback());

  IsolatedWebAppInstallerModel* model = coordinator->GetModelForTesting();
  ASSERT_TRUE(model);

  IsolatedWebAppInstallerViewController* controller =
      coordinator->GetControllerForTesting();
  ASSERT_TRUE(controller);

  TestIsolatedWebAppInstallerModelObserver model_observer(model);

  model_observer.WaitForStepChange(
      IsolatedWebAppInstallerModel::Step::kShowMetadata);

  views::Widget* main_widget = controller->GetWidgetForTesting();
  ASSERT_TRUE(main_widget);

  AcceptDialogAndContinue(main_widget);

  ASSERT_TRUE(model->has_dialog());
  ASSERT_TRUE(std::holds_alternative<
              IsolatedWebAppInstallerModel::ConfirmInstallationDialog>(
      model->dialog()));

  views::Widget* child_widget = controller->GetChildWidgetForTesting();
  ASSERT_TRUE(child_widget);

  // App is not installed.
  ASSERT_FALSE(
      provider().registrar_unsafe().GetInstallState(app_id).has_value());

  AcceptDialogAndAwaitDestruction(child_widget);

  ASSERT_EQ(model->step(), IsolatedWebAppInstallerModel::Step::kInstall);

  model_observer.WaitForStepChange(
      IsolatedWebAppInstallerModel::Step::kInstallSuccess);

  // App is installed.
  ASSERT_TRUE(provider().registrar_unsafe().AppMatches(
      app_id, WebAppFilter::IsIsolatedApp()));

  AcceptDialogAndContinue(main_widget);

  // Installer closed.
  ASSERT_TRUE(on_closed_future.Wait());
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppInstallerBrowserTest,
                       FailsWhenNotOnAllowlist) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(ManifestBuilder()).BuildBundle();

  base::test::TestFuture<void> on_closed_future;

  IsolatedWebAppInstallerCoordinator* coordinator =
      IsolatedWebAppInstallerCoordinator::CreateAndStart(
          profile(), app->path(), on_closed_future.GetCallback());

  IsolatedWebAppInstallerModel* model = coordinator->GetModelForTesting();
  ASSERT_TRUE(model);

  IsolatedWebAppInstallerViewController* controller =
      coordinator->GetControllerForTesting();
  ASSERT_TRUE(controller);

  TestIsolatedWebAppInstallerModelObserver model_observer(model);

  model_observer.WaitForStepChange(
      IsolatedWebAppInstallerModel::Step::kGetMetadata);

  // The "Not Allowlisted" dialog should appear during the kGetMetadata step.
  // We need to wait for the child widget to appear.
  views::Widget* child_widget = nullptr;
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return child_widget = controller->GetChildWidgetForTesting(); }));

  ASSERT_TRUE(model->has_dialog());
  ASSERT_TRUE(
      std::holds_alternative<IsolatedWebAppInstallerModel::
                                 BundleNotAllowlistedForUserInstallationDialog>(
          model->dialog()));

  views::test::CancelDialog(child_widget);
  ASSERT_TRUE(on_closed_future.Wait());

  EXPECT_EQ(model->step(), IsolatedWebAppInstallerModel::Step::kGetMetadata);

  webapps::AppId app_id =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(app->web_bundle_id())
          .app_id();
  ASSERT_FALSE(
      provider().registrar_unsafe().GetInstallState(app_id).has_value());
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppInstallerBrowserTest,
                       FailsWhenBlocklisted) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(ManifestBuilder()).BuildBundle();

  data_provider_->Update(
      [&](auto& update) { update.AddToBlocklist(app->web_bundle_id()); });

  base::test::TestFuture<void> on_closed_future;

  IsolatedWebAppInstallerCoordinator* coordinator =
      IsolatedWebAppInstallerCoordinator::CreateAndStart(
          profile(), app->path(), on_closed_future.GetCallback());

  IsolatedWebAppInstallerModel* model = coordinator->GetModelForTesting();
  ASSERT_TRUE(model);

  IsolatedWebAppInstallerViewController* controller =
      coordinator->GetControllerForTesting();
  ASSERT_TRUE(controller);

  TestIsolatedWebAppInstallerModelObserver model_observer(model);

  model_observer.WaitForStepChange(
      IsolatedWebAppInstallerModel::Step::kGetMetadata);

  // The "Not Allowlisted" dialog should appear during the kGetMetadata step.
  // We need to wait for the child widget to appear.
  views::Widget* child_widget = nullptr;
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return child_widget = controller->GetChildWidgetForTesting(); }));

  ASSERT_TRUE(model->has_dialog());
  ASSERT_TRUE(
      std::holds_alternative<
          IsolatedWebAppInstallerModel::BundleBlocklistedInstallationDialog>(
          model->dialog()));

  views::test::CancelDialog(child_widget);
  ASSERT_TRUE(on_closed_future.Wait());

  EXPECT_EQ(model->step(), IsolatedWebAppInstallerModel::Step::kGetMetadata);

  webapps::AppId app_id =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(app->web_bundle_id())
          .app_id();
  ASSERT_FALSE(
      provider().registrar_unsafe().GetInstallState(app_id).has_value());
}

class IsolatedWebAppInstallerDisabledBrowserTest
    : public WebAppBrowserTestBase {
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kIsolatedWebApps, features::kIsolatedWebAppDevMode},
        {features::kIsolatedWebAppUnmanagedInstall});
    WebAppBrowserTestBase::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(IsolatedWebAppInstallerDisabledBrowserTest,
                       DoesNotLaunchIfUnmanagedInstallIsDisabled) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(ManifestBuilder()).BuildBundle();
  app->TrustSigningKey();

  base::test::TestFuture<void> on_closed_future;

  profile()->GetPrefs()->SetBoolean(ash::prefs::kIsolatedWebAppsEnabled, true);

  IsolatedWebAppInstallerCoordinator* coordinator =
      IsolatedWebAppInstallerCoordinator::CreateAndStart(
          profile(), app->path(), on_closed_future.GetCallback());

  IsolatedWebAppInstallerModel* model = coordinator->GetModelForTesting();
  ASSERT_TRUE(model);

  ASSERT_TRUE(on_closed_future.Wait());

  EXPECT_EQ(model->step(), IsolatedWebAppInstallerModel::Step::kNone);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace web_app
