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
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/shortcuts/shortcut_icon_generator.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/web_apps/progress_delay.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_dialog_delegate.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_flow_dialog_delegate.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_options_view.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_screenshot_fetcher.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "components/webapps/browser/installable/ml_install_operation_tracker.h"
#include "components/webapps/browser/installable/ml_installability_promoter.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/ax_update_notifier.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/test/mock_activation_controller.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/dialog_delegate.h"
#include "url/gurl.h"

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

  void SetUp() override {
#if defined(USE_MOCK_ACTIVATION_CONTROLLER)
    activation_controller_ =
        std::make_unique<views::test::MockActivationController>();
#endif
    DialogBrowserTest::SetUp();
  }

  void TearDownOnMainThread() override {
#if defined(USE_MOCK_ACTIVATION_CONTROLLER)
    activation_controller_.reset();
#endif
    DialogBrowserTest::TearDownOnMainThread();
  }

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
        browser()->GetTabStripModel()->GetActiveWebContents());
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
        browser()->GetTabStripModel()->GetActiveWebContents(),
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
    tab_count_at_dialog_completion_ = browser()->GetTabStripModel()->count();
  }

  void CompleteInstall(bool success) {
    std::move(install_result_callback_).Run(success, base::DoNothing());
  }

  // Presses the "Learn more" button of the currently shown dialog and verifies
  // that the installation was declined before the help page was opened in a
  // new foreground tab, and that the dialog was closed.
  void PressLearnMoreAndExpectDeclineBeforeHelpPage() {
    ASSERT_TRUE(widget_);
    ASSERT_EQ(browser()->GetTabStripModel()->count(), 1);
    views::test::WidgetDestroyedWaiter destruction_waiter(widget_.get());

    views::View* learn_more_view =
        views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
            WebAppInstallFlowDialogDelegate::kLearnMoreButtonId,
            views::ElementTrackerViews::GetContextForWidget(widget_.get()));
    ASSERT_TRUE(learn_more_view);

    ui_test_utils::TabAddedWaiter tab_added_waiter(browser());
    // Key events are not subject to the dialog's input event activation
    // protection, unlike mouse events dispatched right after the dialog is
    // shown.
    ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_RETURN,
                           ui::EF_NONE);
    views::test::ButtonTestApi(
        views::AsViewClass<views::Button>(learn_more_view))
        .NotifyClick(key_event);

    // The installation is declined synchronously, while the installing tab
    // was still the only tab, i.e. before the help page was opened.
    EXPECT_EQ(dialog_accepted_, false);
    EXPECT_EQ(tab_count_at_dialog_completion_, 1);

    content::WebContents* help_page = tab_added_waiter.Wait();
    ASSERT_TRUE(help_page);
    EXPECT_EQ(help_page->GetVisibleURL(),
              GURL(chrome::kInstallDialogFlowLearnMoreURL));
    EXPECT_EQ(help_page, browser()->GetTabStripModel()->GetActiveWebContents());
    destruction_waiter.Wait();
  }

 protected:
  InstallOsType GetOsType() { return std::get<0>(GetParam()); }
  InstallDialogType GetDialogType() { return std::get<1>(GetParam()); }
  std::optional<bool> dialog_accepted_;
  std::unique_ptr<WebAppInstallInfo> dialog_install_info_;
  WebAppInstallationAcceptanceResultCallback install_result_callback_;
  // The number of tabs in `browser()` at the time `OnDialogCompleted()` ran.
  std::optional<int> tab_count_at_dialog_completion_;
  TestWebAppScreenshotFetcher screenshot_fetcher_;
  base::WeakPtr<views::Widget> widget_ = nullptr;
  base::WeakPtr<WebAppInstallFlowDialogDelegate> delegate_ = nullptr;

 private:
  base::test::ScopedFeatureList feature_list_;
#if defined(USE_MOCK_ACTIVATION_CONTROLLER)
  std::unique_ptr<views::test::MockActivationController> activation_controller_;
#endif
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
  base::HistogramTester histogram_tester;
  ShowUi("Intro");
  ASSERT_TRUE(widget_);
  views::test::WidgetDestroyedWaiter destruction_waiter(widget_.get());
  widget_->widget_delegate()->AsDialogDelegate()->CancelDialog();
  destruction_waiter.Wait();
  EXPECT_EQ(dialog_accepted_, false);
  histogram_tester.ExpectUniqueSample(
      "WebApp.InstallConfirmation.CloseReason",
      views::Widget::ClosedReason::kCancelButtonClicked, 1);
  histogram_tester.ExpectTotalCount("WebApp.InstallFlow.SuccessStepAction", 0);
}

