// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/live_caption/views/caption_bubble.h"

#include <memory>
#include <utility>

#include "base/cfi_buildflags.h"
#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "caption_bubble_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/confirm_bubble.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/accessibility/caption_bubble_context_views.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/live_caption/caption_bubble_context.h"
#include "components/live_caption/caption_bubble_controller.h"
#include "components/live_caption/caption_bubble_settings.h"
#include "components/live_caption/live_caption_bubble_settings.h"
#include "components/live_caption/pref_names.h"
#include "components/live_caption/views/translation_view_wrapper.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/test/ui_controls.h"

namespace captions {
namespace {

constexpr char kEnglishLanguage[] = "en-US";

class CaptionBubbleBrowserTest : public UiBrowserTest {
 protected:
  CaptionBubbleBrowserTest() {
    scoped_feature_list_.InitWithFeatures({captions::kLiveCaptionScrollable},
                                          {});
  }

  void SetUpOnMainThread() override {
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kLiveCaptionBubbleExpanded, false);
    pref_service_.registry()->RegisterBooleanPref(prefs::kLiveTranslateEnabled,
                                                  false);
    pref_service_.registry()->RegisterBooleanPref(prefs::kLiveCaptionEnabled,
                                                  false);
    pref_service_.registry()->RegisterStringPref(
        prefs::kLiveCaptionLanguageCode, kEnglishLanguage);
    pref_service_.registry()->RegisterStringPref(
        prefs::kLiveTranslateTargetLanguageCode, kEnglishLanguage);
    UiBrowserTest::SetUpOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII("--force-device-scale-factor", "1.01");
    UiBrowserTest::SetUpCommandLine(command_line);
  }

  void TearDownOnMainThread() override {
    context_.reset();
    UiBrowserTest::TearDownOnMainThread();
  }

  // UiBrowserTest:
  void ShowUi(const std::string& name) override {
    context_ = std::make_unique<CaptionBubbleContextViews>(
        browser()->GetActiveTabInterface()->GetContents());
    OnCaptionBubbleClosedCallback callback;
    model_ = std::make_unique<CaptionBubbleModel>(context_.get(),
                                                  std::move(callback));
    settings_ = std::make_unique<LiveCaptionBubbleSettings>(&pref_service_);
    settings_->SetLiveCaptionBubbleExpanded(true);

    const std::string application_locale;
    base::OnceClosure destroyed_callback;
    auto bubble = std::make_unique<CaptionBubble>(
        settings_.get(),
        std::make_unique<TranslationViewWrapper>(settings_.get()),
        application_locale, std::move(destroyed_callback));
    bubble_ = bubble.get();
    views::BubbleDialogDelegateView::CreateBubble(std::move(bubble))->Show();
    bubble_->SetModel(model_.get());

    // Prepare RunLoop,
    run_loop_ = std::make_unique<base::RunLoop>();
    // Increase to 1024u for a manual run to observe scrolling on screen.
    SingleStep(128u);
    // The test will wait until all steps are completed.
    run_loop_->Run();
  }

  // These next two are not necessary if subclassing DialogBrowserTest.
  bool VerifyUi() override {
    views::Widget* widget = GetWidgetForScreenshot();

    auto* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    const std::string screenshot_name =
        base::StrCat({test_info->test_suite_name(), "_", test_info->name()});

    return VerifyPixelUi(widget, "CaptureBubblePixelTest", screenshot_name) !=
           ui::test::ActionResult::kFailed;
  }

  void DismissUi() override {
    if (bubble_) {
      bubble_->SetModel(nullptr);
      bubble_ = nullptr;
    }
    IgnoreNetworkServiceCrashes();
  }

  void WaitForUserDismissal() override {
    /* Block until the UI has been dismissed. */
    ui_test_utils::WaitForBrowserToClose();
    if (bubble_) {
      bubble_->SetModel(nullptr);
      bubble_ = nullptr;
    }
    IgnoreNetworkServiceCrashes();
  }

