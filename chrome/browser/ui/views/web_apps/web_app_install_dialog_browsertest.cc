// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <numeric>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/shortcuts/shortcut_icon_generator.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/web_apps/progress_delay.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_dialog_delegate.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_flow_dialog_delegate.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_options_view.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_screenshot_fetcher.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/vector_icons/vector_icons.h"
#include "components/webapps/browser/installable/ml_install_operation_tracker.h"
#include "components/webapps/browser/installable/ml_installability_promoter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/dialog_delegate.h"

namespace web_app {

namespace {

int GetTimesToAcceptDialog(const std::string& name, InstallOsType os_type) {
  if (name == "Intro") {
    return 0;
  } else if (name == "InstallOptions") {
    return 1;
  } else if (name == "Progress") {
    // Adjust the click count for `kOther` because the `InstallOptions` step is
    // skipped on this OS.
    return os_type == InstallOsType::kOther ? 1 : 2;
  } else if (name == "Successful") {
    return os_type == InstallOsType::kOther ? 1 : 2;
  }

  NOTREACHED();
}

// The number of times the dialog could be accepted before the dialog closes.
const int kAcceptsBeforeClosure = 3;

}  // namespace

class TestWebAppScreenshotFetcher : public WebAppScreenshotFetcher {
 public:
  TestWebAppScreenshotFetcher() = default;
  ~TestWebAppScreenshotFetcher() override = default;

  void GetScreenshot(
      int index,
      base::OnceCallback<void(SkBitmap bitmap,
                              std::optional<std::u16string> label)> callback)
      override {
    // Green "screenshot" image for test.
    SkBitmap bitmap;
    bitmap.allocN32Pixels(sizes_[index].width(), sizes_[index].height());
    bitmap.eraseColor(SK_ColorGREEN);
    std::move(callback).Run(std::move(bitmap),
                            std::u16string(u"Test Screenshot"));
  }

  const std::vector<gfx::Size>& GetScreenshotSizes() override { return sizes_; }