IN_PROC_BROWSER_TEST_P(WebAppInstallDialogClosedTest, CancelInSuccess) {
  base::HistogramTester histogram_tester;
  ShowUi("Successful");
  ASSERT_TRUE(widget_);
  views::test::WidgetDestroyedWaiter destruction_waiter(widget_.get());
  widget_->widget_delegate()->AsDialogDelegate()->CancelDialog();
  destruction_waiter.Wait();
  EXPECT_EQ(dialog_accepted_, true);
  histogram_tester.ExpectUniqueSample(
      "WebApp.InstallConfirmation.CloseReason",
      views::Widget::ClosedReason::kAcceptButtonClicked, 1);
  histogram_tester.ExpectUniqueSample(
      "WebApp.InstallFlow.SuccessStepAction",
      GetOsType() == InstallOsType::kMac
          ? InstallFlowSuccessStepAction::kShowInFinderClicked
          : InstallFlowSuccessStepAction::kCloseClicked,
      1);
}

IN_PROC_BROWSER_TEST_P(WebAppInstallDialogClosedTest, Success) {
  base::HistogramTester histogram_tester;
  // Accept through all the dialog steps
  // kInstallDialog -> kInstallerOptions -> kProgress -> kSuccessful
  ShowUi("Successful");
  ASSERT_TRUE(widget_);
  views::test::WidgetDestroyedWaiter destruction_waiter(widget_.get());
  widget_->widget_delegate()->AsDialogDelegate()->AcceptDialog();
  destruction_waiter.Wait();
  EXPECT_EQ(dialog_accepted_, true);
  EXPECT_TRUE(dialog_install_info_);
  histogram_tester.ExpectUniqueSample(
      "WebApp.InstallConfirmation.CloseReason",
      views::Widget::ClosedReason::kAcceptButtonClicked, 1);
  histogram_tester.ExpectUniqueSample(
      "WebApp.InstallFlow.SuccessStepAction",
      InstallFlowSuccessStepAction::kOpenAppClicked, 1);
}

IN_PROC_BROWSER_TEST_P(WebAppInstallDialogClosedTest,
                       CloseInProgressRecordsAcceptCloseReasonOnlyOnce) {
  base::HistogramTester histogram_tester;
  ShowUi("Progress");
  ASSERT_TRUE(widget_);
  EXPECT_EQ(dialog_accepted_, true);
  auto* promoter = webapps::MLInstallabilityPromoter::FromWebContents(
      browser()->GetTabStripModel()->GetActiveWebContents());
  ASSERT_TRUE(promoter);
  EXPECT_TRUE(promoter->HasCurrentInstall());
  histogram_tester.ExpectUniqueSample(
      "WebApp.InstallConfirmation.CloseReason",
      views::Widget::ClosedReason::kAcceptButtonClicked, 1);

  views::test::WidgetDestroyedWaiter destruction_waiter(widget_.get());
  widget_->Close();
  destruction_waiter.Wait();

  EXPECT_FALSE(promoter->HasCurrentInstall());
  histogram_tester.ExpectUniqueSample(
      "WebApp.InstallConfirmation.CloseReason",
      views::Widget::ClosedReason::kAcceptButtonClicked, 1);
  histogram_tester.ExpectTotalCount("WebApp.InstallFlow.SuccessStepAction", 0);
}

// Regression tests for crbug.com/570729142. "Learn more" opens the help page in
// a new foreground tab, which hides the installing tab and closes this
// tab-modal dialog. The dialog must decline the installation *before* that tab
// is opened, otherwise the pending install command aborts itself on the tab
// switch and the decline never reaches it.
IN_PROC_BROWSER_TEST_P(WebAppInstallDialogClosedTest, LearnMoreInIntro) {
  ShowUi("Intro");
  PressLearnMoreAndExpectDeclineBeforeHelpPage();
}

IN_PROC_BROWSER_TEST_P(WebAppInstallDialogClosedTest,
                       LearnMoreInInstallOptions) {
  if (GetOsType() == InstallOsType::kOther) {
    GTEST_SKIP() << "InstallOptions step does not exist for kOther";
  }
  ShowUi("InstallOptions");
  PressLearnMoreAndExpectDeclineBeforeHelpPage();
}

IN_PROC_BROWSER_TEST_P(WebAppInstallDialogClosedTest,
                       FocusAndAccessibilityAcrossSteps) {
  ShowUi("Intro");
  ASSERT_TRUE(widget_);
  ASSERT_TRUE(delegate_);
  auto* dialog_delegate = widget_->widget_delegate()->AsDialogDelegate();
  ASSERT_TRUE(dialog_delegate);
  views::FocusManager* focus_manager = widget_->GetFocusManager();
  ASSERT_TRUE(focus_manager);

  EXPECT_EQ(delegate_->GetCurrentStepForTesting(),
            InstallDialogStep::kInstallDialog);
  if (GetDialogType() == InstallDialogType::kDiy) {
    EXPECT_TRUE(
        views::IsViewClass<views::Textfield>(focus_manager->GetFocusedView()));
  } else {
    EXPECT_EQ(focus_manager->GetFocusedView(),
              dialog_delegate->GetCancelButton());
  }

  EXPECT_EQ(
      dialog_delegate->GetOkButton()->GetViewAccessibility().GetCachedName(),
      l10n_util::GetStringUTF16(GetOsType() == InstallOsType::kOther
                                    ? IDS_INSTALL
                                    : IDS_WEB_APP_INSTALL_FLOW_NEXT));

  views::test::AXEventCounter ax_counter(views::AXUpdateNotifier::Get());

  // For OS types that show the installer options step, accepting the intro step
  // transitions to `kInstallerOptions`, politely announces the step title,
  // relabels the primary button to "Install", and focuses the Cancel button.
  if (GetOsType() != InstallOsType::kOther) {
    dialog_delegate->AcceptDialog();
    EXPECT_EQ(delegate_->GetCurrentStepForTesting(),
              InstallDialogStep::kInstallerOptions);
    EXPECT_EQ(focus_manager->GetFocusedView(),
              dialog_delegate->GetCancelButton());
    EXPECT_EQ(
        dialog_delegate->GetOkButton()->GetViewAccessibility().GetCachedName(),
        l10n_util::GetStringUTF16(IDS_INSTALL));
    EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kFocus,
                                  dialog_delegate->GetCancelButton()),
              1);
    EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kFocus), 1);
