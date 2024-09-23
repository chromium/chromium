// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_view.h"

#include <optional>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/stl_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/version.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/pixel_test_configuration_mixin.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/fake_pref_observer.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_coordinator.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_model.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_view_controller.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/test_isolated_web_app_installer_model_observer.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_storage_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_metadata.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/shell.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/test/test_browser_dialog_mac.h"
#endif

namespace web_app {
namespace {

using Step = IsolatedWebAppInstallerModel::Step;

struct TestParam {
  std::string test_suffix;
  Step step;
  std::optional<IsolatedWebAppInstallerModel::Dialog> dialog = std::nullopt;
  bool use_dark_theme = false;
  bool use_right_to_left_language = false;
};

const TestParam kTestParam[] = {
    {.test_suffix = "Disabled", .step = Step::kDisabled},
    {.test_suffix = "GetMetadata", .step = Step::kGetMetadata},
    {.test_suffix = "ShowMetadata", .step = Step::kShowMetadata},
    {.test_suffix = "Install", .step = Step::kInstall},
    {.test_suffix = "Success", .step = Step::kInstallSuccess},
    {.test_suffix = "InvalidBundle",
     .step = Step::kGetMetadata,
     .dialog = IsolatedWebAppInstallerModel::BundleInvalidDialog{}},
    {.test_suffix = "AlreadyInstalled",
     .step = Step::kGetMetadata,
     .dialog =
         IsolatedWebAppInstallerModel::BundleAlreadyInstalledDialog{
             u"Test IWA", base::Version("1.0")}},
    {.test_suffix = "ConfirmInstall",
     .step = Step::kShowMetadata,
     .dialog =
         IsolatedWebAppInstallerModel::ConfirmInstallationDialog{
             base::DoNothing()}},
    {.test_suffix = "InstallationError",
     .step = Step::kInstall,
     .dialog = IsolatedWebAppInstallerModel::InstallationFailedDialog{}},
};

SignedWebBundleMetadata CreateTestMetadata() {
  IconBitmaps icons;
  AddGeneratedIcon(&icons.any, 32, SK_ColorBLUE);
  return SignedWebBundleMetadata::CreateForTesting(
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
          web_package::SignedWebBundleId::CreateRandomForProxyMode()),
      IwaSourceBundleProdMode(base::FilePath()), u"Test Isolated Web App",
      base::Version("0.0.1"), icons);
}

// To be passed as 4th argument to `INSTANTIATE_TEST_SUITE_P()`, allows the test
// to be named like `<TestClassName>.InvokeUi_default/<TestSuffix>` instead
// of using the index of the param in `TestParam` as suffix.
std::string ParamToTestSuffix(const ::testing::TestParamInfo<TestParam>& info) {
  return info.param.test_suffix;
}

using MixinBasedUiBrowserTest =
    SupportsTestUi<MixinBasedInProcessBrowserTest, TestBrowserUi>;

// Takes a screenshot of a Widget with the name returned by `widget_name()`.
// Both `widget_name()` and `ShowUi()` must be overridden.
class NamedWidgetUiPixelTest : public MixinBasedUiBrowserTest {
 public:
  NamedWidgetUiPixelTest(bool use_dark_theme, bool use_right_to_left_language)
      : pixel_test_mixin_(&mixin_host_,
                          use_dark_theme,
                          use_right_to_left_language) {}

  ~NamedWidgetUiPixelTest() override = default;

 protected:
  virtual std::string widget_name() = 0;

  void PreShow() override { UpdateWidgets(); }

  bool VerifyUi() override {
    views::Widget::Widgets widgets_before = widgets_;
    UpdateWidgets();

    // Force pending layouts of all existing widgets. This ensures any
    // anchor Views are in the correct position.
    for (views::Widget* widget : widgets_) {
      widget->LayoutRootViewIfNecessary();
    }

    // Find the widget named `widget_name()` that was opened during `ShowUi()`.
    widgets_ = base::STLSetDifference<views::Widget::Widgets>(widgets_,
                                                              widgets_before);
    std::string name = widget_name();
    std::erase_if(widgets_, [&](views::Widget* widget) {
      return widget->GetName() != name;
    });
    if (widgets_.size() != 1) {
      LOG(ERROR) << "VerifyUi(): Expected 1 added widget with name '" << name
                 << "'; got " << widgets_.size();
      return false;
    }

    views::Widget* widget = *(widgets_.begin());
    widget->SetBlockCloseForTesting(true);
    // Deactivate before taking screenshot. Deactivated dialog pixel outputs
    // is more predictable than activated dialog.
    widget->Deactivate();
    widget->GetFocusManager()->ClearFocus();
    absl::Cleanup unblock_close = [widget] {
      widget->SetBlockCloseForTesting(false);
    };

    auto* test_info = testing::UnitTest::GetInstance()->current_test_info();
    const std::string screenshot_name =
        base::StrCat({test_info->test_suite_name(), "_", test_info->name()});

    if (VerifyPixelUi(widget, "BrowserUiDialog", screenshot_name) ==
        ui::test::ActionResult::kFailed) {
      LOG(ERROR) << "VerifyUi(): Pixel compare failed.";
      return false;
    }

    return true;
  }