  base::WeakPtr<WebAppScreenshotFetcher> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  std::vector<gfx::Size> sizes_ = {gfx::Size(400, 300), gfx::Size(400, 300),
                                   gfx::Size(400, 300)};
  base::WeakPtrFactory<TestWebAppScreenshotFetcher> weak_ptr_factory_{this};
};

using WebAppInstallDialogTestParams =
    std::tuple<InstallOsType, InstallDialogType>;

class WebAppInstallDialogBrowserTest
    : public DialogBrowserTest,
      public testing::WithParamInterface<WebAppInstallDialogTestParams> {
 public:
  WebAppInstallDialogBrowserTest() {
    // TODO(b/492657940): Remove this once the feature is launched.
    feature_list_.InitWithFeatures(
        {features::kWebAppInstallDialog, features::kWebAppInstallDialogWinPin},
        {});
  }
  WebAppInstallDialogBrowserTest(const WebAppInstallDialogBrowserTest&) =
      delete;
  WebAppInstallDialogBrowserTest& operator=(
      const WebAppInstallDialogBrowserTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    InstallOsType os_type = GetOsType();
    InstallDialogType dialog_type = GetDialogType();
    auto install_info = WebAppInstallInfo::CreateWithStartUrlForTesting(
        GURL("https://example.com"));
    install_info->title = u"Test App";
    auto icon = ::shortcuts::GenerateBitmap(kIconSize, u"A");
    install_info->icon_bitmaps.any[kIconSize] = icon;
    install_info->trusted_icon_bitmaps.any[kIconSize] = icon;
    install_info->description = u"This is a test app";

    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         "WebAppInstallFlowDialog");

    auto* promoter = webapps::MLInstallabilityPromoter::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
    auto install_tracker = promoter->RegisterCurrentInstallForWebContents(
        webapps::WebappInstallSource::MENU_BROWSER_TAB);

    std::optional<ui::ImageModel> folder_image_model;
    std::optional<std::u16string> folder_label;
    // TODO(crbug.com/513311056): Mac uses the folder icon from the OS. Because
    // screenshot tests are only built on Windows, we have to use a placeholder
    // or skip these tests. We should explore the options for making Mac tests
    // more robust.
    if (os_type == InstallOsType::kMac) {
      folder_image_model = ui::ImageModel::FromVectorIcon(
          vector_icons::kFolderChromeRefreshOldIcon, ui::kColorIcon,
          kLargeImageSize);
      folder_label = u"Test Folder";
    }

    delegate_ = WebAppInstallFlowDialogDelegate::Show(
        browser()->tab_strip_model()->GetActiveWebContents(),
        std::move(install_info), std::move(install_tracker),
        base::BindOnce(&WebAppInstallDialogBrowserTest::OnDialogCompleted,
                       weak_ptr_factory_.GetWeakPtr()),
        PwaInProductHelpState::kNotShown,
        dialog_type == InstallDialogType::kDetailed
            ? screenshot_fetcher_.GetWeakPtr()
            : base::WeakPtr<WebAppScreenshotFetcher>(),
        /*show_initiating_origin=*/false, dialog_type, os_type,
        std::make_unique<ProgressDelay>(/*delay_time=*/base::Seconds(0),
                                        /*steps=*/1),
        folder_image_model, folder_label);

    views::Widget* widget = waiter.WaitIfNeededAndGet();
    ASSERT_NE(nullptr, widget);
    widget_ = widget->GetWeakPtr();
    std::vector<std::string> parts = base::SplitString(
        name, "_", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    std::vector<int> parts_to_accept_times;
    // Map the separate parts into their "number of times to accept".
    // "Successful" -> [2]
    for (const std::string& part : parts) {
      parts_to_accept_times.push_back(GetTimesToAcceptDialog(part, os_type));
    }

    int total_accepts = std::accumulate(parts_to_accept_times.begin(),
                                        parts_to_accept_times.end(), 0);
    // The number of times Accept is clicked should not exceed the minimum
    // amount.
    ASSERT_LE(total_accepts, kAcceptsBeforeClosure);

    // Accept the dialog according to GetTimesToAcceptDialog.
    for (int part_times : parts_to_accept_times) {
      for (int i = 0; i < part_times; ++i) {
        ASSERT_TRUE(widget_);
        widget_->widget_delegate()->AsDialogDelegate()->AcceptDialog();
      }
    }

    if (name.contains("Successful")) {
      // Ensure the mock installation completes so we can reach the Success
      // step.
      CompleteInstall(true);

      // Wait for the dialog to reach the Successful step.
      ASSERT_TRUE(base::test::RunUntil([&]() {
        return delegate_ && delegate_->GetCurrentStepForTesting() ==
                                InstallDialogStep::kSuccessful;
      }));
    }
  }

  void OnDialogCompleted(
      bool accepted,
      std::unique_ptr<WebAppInstallInfo> install_info,
      WebAppInstallationAcceptanceResultCallback result_callback) {
    dialog_accepted_ = accepted;
    dialog_install_info_ = std::move(install_info);
    install_result_callback_ = std::move(result_callback);
  }

  void CompleteInstall(bool success) {
    std::move(install_result_callback_).Run(success, base::DoNothing());
  }

 protected:
  InstallOsType GetOsType() { return std::get<0>(GetParam()); }
  std::optional<bool> dialog_accepted_;
  std::unique_ptr<WebAppInstallInfo> dialog_install_info_;
  WebAppInstallationAcceptanceResultCallback install_result_callback_;
  TestWebAppScreenshotFetcher screenshot_fetcher_;
  base::WeakPtr<views::Widget> widget_ = nullptr;
  base::WeakPtr<WebAppInstallFlowDialogDelegate> delegate_ = nullptr;

 private:
  InstallDialogType GetDialogType() { return std::get<1>(GetParam()); }

  base::test::ScopedFeatureList feature_list_;
  base::WeakPtrFactory<WebAppInstallDialogBrowserTest> weak_ptr_factory_{this};
};

// TODO(crbug.com/510455863): Re-enable the test
#if BUILDFLAG(IS_WIN)
#define MAYBE_InvokeUi_Intro DISABLED_InvokeUi_Intro
#else
#define MAYBE_InvokeUi_Intro InvokeUi_Intro
#endif
IN_PROC_BROWSER_TEST_P(WebAppInstallDialogBrowserTest, MAYBE_InvokeUi_Intro) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(WebAppInstallDialogBrowserTest,
                       InvokeUi_InstallOptions) {
  if (std::get<0>(GetParam()) == InstallOsType::kOther) {
    GTEST_SKIP() << "InstallOptions step does not exist for kOther";
  }
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(WebAppInstallDialogBrowserTest, InvokeUi_Progress) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(WebAppInstallDialogBrowserTest, InvokeUi_Successful) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(
    /* prefix */,
    WebAppInstallDialogBrowserTest,
    testing::Combine(testing::Values(InstallOsType::kWin,
                                     InstallOsType::kMac,
                                     InstallOsType::kCros,
                                     InstallOsType::kOther),
                     testing::Values(InstallDialogType::kSimple,
                                     InstallDialogType::kDetailed,
                                     InstallDialogType::kDiy)),
    [](const testing::TestParamInfo<WebAppInstallDialogTestParams>& info) {
      std::string os_type = base::ToString(std::get<0>(info.param));
      std::string dialog_type = base::ToString(std::get<1>(info.param));
      return os_type + "_" + dialog_type;
    });

using WebAppInstallDialogClosedTest = WebAppInstallDialogBrowserTest;

IN_PROC_BROWSER_TEST_P(WebAppInstallDialogClosedTest, CancelInIntro) {
  ShowUi("Intro");
  ASSERT_TRUE(widget_);
  views::test::WidgetDestroyedWaiter destruction_waiter(widget_.get());
  widget_->widget_delegate()->AsDialogDelegate()->CancelDialog();
  destruction_waiter.Wait();
  EXPECT_EQ(dialog_accepted_, false);
}

IN_PROC_BROWSER_TEST_P(WebAppInstallDialogClosedTest, CancelInSuccess) {
  ShowUi("Successful");
  ASSERT_TRUE(widget_);
  views::test::WidgetDestroyedWaiter destruction_waiter(widget_.get());
  widget_->widget_delegate()->AsDialogDelegate()->CancelDialog();
  destruction_waiter.Wait();
  EXPECT_EQ(dialog_accepted_, true);
}

IN_PROC_BROWSER_TEST_P(WebAppInstallDialogClosedTest, Success) {
  // Accept through all the dialog steps
  // kInstallDialog -> kInstallerOptions -> kProgress -> kSuccessful
  ShowUi("Successful");
  ASSERT_TRUE(widget_);
  views::test::WidgetDestroyedWaiter destruction_waiter(widget_.get());
  widget_->widget_delegate()->AsDialogDelegate()->AcceptDialog();
  destruction_waiter.Wait();
  EXPECT_EQ(dialog_accepted_, true);
  EXPECT_TRUE(dialog_install_info_);
}

INSTANTIATE_TEST_SUITE_P(
    /** prefix */,
    WebAppInstallDialogClosedTest,
    testing::Combine(testing::Values(InstallOsType::kOther),
                     testing::Values(InstallDialogType::kSimple,
                                     InstallDialogType::kDetailed,
                                     InstallDialogType::kDiy)),
    [](const testing::TestParamInfo<WebAppInstallDialogTestParams>& info) {
      std::string os_type = base::ToString(std::get<0>(info.param));
      std::string dialog_type = base::ToString(std::get<1>(info.param));
      return os_type + "_" + dialog_type;
    });

class WebAppInstallDialogCheckboxTest : public WebAppInstallDialogBrowserTest {
 public:
  WebAppInstallDialogCheckboxTest() {
    checkbox_feature_list_.InitAndDisableFeature(
        features::kWebAppInstallDialogWinPin);
  }

 protected:
  WebAppInstallOptionsView* NavigateToAndGetOptionsView() {
    ShowUi("InstallOptions");
    EXPECT_TRUE(widget_);

    views::View* options_view =
        views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
            WebAppInstallFlowDialogDelegate::kOptionsViewId,
            views::ElementTrackerViews::GetContextForWidget(widget_.get()));
    EXPECT_TRUE(options_view);
    if (!options_view) {
      return nullptr;
    }
    return static_cast<WebAppInstallOptionsView*>(options_view);
  }

  void CompleteInstallationAndVerifyDialogAccepted() {
    // Accept the dialog until the installation is completed (InstallOptions ->
    // Progress -> Successful).
    widget_->widget_delegate()->AsDialogDelegate()->AcceptDialog();
    widget_->widget_delegate()->AsDialogDelegate()->AcceptDialog();

    // Close the dialog.
    views::test::WidgetDestroyedWaiter destruction_waiter(widget_.get());
    widget_->widget_delegate()->AsDialogDelegate()->AcceptDialog();
    destruction_waiter.Wait();

    EXPECT_EQ(dialog_accepted_, true);
    ASSERT_TRUE(dialog_install_info_);
  }

  bool IsWin() { return GetOsType() == InstallOsType::kWin; }
  bool IsCros() { return GetOsType() == InstallOsType::kCros; }

 private:
  base::test::ScopedFeatureList checkbox_feature_list_;
};

IN_PROC_BROWSER_TEST_P(WebAppInstallDialogCheckboxTest,
                       VerifyCrosCheckboxUnchecked) {
  if (!IsCros()) {
    GTEST_SKIP() << "Pin to shelf checkbox only works on CrOS";
  }
  WebAppInstallOptionsView* options_view = NavigateToAndGetOptionsView();
  ASSERT_TRUE(options_view);
  options_view->SetPinToShelfCheckedForTesting(false);
  CompleteInstallationAndVerifyDialogAccepted();
  EXPECT_EQ(dialog_install_info_->add_to_quick_launch_bar, false);
}

IN_PROC_BROWSER_TEST_P(WebAppInstallDialogCheckboxTest,
                       VerifyCrosCheckboxChecked) {
  if (!IsCros()) {
    GTEST_SKIP() << "Pin to shelf checkbox only works on CrOS";
  }
  WebAppInstallOptionsView* options_view = NavigateToAndGetOptionsView();
  ASSERT_TRUE(options_view);
  options_view->SetPinToShelfCheckedForTesting(true);
  CompleteInstallationAndVerifyDialogAccepted();
  EXPECT_EQ(dialog_install_info_->add_to_quick_launch_bar, true);
}

IN_PROC_BROWSER_TEST_P(WebAppInstallDialogCheckboxTest,
                       VerifyWinCheckboxNotPresent) {
  if (!IsWin()) {
    GTEST_SKIP() << "Creating desktop shortcut and pinning to task bar only "
                    "works for Windows";
  }
  WebAppInstallOptionsView* options_view = NavigateToAndGetOptionsView();
  ASSERT_TRUE(options_view);

  views::View* checkbox =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          WebAppInstallOptionsView::kPinToTaskbarCheckboxId,
          views::ElementTrackerViews::GetContextForWidget(widget_.get()));
  EXPECT_FALSE(checkbox);

