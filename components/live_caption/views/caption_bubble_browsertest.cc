// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/live_caption/views/caption_bubble.h"

#include <memory>
#include <utility>

#include "base/cfi_buildflags.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
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
  CaptionBubbleBrowserTest() = default;

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
    model_->SetPartialText("ABCDEF");
    model_->CommitPartialText();
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

 private:
  views::Widget* GetWidgetForScreenshot() { return bubble_->GetWidget(); }

  TestingPrefServiceSimple pref_service_;

  std::unique_ptr<CaptionBubbleContextViews> context_;
  std::unique_ptr<CaptionBubbleModel> model_;
  std::unique_ptr<LiveCaptionBubbleSettings> settings_;
  raw_ptr<CaptionBubble> bubble_;
};

// Test that calls ShowUi("default").
IN_PROC_BROWSER_TEST_F(CaptionBubbleBrowserTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

}  // namespace
}  // namespace captions
