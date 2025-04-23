// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/picture_in_picture/document_picture_in_picture_mixin_test_base.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_occlusion_tracker.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/web_apps/web_app_dialog_test_utils.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_screenshot_fetcher.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/webapps/browser/installable/installable_data.h"
#include "components/webapps/browser/installable/ml_install_operation_tracker.h"
#include "components/webapps/browser/installable/ml_installability_promoter.h"
#include "components/webapps/common/constants.h"
#include "content/public/browser/document_picture_in_picture_window_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"

namespace web_app {

namespace {

static constexpr int kIconSize = 40;
static constexpr SkColor kIconColor = SK_ColorGREEN;
static constexpr int kScreenshotSize = 300;

enum class CarouselState {
  kLoading = 0,
  kScreenshot = 1,
  kMaxValue = kScreenshot
};

SkBitmap CreateSolidColorIcon(int width, int height, SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(color);
  return bitmap;
}

std::unique_ptr<WebAppInstallInfo> GetInstallInfo() {
  auto install_info = WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://example.com"));
  install_info->title = u"test";
  install_info->description = u"This is a test app";

  install_info->icon_bitmaps.any[kIconSize] =
      CreateSolidColorIcon(kIconSize, kIconSize, kIconColor);
  return install_info;
}

std::vector<webapps::Screenshot> GetScreenshots(const std::string& type) {
  std::vector<webapps::Screenshot> screenshots;
  if (type == "single_screenshot") {
    screenshots.emplace_back(
        CreateSolidColorIcon(kScreenshotSize, kScreenshotSize, SK_ColorGREEN),
        u"example screenshot");
  } else if (type == "multiple_screenshots") {
    screenshots.emplace_back(
        CreateSolidColorIcon(kScreenshotSize, kScreenshotSize, SK_ColorGREEN),
        u"example screenshot");
    screenshots.emplace_back(
        CreateSolidColorIcon(kScreenshotSize, kScreenshotSize, SK_ColorBLACK),
        u"example screenshot 2");
    screenshots.emplace_back(
        CreateSolidColorIcon(kScreenshotSize, kScreenshotSize, SK_ColorBLUE),
        u"");
  } else if (type == "max_ratio_screenshot") {
    screenshots.emplace_back(
        CreateSolidColorIcon(webapps::kMaximumScreenshotRatio * kScreenshotSize,
                             kScreenshotSize, SK_ColorGREEN),
        std::nullopt);
  } else {
    screenshots.emplace_back(
        CreateSolidColorIcon(kScreenshotSize, kScreenshotSize, SK_ColorGREEN),
        std::nullopt);
  }

  return screenshots;
}

std::unique_ptr<webapps::MlInstallOperationTracker> GetMLInstallTracker(
    Browser* browser) {
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  return webapps::MLInstallabilityPromoter::FromWebContents(web_contents)
      ->RegisterCurrentInstallForWebContents(
          webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON);
}

// A stub WebAppScreenshotFetcher that can be used to mimic the fetching of
// screenshots and well as states like a screenshot not loading or a delayed
// fetch to test various stages of the detailed install dialog.
class FakeScreenshotFetcher : public WebAppScreenshotFetcher {
 public:
  FakeScreenshotFetcher(const std::vector<webapps::Screenshot>& screenshots,
                        base::flat_set<int> indices_to_skip)
      : screenshots_(screenshots), indices_to_skip_(indices_to_skip) {
    for (const auto& screenshot : screenshots) {
      screenshot_sizes_.emplace_back(screenshot.image.width(),
                                     screenshot.image.height());
    }
  }

  ~FakeScreenshotFetcher() override = default;

  // WebAppScreenshotFetcher overrides:
  void GetScreenshot(
      int index,
      base::OnceCallback<void(SkBitmap, std::optional<std::u16string>)>
          callback) override {
    // Handle out of bounds.
    if (index >= static_cast<int>(screenshots_.size())) {
      std::move(callback).Run(SkBitmap(), std::nullopt);
      return;
    }

    base::OnceClosure image_loaded_callback =
        base::BindOnce(std::move(callback), screenshots_[index].image,
                       screenshots_[index].label);

    if (IsSkippedIndex(index)) {
      delayed_callbacks_.AddUnsafe(std::move(image_loaded_callback));
      return;
    }

    std::move(image_loaded_callback).Run();
  }

  const std::vector<gfx::Size>& GetScreenshotSizes() override {
    return screenshot_sizes_;
  }

  base::WeakPtr<FakeScreenshotFetcher> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void LoadDelayedScreenshots() { delayed_callbacks_.Notify(); }

 private:
  bool IsSkippedIndex(int index) {
    auto it = indices_to_skip_.find(index);
    return it != indices_to_skip_.end();
  }