#if !BUILDFLAG(IS_MAC)
    EXPECT_GE(ax_counter.GetCount(ax::mojom::Event::kLiveRegionChanged), 1);
#endif

    ax_counter.ResetAllCounts();
  }

  // Accepting the dialog now transitions to `kProgress`. Because all dialog
  // buttons are hidden during installation progress, focus inside the dialog is
  // cleared (no `kFocus` event is fired) and the "Installing app..." status is
  // announced politely via a live region.
  dialog_delegate->AcceptDialog();
  EXPECT_EQ(delegate_->GetCurrentStepForTesting(),
            InstallDialogStep::kProgress);
  EXPECT_EQ(focus_manager->GetFocusedView(), nullptr);
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kFocus), 0);
  // On macOS, `RootView::AnnounceTextAs` posts a native NSAccessibility
  // announcement notification directly instead of updating the polite live
  // region view, so `kLiveRegionChanged` is only emitted on non-Mac platforms.
#if !BUILDFLAG(IS_MAC)
  EXPECT_GE(ax_counter.GetCount(ax::mojom::Event::kLiveRegionChanged), 1);
#endif

  // Once installation finishes and the progress animation completes, the dialog
  // transitions to `kSuccessful`, politely announces the success title, and
  // focuses the primary ("Open tab in app") button.
  ax_counter.ResetAllCounts();
  CompleteInstall(true);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return delegate_ && delegate_->GetCurrentStepForTesting() ==
                            InstallDialogStep::kSuccessful;
  }));

  EXPECT_EQ(focus_manager->GetFocusedView(), dialog_delegate->GetOkButton());
  EXPECT_EQ(
      dialog_delegate->GetOkButton()->GetViewAccessibility().GetCachedName(),
      l10n_util::GetStringUTF16(IDS_WEB_APP_INSTALL_SUCCESS_OPEN_APP));
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kFocus,
                                dialog_delegate->GetOkButton()),
            1);
  EXPECT_EQ(ax_counter.GetCount(ax::mojom::Event::kFocus), 1);
#if !BUILDFLAG(IS_MAC)
  EXPECT_GE(ax_counter.GetCount(ax::mojom::Event::kLiveRegionChanged), 1);
#endif
}

IN_PROC_BROWSER_TEST_P(WebAppInstallDialogClosedTest,
                       InactiveBrowserStaysInactiveOnSuccess) {
  ShowUi("Progress");
  ASSERT_TRUE(widget_);
  ASSERT_TRUE(delegate_);
  EXPECT_EQ(delegate_->GetCurrentStepForTesting(),
            InstallDialogStep::kProgress);

  views::Widget* top_level_widget = widget_->GetTopLevelWidget();
  ASSERT_TRUE(top_level_widget);
  views::FocusManager* focus_manager = widget_->GetFocusManager();
  ASSERT_TRUE(focus_manager);
  auto* dialog_delegate = widget_->widget_delegate()->AsDialogDelegate();
  ASSERT_TRUE(dialog_delegate);

  // Show another top-level widget to deactivate `top_level_widget`
  // (`Widget::Deactivate()` is not implemented on macOS).
  views::Widget other_widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
#if BUILDFLAG(IS_CHROMEOS)
  params.context = top_level_widget->GetNativeWindow();
#endif
  other_widget.Init(std::move(params));
  other_widget.Show();
  ASSERT_FALSE(top_level_widget->IsActive());

  CompleteInstall(true);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return delegate_ && delegate_->GetCurrentStepForTesting() ==
                            InstallDialogStep::kSuccessful;
  }));

  EXPECT_FALSE(top_level_widget->IsActive());
  EXPECT_EQ(focus_manager->GetFocusedView(), nullptr);
  EXPECT_EQ(focus_manager->GetStoredFocusView(),
            dialog_delegate->GetOkButton());
  other_widget.CloseNow();
}

INSTANTIATE_TEST_SUITE_P(
    /** prefix */,
    WebAppInstallDialogClosedTest,
    testing::Combine(testing::Values(InstallOsType::kOther,
                                     InstallOsType::kMac,
                                     InstallOsType::kCros,
                                     InstallOsType::kWin),
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