 protected:
  // This method adds one more piece of text to the bubble and then
  // performs scroll to the start and scroll to the end (for testing only).
  // It schedules asynchronous call to itself for the next piece of text,
  // in order to yield the the UI thread and let scrolls repaint the view.
  void SingleStep(uint64_t i) {
    if (!bubble_) {
      return;
    }
    model_->SetPartialText(base::StringPrintf("ABCDEF %ul ", i));
    model_->CommitPartialText();
    auto* const scroll_bar =
        bubble_->GetScrollViewForTesting()->vertical_scroll_bar();
    ASSERT_EQ(scroll_bar->GetOrientation(),
              views::ScrollBar::Orientation::kVertical);

    // Next iteration needs to be reschedued on UI thread, so that views can
    // be repainted.
    auto next_step =
        (i > 1) ? base::BindPostTask(
                      base::SingleThreadTaskRunner::GetCurrentDefault(),
                      base::BindOnce(&CaptionBubbleBrowserTest::SingleStep,
                                     base::Unretained(this), i - 1))
                : base::BindOnce(&base::RunLoop::Quit,
                                 base::Unretained(run_loop_.get()));

    if (scroll_bar->GetVisible()) {
      // If the scrollbar is already visible, reschedule simulated scroll to the
      // beginning, then scroll to the end, and then do the next step.
      // The scrolled view should be repainted after ~every action.
      next_step =
          base::BindPostTask(base::SingleThreadTaskRunner::GetCurrentDefault(),
                             base::BindOnce(
                                 [](views::ScrollBar* scroll_bar) {
                                   ASSERT_TRUE(scroll_bar->ScrollByAmount(
                                       views::ScrollBar::ScrollAmount::kStart));
                                 },
                                 base::Unretained(scroll_bar)))
              .Then(base::BindPostTask(
                  base::SingleThreadTaskRunner::GetCurrentDefault(),
                  base::BindOnce(
                      [](views::ScrollBar* scroll_bar) {
                        ASSERT_TRUE(scroll_bar->ScrollByAmount(
                            views::ScrollBar::ScrollAmount::kEnd));
                      },
                      base::Unretained(scroll_bar))))
              .Then(std::move(next_step));
    }

    std::move(next_step).Run();
  }

  base::test::ScopedFeatureList scoped_feature_list_;

  TestingPrefServiceSimple pref_service_;

  std::unique_ptr<CaptionBubbleContextViews> context_;
  std::unique_ptr<CaptionBubbleModel> model_;
  std::unique_ptr<LiveCaptionBubbleSettings> settings_;
  raw_ptr<CaptionBubble> bubble_;

  std::unique_ptr<base::RunLoop> run_loop_;

 private:
  views::Widget* GetWidgetForScreenshot() const { return bubble_->GetWidget(); }
};

// Test that calls ShowUi("default").
// TODO(crbug.com/422524764): Flakily failing on Windows
#if BUILDFLAG(IS_WIN)
#define MAYBE_InvokeUi_default DISABLED_InvokeUi_default
#else
#define MAYBE_InvokeUi_default InvokeUi_default
#endif
IN_PROC_BROWSER_TEST_F(CaptionBubbleBrowserTest, MAYBE_InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleBrowserTest, InvokeUi_NoWiggleOnResize) {
  ShowUi("NoWiggleOnResize");
  views::Widget* widget = bubble_->GetWidget();
  gfx::Point initial_origin = widget->GetWindowBoundsInScreen().origin();

  model_->SetPartialText("A new line of text.");
  model_->CommitPartialText();

  base::RunLoop().RunUntilIdle();

  gfx::Point new_origin = widget->GetWindowBoundsInScreen().origin();
  EXPECT_EQ(initial_origin, new_origin);
  DismissUi();
}

}  // namespace
}  // namespace captions