  std::vector<webapps::Screenshot> screenshots_;
  std::vector<gfx::Size> screenshot_sizes_;
  base::flat_set<int> indices_to_skip_;
  base::OnceClosureList delayed_callbacks_;
  base::WeakPtrFactory<FakeScreenshotFetcher> weak_ptr_factory_{this};
};

class WebAppDetailedInstallDialogBrowserTest : public DialogBrowserTest {
 public:
  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    if (!fetcher_) {
      fetcher_ = std::make_unique<FakeScreenshotFetcher>(GetScreenshots(name),
                                                         base::flat_set<int>());
    }
    ShowWebAppDetailedInstallDialog(
        browser()->tab_strip_model()->GetWebContentsAt(0), GetInstallInfo(),
        GetMLInstallTracker(browser()),
        base::BindLambdaForTesting(
            [&](bool result, std::unique_ptr<WebAppInstallInfo>) {
              dialog_accepted_ = result;
            }),
        screenshot_fetcher(), PwaInProductHelpState::kNotShown);
  }

  std::optional<bool> dialog_accepted() { return dialog_accepted_; }

  base::WeakPtr<FakeScreenshotFetcher> screenshot_fetcher() {
    return fetcher_->GetWeakPtr();
  }

  void SetScreenshotFetcher(std::unique_ptr<FakeScreenshotFetcher> fetcher) {
    fetcher_ = std::move(fetcher);
  }

  std::vector<CarouselState> GetActualCarouselStateFromWidget(
      views::Widget* widget) {
    std::vector<CarouselState> carousel_state;

    views::ElementTrackerViews* tracker_views =
        views::ElementTrackerViews::GetInstance();
    ui::ElementContext context =
        views::ElementTrackerViews::GetContextForWidget(widget);
    views::View* image_container = tracker_views->GetUniqueView(
        kDetailedInstallDialogImageContainer, context);
    CHECK_NE(image_container, nullptr);

    for (const auto& view : image_container->children()) {
      if (views::AsViewClass<views::BoxLayoutView>(view)) {
        carousel_state.push_back(CarouselState::kLoading);
        continue;
      }

      CHECK(views::AsViewClass<views::ImageView>(view));
      carousel_state.push_back(CarouselState::kScreenshot);
    }

    return carousel_state;
  }

 private:
  std::unique_ptr<FakeScreenshotFetcher> fetcher_;
  std::optional<bool> dialog_accepted_ = std::nullopt;
};

IN_PROC_BROWSER_TEST_F(WebAppDetailedInstallDialogBrowserTest,
                       InvokeUi_single_screenshot) {
  ShowAndVerifyUi();
  // `ShowAndVerifyUi` close the dialog without accept/cancel.
  ASSERT_FALSE(dialog_accepted().value());
}

IN_PROC_BROWSER_TEST_F(WebAppDetailedInstallDialogBrowserTest,
                       InvokeUi_multiple_screenshots) {
  ShowAndVerifyUi();
  ASSERT_FALSE(dialog_accepted().value());
}

IN_PROC_BROWSER_TEST_F(WebAppDetailedInstallDialogBrowserTest,
                       InvokeUi_max_ratio_screenshot) {
  ShowAndVerifyUi();
  ASSERT_FALSE(dialog_accepted().value());
}

IN_PROC_BROWSER_TEST_F(WebAppDetailedInstallDialogBrowserTest,
                       InvokeUi_widget_destroyed_on_navigation) {
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppDetailedInstallDialog");
  ShowUi("destroyed_on_navigation");
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();

  views::test::WidgetDestroyedWaiter destroy_waiter(widget);
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), GURL(url::kAboutBlankURL), /*number_of_navigations=*/1);

  destroy_waiter.Wait();
  ASSERT_FALSE(dialog_accepted().value());
}

IN_PROC_BROWSER_TEST_F(WebAppDetailedInstallDialogBrowserTest,
                       InvokeUi_widget_destroyed_on_web_contents_invisible) {
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppDetailedInstallDialog");
  ShowUi("destroyed_on_web_contents_invisible");
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();

  views::test::WidgetDestroyedWaiter destroy_waiter(widget);
  // Navigate to a new tab.
  content::WebContents::CreateParams params(browser()->profile());
  browser()->tab_strip_model()->AppendWebContents(
      content::WebContents::Create(params), /*foreground=*/true);

  destroy_waiter.Wait();
  ASSERT_FALSE(dialog_accepted().value());
}

