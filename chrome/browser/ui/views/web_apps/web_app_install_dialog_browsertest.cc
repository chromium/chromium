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
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/ui_manager/update_dialog_types.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_screenshot_fetcher.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/webapps/browser/installable/ml_install_operation_tracker.h"
#include "components/webapps/browser/installable/ml_installability_promoter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"
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
    return os_type == InstallOsType::kOther ? 2 : 3;
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
  std::vector<gfx::Size> sizes_ = {gfx::Size(400, 300)};
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
    feature_list_.InitAndEnableFeature(features::kWebAppInstallDialog);
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

    WebAppInstallFlowDialogDelegate::Show(
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
                                        /*steps=*/1));

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
  }

  void OnDialogCompleted(bool accepted,
                         std::unique_ptr<WebAppInstallInfo> install_info) {
    dialog_accepted_ = accepted;
    dialog_install_info_ = std::move(install_info);
  }

 protected:
  std::optional<bool> dialog_accepted_;
  std::unique_ptr<WebAppInstallInfo> dialog_install_info_;
  TestWebAppScreenshotFetcher screenshot_fetcher_;
  base::WeakPtr<views::Widget> widget_ = nullptr;

 private:
  InstallOsType GetOsType() { return std::get<0>(GetParam()); }
  InstallDialogType GetDialogType() { return std::get<1>(GetParam()); }

  base::test::ScopedFeatureList feature_list_;
  base::WeakPtrFactory<WebAppInstallDialogBrowserTest> weak_ptr_factory_{this};
};

IN_PROC_BROWSER_TEST_P(WebAppInstallDialogBrowserTest, InvokeUi_Intro) {
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
  EXPECT_EQ(dialog_accepted_, false);
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

}  // namespace web_app
