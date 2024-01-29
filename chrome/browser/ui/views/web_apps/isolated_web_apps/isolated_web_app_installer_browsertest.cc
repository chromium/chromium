// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string_view>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/fake_pref_observer.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/installability_checker.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_coordinator.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_model.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_view_controller.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/pref_observer.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/test_isolated_web_app_installer_model_observer.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/test_signed_web_bundle_builder.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

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

class IsolatedWebAppInstallerBrowserTest : public WebAppControllerBrowserTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kIsolatedWebApps, features::kIsolatedWebAppDevMode}, {});
    WebAppControllerBrowserTest::SetUp();
  }

  base::FilePath BuildBundleAndWrite(
      std::string_view bundle_file_name,
      std::string_view version_string,
      std::optional<web_package::WebBundleSigner::ErrorsForTesting> errors) {
    base::Version version(version_string);
    CHECK(temp_dir_.CreateUniqueTempDir());
    const base::FilePath& dir_path = temp_dir_.GetPath();
    base::FilePath bundle_path =
        dir_path.Append(base::FilePath::FromASCII(bundle_file_name));

    TestSignedWebBundleBuilder builder;
    TestSignedWebBundleBuilder::BuildOptions build_options;
    build_options.SetVersion(version);
    if (errors.has_value()) {
      build_options.SetErrorsForTesting(errors.value());
    }
    TestSignedWebBundle test_bundle = builder.BuildDefault(build_options);
    web_package::SignedWebBundleId signed_web_bundle_id(test_bundle.id);
    IsolatedWebAppUrlInfo url_info =
        IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
            signed_web_bundle_id);
    app_id_ = url_info.app_id();
    CHECK(base::WriteFile(bundle_path, test_bundle.data));
    return bundle_path;
  }

  const webapps::AppId& app_id() const { return app_id_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir temp_dir_;
  webapps::AppId app_id_;
};

IN_PROC_BROWSER_TEST_F(IsolatedWebAppInstallerBrowserTest,
                       ValidBundleInstallAndLaunch) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::FilePath bundle_path = BuildBundleAndWrite(
      "test_bundle_good.swbn", "1.0.0", /*errors=*/std::nullopt);

  base::test::TestFuture<void> on_closed_future;

  IsolatedWebAppInstallerCoordinator* coordinator =
      IsolatedWebAppInstallerCoordinator::CreateAndStart(
          profile(), bundle_path, on_closed_future.GetCallback(),
          std::make_unique<FakeIsolatedWebAppsEnabledPrefObserver>(true));

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
  ASSERT_TRUE(absl::holds_alternative<
              IsolatedWebAppInstallerModel::ConfirmInstallationDialog>(
      model->dialog()));

  views::Widget* child_widget = controller->GetChildWidgetForTesting();
  ASSERT_TRUE(child_widget);

  // App is not installed.
  ASSERT_FALSE(provider().registrar_unsafe().IsInstalled(app_id()));

  AcceptDialogAndAwaitDestruction(child_widget);

  ASSERT_EQ(model->step(), IsolatedWebAppInstallerModel::Step::kInstall);

  model_observer.WaitForStepChange(
      IsolatedWebAppInstallerModel::Step::kInstallSuccess);

  // App is installed.
  ASSERT_TRUE(provider().registrar_unsafe().IsInstalled(app_id()));

  AcceptDialogAndContinue(main_widget);

  // Installer closed.
  ASSERT_TRUE(on_closed_future.Wait());
}

}  // namespace web_app