IN_PROC_BROWSER_TEST_F(WebAppDetailedInstallDialogBrowserTest,
                       InvokeUi_widget_destroyed_on_web_contents_destroyed) {
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppDetailedInstallDialog");
  ShowUi("destroyed_on_web_contents_destroyed");
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();

  views::test::WidgetDestroyedWaiter destroy_waiter(widget);
  content::WebContents::CreateParams params(browser()->profile());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  web_contents->Close();

  destroy_waiter.Wait();
  ASSERT_FALSE(dialog_accepted().value());
}

IN_PROC_BROWSER_TEST_F(WebAppDetailedInstallDialogBrowserTest,
                       InvokeUi_accept_dialog) {
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppDetailedInstallDialog");
  ShowUi("accept_dialog");
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();

  views::test::WidgetDestroyedWaiter destroy_waiter(widget);
  views::test::AcceptDialog(widget);
  destroy_waiter.Wait();
  ASSERT_TRUE(dialog_accepted().value());
}

IN_PROC_BROWSER_TEST_F(WebAppDetailedInstallDialogBrowserTest,
                       InvokeUi_cancel_dialog) {
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppDetailedInstallDialog");
  ShowUi("cancel_dialog");
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();

  views::test::WidgetDestroyedWaiter destroy_waiter(widget);
  views::test::CancelDialog(widget);
  destroy_waiter.Wait();
  ASSERT_FALSE(dialog_accepted().value());
}

IN_PROC_BROWSER_TEST_F(WebAppDetailedInstallDialogBrowserTest,
                       WindowSizeLoweringClosesDialog) {
  auto popup_value =
      OpenPopupOfSize(browser()->tab_strip_model()->GetActiveWebContents(),
                      GURL("https://www.example.com"),
                      /*width=*/500, /*height=*/500);
  EXPECT_TRUE(popup_value.has_value());
  content::WebContents* popup_contents = popup_value.value();
  Browser* popup_browser = chrome::FindBrowserWithTab(popup_contents);

  std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker =
      GetMLInstallTracker(popup_browser);

  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppDetailedInstallDialog");
  base::test::TestFuture<bool, std::unique_ptr<WebAppInstallInfo>> test_future;
  SetScreenshotFetcher(std::make_unique<FakeScreenshotFetcher>(
      GetScreenshots(std::string()), base::flat_set<int>()));
  ShowWebAppDetailedInstallDialog(
      popup_browser->tab_strip_model()->GetActiveWebContents(),
      GetInstallInfo(), std::move(install_tracker), test_future.GetCallback(),
      screenshot_fetcher(), PwaInProductHelpState::kNotShown);

  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);
  EXPECT_FALSE(test_future.IsReady());

  base::HistogramTester histograms;
  views::test::WidgetDestroyedWaiter destroy_waiter(widget);
  // Make the size of the popup window to be too small for the dialog.
  ui_test_utils::SetAndWaitForBounds(*popup_browser, gfx::Rect(100, 100));
  destroy_waiter.Wait();

  ASSERT_TRUE(test_future.Wait());
  EXPECT_FALSE(test_future.Get<bool>());

  histograms.ExpectUniqueSample(
      "WebApp.InstallConfirmation.CloseReason",
      views::Widget::ClosedReason::kCloseButtonClicked, 1);
}

IN_PROC_BROWSER_TEST_F(WebAppDetailedInstallDialogBrowserTest,
                       SmallPopupClosesWindowAutomatically) {
  auto popup_value =
      OpenPopupOfSize(browser()->tab_strip_model()->GetActiveWebContents(),
                      GURL("https://www.example.com"));
  EXPECT_TRUE(popup_value.has_value());
  content::WebContents* popup_contents = popup_value.value();
  Browser* popup_browser = chrome::FindBrowserWithTab(popup_contents);

  std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker =
      GetMLInstallTracker(popup_browser);

  base::HistogramTester histograms;
  views::AnyWidgetObserver widget_observer(views::test::AnyWidgetTestPasskey{});

  base::RunLoop run_loop;
  widget_observer.set_closing_callback(
      base::BindLambdaForTesting([&](views::Widget* widget) {
        if (widget->GetName() == "WebAppDetailedInstallDialog") {
          run_loop.Quit();
        }
      }));
  SetScreenshotFetcher(std::make_unique<FakeScreenshotFetcher>(
      GetScreenshots(std::string()), base::flat_set<int>()));
  ShowWebAppDetailedInstallDialog(popup_contents, GetInstallInfo(),
                                  std::move(install_tracker), base::DoNothing(),
                                  screenshot_fetcher(),
                                  PwaInProductHelpState::kNotShown);
  run_loop.Run();

  histograms.ExpectUniqueSample(
      "WebApp.InstallConfirmation.CloseReason",
      views::Widget::ClosedReason::kCloseButtonClicked, 1);
}