  void WaitForUserDismissal() override {
#if BUILDFLAG(IS_MAC)
    ::internal::TestBrowserDialogInteractiveSetUp();
#endif

    ASSERT_EQ(1UL, widgets_.size());
    views::test::WidgetDestroyedWaiter waiter(*widgets_.begin());
    waiter.Wait();
  }

  void DismissUi() override {
    ASSERT_EQ(1UL, widgets_.size());
    views::Widget* widget = *widgets_.begin();
    views::test::WidgetDestroyedWaiter waiter(widget);
    widget->CloseNow();
    waiter.Wait();
  }

 private:
  // Stores the current widgets in |widgets_|.
  void UpdateWidgets() {
    widgets_.clear();
#if BUILDFLAG(IS_CHROMEOS_ASH)
    for (aura::Window* root_window : ash::Shell::GetAllRootWindows()) {
      views::Widget::GetAllChildWidgets(root_window, &widgets_);
    }
#else
    widgets_ = views::test::WidgetTest::GetAllWidgets();
#endif
  }

  PixelTestConfigurationMixin pixel_test_mixin_;

  // The widgets present before/after showing UI.
  views::Widget::Widgets widgets_;
};

}  // namespace

class IsolatedWebAppInstallerViewUiPixelTest
    : public NamedWidgetUiPixelTest,
      public testing::WithParamInterface<TestParam> {
 public:
  IsolatedWebAppInstallerViewUiPixelTest()
      : NamedWidgetUiPixelTest(GetParam().use_dark_theme,
                               GetParam().use_right_to_left_language) {
    feature_list_.InitWithFeatures(
        {features::kIsolatedWebApps, features::kIsolatedWebAppUnmanagedInstall},
        {});
  }

  ~IsolatedWebAppInstallerViewUiPixelTest() override = default;

 protected:
  // `NamedWidgetUiPixelTest`:
  std::string widget_name() override {
    return GetParam().dialog.has_value()
               ? IsolatedWebAppInstallerView::kNestedDialogWidgetName
               : IsolatedWebAppInstallerView::kInstallerWidgetName;
  }

  void ShowUi(const std::string& name) override {
    Profile* profile = browser()->profile();
    IsolatedWebAppInstallerCoordinator* coordinator =
        IsolatedWebAppInstallerCoordinator::CreateAndStart(
            profile, base::FilePath(), on_complete_future.GetCallback(),
            std::make_unique<FakeIsolatedWebAppsEnabledPrefObserver>(false));

    IsolatedWebAppInstallerModel* model = coordinator->GetModelForTesting();
    ASSERT_TRUE(model);

    IsolatedWebAppInstallerViewController* controller =
        coordinator->GetControllerForTesting();
    ASSERT_TRUE(controller);

    TestIsolatedWebAppInstallerModelObserver model_observer(model);

    model_observer.WaitForStepChange(Step::kDisabled);

    widget_ = controller->GetWidgetForTesting();
    ASSERT_TRUE(widget_);

    model->SetSignedWebBundleMetadata(CreateTestMetadata());
    model->SetStep(GetParam().step);

    model_observer.WaitForStepChange(GetParam().step);

    if (GetParam().dialog.has_value()) {
      CHECK(!model->has_dialog());
      model->SetDialog(GetParam().dialog);
      model_observer.WaitForChildDialog();
    }
  }

  void DismissUi() override {
    ASSERT_TRUE(widget_);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce([](views::Widget* widget) { widget->Close(); },
                       widget_));
    widget_ = nullptr;
    ASSERT_TRUE(on_complete_future.Wait());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::test::TestFuture<void> on_complete_future;
  raw_ptr<views::Widget> widget_;
};

IN_PROC_BROWSER_TEST_P(IsolatedWebAppInstallerViewUiPixelTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(,
                         IsolatedWebAppInstallerViewUiPixelTest,
                         testing::ValuesIn(kTestParam),
                         &ParamToTestSuffix);

}  // namespace web_app
