// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/live_caption/views/caption_bubble.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/i18n/language_tag.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/live_caption/caption_bubble_context.h"
#include "components/live_caption/caption_bubble_session_observer.h"
#include "components/live_caption/live_caption_bubble_settings.h"
#include "components/live_caption/pref_names.h"
#include "components/live_caption/views/caption_bubble_model.h"
#include "components/live_caption/views/translation_view_wrapper.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/scrollbar/scroll_bar.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/widget/widget.h"

namespace captions {
namespace {

constexpr char kEnglishLanguage[] = "en-US";

// A CaptionBubbleContext that doesn't depend on a browser window.
class FakeCaptionBubbleContext : public CaptionBubbleContext {
 public:
  FakeCaptionBubbleContext() = default;
  ~FakeCaptionBubbleContext() override = default;

  // CaptionBubbleContext:
  void GetBounds(GetBoundsCallback callback) const override {
    std::move(callback).Run(gfx::Rect(0, 0, 800, 600));
  }
  const std::string GetSessionId() const override { return "session"; }
  void Activate() override {}
  bool IsActivatable() const override { return false; }
  bool ShouldAvoidOverlap() const override { return false; }
  std::unique_ptr<CaptionBubbleSessionObserver>
  GetCaptionBubbleSessionObserver() override {
    return nullptr;
  }
  OpenCaptionSettingsCallback GetOpenCaptionSettingsCallback() override {
    return base::DoNothing();
  }
};

class CaptionBubbleTest : public views::ViewsTestBase {
 protected:
  CaptionBubbleTest() {
    scoped_feature_list_.InitWithFeatures({kLiveCaptionScrollable}, {});
    // The caption bubble is a top-level widget with no parent.
    set_native_widget_type(NativeWidgetType::kDesktop);
  }

  void SetUp() override {
    views::ViewsTestBase::SetUp();
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kLiveCaptionBubbleExpanded, false);
    pref_service_.registry()->RegisterBooleanPref(prefs::kLiveTranslateEnabled,
                                                  false);
    pref_service_.registry()->RegisterBooleanPref(prefs::kLiveCaptionEnabled,
                                                  false);
    pref_service_.registry()->RegisterStringPref(
        prefs::kLiveCaptionLanguageCode, kEnglishLanguage);
    pref_service_.registry()->RegisterLanguageTagPref(
        prefs::kLiveTranslateTargetLanguageCode,
        base::i18n::GetKnownLanguageTag("en"));

    model_ = std::make_unique<CaptionBubbleModel>(&context_, base::DoNothing());
    settings_ = std::make_unique<LiveCaptionBubbleSettings>(&pref_service_);
    settings_->SetLiveCaptionBubbleExpanded(true);

    auto bubble = std::make_unique<CaptionBubble>(
        settings_.get(),
        std::make_unique<TranslationViewWrapper>(settings_.get()),
        /*application_locale=*/std::string(),
        /*destroyed_callback=*/base::DoNothing());
    bubble_ = bubble.get();
#if BUILDFLAG(IS_CHROMEOS)
    // The caption bubble has no parent, so give its widget a context.
    test_views_delegate()->set_context(GetContext());
#endif
    widget_ = views::BubbleDialogDelegateView::CreateBubble(std::move(bubble));
    widget_->Show();
    bubble_->SetModel(model_.get());
  }

  void TearDown() override {
    bubble_->SetModel(nullptr);
    // Clear the pointers before the widget deletes itself and the bubble.
    views::Widget* widget = widget_;
    bubble_ = nullptr;
    widget_ = nullptr;
    widget->CloseNow();
    model_.reset();
    settings_.reset();
    views::ViewsTestBase::TearDown();
  }

  void AddLine(const std::string& text) {
    model_->SetPartialText(text);
    model_->CommitPartialText();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  TestingPrefServiceSimple pref_service_;
  FakeCaptionBubbleContext context_;
  std::unique_ptr<CaptionBubbleModel> model_;
  std::unique_ptr<LiveCaptionBubbleSettings> settings_;
  raw_ptr<CaptionBubble> bubble_ = nullptr;
  raw_ptr<views::Widget> widget_ = nullptr;
};

// Adding text must not move the bubble.
TEST_F(CaptionBubbleTest, NoWiggleOnResize) {
  AddLine("The first line of text.");
  views::test::RunScheduledLayout(widget_);
  gfx::Point initial_origin = widget_->GetWindowBoundsInScreen().origin();

  AddLine("A new line of text.");
  views::test::RunScheduledLayout(widget_);

  gfx::Point new_origin = widget_->GetWindowBoundsInScreen().origin();
  EXPECT_EQ(initial_origin, new_origin);
}

// With scrolling enabled, a long transcript shows a vertical scroll bar that
// can scroll to the start and back to the end.
TEST_F(CaptionBubbleTest, ScrollsLongText) {
  views::ScrollBar* scroll_bar =
      bubble_->GetScrollViewForTesting()->vertical_scroll_bar();
  ASSERT_TRUE(scroll_bar);
  EXPECT_EQ(scroll_bar->GetOrientation(),
            views::ScrollBar::Orientation::kVertical);

  for (int i = 0; i < 64; ++i) {
    AddLine(base::StringPrintf("ABCDEF %d ", i));
  }
  views::test::RunScheduledLayout(widget_);

  EXPECT_TRUE(scroll_bar->GetVisible());
  EXPECT_TRUE(
      scroll_bar->ScrollByAmount(views::ScrollBar::ScrollAmount::kStart));
  EXPECT_TRUE(scroll_bar->ScrollByAmount(views::ScrollBar::ScrollAmount::kEnd));
}

}  // namespace
}  // namespace captions