IN_PROC_BROWSER_TEST_F(WebAppDetailedInstallDialogBrowserTest,
                       PendingScreenshots) {
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppDetailedInstallDialog");

  // Skip all screenshots from being loaded, so there should be 3 loading images
  // in the dialog.
  SetScreenshotFetcher(std::make_unique<FakeScreenshotFetcher>(
      GetScreenshots("multiple_screenshots"), base::flat_set<int>({0, 1, 2})));

  ShowUi(std::string());
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();
  ASSERT_NE(nullptr, widget);

  std::vector<CarouselState> expected_state({CarouselState::kLoading,
                                             CarouselState::kLoading,
                                             CarouselState::kLoading});
  EXPECT_EQ(expected_state, GetActualCarouselStateFromWidget(widget));
}

IN_PROC_BROWSER_TEST_F(WebAppDetailedInstallDialogBrowserTest,
                       PartialScreenshots) {
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppDetailedInstallDialog");

  // Only load 2 screenshots, set the middle index to not load.
  SetScreenshotFetcher(std::make_unique<FakeScreenshotFetcher>(
      GetScreenshots("multiple_screenshots"), base::flat_set<int>({1})));

  ShowUi(std::string());
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();
  ASSERT_NE(nullptr, widget);

  std::vector<CarouselState> expected_state({CarouselState::kScreenshot,
                                             CarouselState::kLoading,
                                             CarouselState::kScreenshot});
  EXPECT_EQ(expected_state, GetActualCarouselStateFromWidget(widget));
}

IN_PROC_BROWSER_TEST_F(WebAppDetailedInstallDialogBrowserTest,
                       PendingScreenshotsDelay) {
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppDetailedInstallDialog");

  // Only load 2 screenshots, set the last index to not load.
  SetScreenshotFetcher(std::make_unique<FakeScreenshotFetcher>(
      GetScreenshots("multiple_screenshots"), base::flat_set<int>({2})));

  ShowUi(std::string());
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();
  ASSERT_NE(nullptr, widget);

  // Verify that only the first 2 screenshots loaded in the dialog being shown.
  std::vector<CarouselState> first_expected_state({CarouselState::kScreenshot,
                                                   CarouselState::kScreenshot,
                                                   CarouselState::kLoading});
  EXPECT_EQ(first_expected_state, GetActualCarouselStateFromWidget(widget));

  // Load the screenshot after the first check, simulating a delay.
  screenshot_fetcher()->LoadDelayedScreenshots();

  // Verify the dialog finished loading all screenshots.
  std::vector<CarouselState> final_state({CarouselState::kScreenshot,
                                          CarouselState::kScreenshot,
                                          CarouselState::kScreenshot});
  EXPECT_EQ(final_state, GetActualCarouselStateFromWidget(widget));
}

class PictureInPictureDetailedInstallDialogOcclusionTest
    : public MixinBasedInProcessBrowserTest {
 protected:
  void ShowDialogUi() {
    FakeScreenshotFetcher fetcher(GetScreenshots(std::string()),
                                  base::flat_set<int>());
    ShowWebAppDetailedInstallDialog(
        browser()->tab_strip_model()->GetWebContentsAt(0), GetInstallInfo(),
        GetMLInstallTracker(browser()), base::DoNothing(), fetcher.GetWeakPtr(),
        PwaInProductHelpState::kNotShown);
  }
  DocumentPictureInPictureMixinTestBase picture_in_picture_test_base_{
      &mixin_host_};
};

IN_PROC_BROWSER_TEST_F(PictureInPictureDetailedInstallDialogOcclusionTest,
                       PipWindowCloses) {
  picture_in_picture_test_base_.NavigateToURLAndEnterPictureInPicture(
      browser());
  auto* pip_web_contents =
      picture_in_picture_test_base_.window_controller()->GetChildWebContents();
  ASSERT_NE(nullptr, pip_web_contents);
  picture_in_picture_test_base_.WaitForPageLoad(pip_web_contents);

  // Show dialog.
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppDetailedInstallDialog");
  ShowDialogUi();
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();
  EXPECT_NE(nullptr, widget);

  // Occlude dialog with picture in picture web contents, verify window is
  // closed but dialog stays open.
  PictureInPictureWindowManager::GetInstance()
      ->GetOcclusionTracker()
      ->SetWidgetOcclusionStateForTesting(widget, /*occluded=*/true);
  EXPECT_TRUE(picture_in_picture_test_base_.AwaitPipWindowClosedSuccessfully());
  EXPECT_NE(nullptr, widget);
  EXPECT_TRUE(widget->IsVisible());
}

}  // namespace

}  // namespace web_app