  CompleteInstallationAndVerifyDialogAccepted();
  EXPECT_EQ(dialog_install_info_->add_to_quick_launch_bar, false);
}

INSTANTIATE_TEST_SUITE_P(
    /* prefix */,
    WebAppInstallDialogCheckboxTest,
    testing::Combine(testing::Values(InstallOsType::kCros, InstallOsType::kWin),
                     testing::Values(InstallDialogType::kSimple,
                                     InstallDialogType::kDetailed,
                                     InstallDialogType::kDiy)),
    [](const testing::TestParamInfo<WebAppInstallDialogTestParams>& info) {
      std::string os_type = base::ToString(std::get<0>(info.param));
      std::string dialog_type = base::ToString(std::get<1>(info.param));
      return os_type + "_" + dialog_type;
    });

class WebAppInstallDialogCheckboxEnabledTest
    : public WebAppInstallDialogCheckboxTest {
 public:
  WebAppInstallDialogCheckboxEnabledTest() {
    feature_list_.InitAndEnableFeature(features::kWebAppInstallDialogWinPin);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(WebAppInstallDialogCheckboxEnabledTest,
                       VerifyWinCheckboxUnchecked) {
  if (!IsWin()) {
    GTEST_SKIP() << "Creating desktop shortcut and pinning to task bar only "
                    "works for Windows";
  }
  WebAppInstallOptionsView* options_view = NavigateToAndGetOptionsView();
  ASSERT_TRUE(options_view);
  options_view->SetAddDesktopShortcutCheckedForTesting(false);
  options_view->SetPinToTaskBarCheckedForTesting(false);
  CompleteInstallationAndVerifyDialogAccepted();
  EXPECT_EQ(dialog_install_info_->add_to_desktop, false);
  EXPECT_EQ(dialog_install_info_->add_to_quick_launch_bar, false);
}

IN_PROC_BROWSER_TEST_P(WebAppInstallDialogCheckboxEnabledTest,
                       VerifyWinCheckboxChecked) {
  if (!IsWin()) {
    GTEST_SKIP() << "Creating desktop shortcut and pinning to task bar only "
                    "works for Windows";
  }
  WebAppInstallOptionsView* options_view = NavigateToAndGetOptionsView();
  ASSERT_TRUE(options_view);
  options_view->SetAddDesktopShortcutCheckedForTesting(true);
  options_view->SetPinToTaskBarCheckedForTesting(true);
  CompleteInstallationAndVerifyDialogAccepted();
  EXPECT_EQ(dialog_install_info_->add_to_desktop, true);
  EXPECT_EQ(dialog_install_info_->add_to_quick_launch_bar, true);
}

INSTANTIATE_TEST_SUITE_P(
    /* prefix */,
    WebAppInstallDialogCheckboxEnabledTest,
    testing::Combine(testing::Values(InstallOsType::kWin),
                     testing::Values(InstallDialogType::kSimple,
                                     InstallDialogType::kDetailed,
                                     InstallDialogType::kDiy)),
    [](const testing::TestParamInfo<WebAppInstallDialogTestParams>& info) {
      std::string os_type = base::ToString(std::get<0>(info.param));
      std::string dialog_type = base::ToString(std::get<1>(info.param));
      return os_type + "_" + dialog_type;
    });

}  // namespace web_app
