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
#include "components/live_caption/views/format_constants.h"
#include "components/live_caption/views/translation_view_wrapper.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/compositor/canvas_painter.h"
#include "ui/compositor/layer.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/scrollbar/scroll_bar.h"
#include "ui/views/paint_info.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

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
  CaptionBubbleTest()
      : views::ViewsTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
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
// auto-scrolls at the bottom, pauses when scrolled up (showing a laid-out Jump
// to bottom button), and resumes auto-scrolling when returning to the bottom.
TEST_F(CaptionBubbleTest, ScrollsLongText) {
  views::ScrollView* scroll_view = bubble_->GetScrollViewForTesting();
  ASSERT_TRUE(scroll_view);
  if (scroll_view->contents()->layer()) {
    EXPECT_TRUE(
        scroll_view->contents()->layer()->main_side_scrolling_enabled());
  }
  views::ScrollBar* scroll_bar = scroll_view->vertical_scroll_bar();
  ASSERT_TRUE(scroll_bar);
  EXPECT_EQ(scroll_bar->GetOrientation(),
            views::ScrollBar::Orientation::kVertical);
  views::MdTextButton* jump_to_bottom =
      bubble_->GetJumpToBottomButtonForTesting();
  ASSERT_TRUE(jump_to_bottom);

  for (int i = 0; i < 64; ++i) {
    AddLine(base::StringPrintf("ABCDEF %d ", i));
  }
  if (widget_->GetRootView()->size() !=
      widget_->GetWindowBoundsInScreen().size()) {
    widget_->OnNativeWidgetSizeChanged(
        widget_->GetWindowBoundsInScreen().size());
  }
  views::test::RunScheduledLayout(widget_);

  EXPECT_TRUE(scroll_bar->GetVisible());
  EXPECT_EQ(scroll_bar->GetMaxPosition(), scroll_view->CurrentOffset().y());
  EXPECT_FALSE(jump_to_bottom->GetVisible());

  // Touchpad scroll up pauses auto-scrolling and lays out Jump to bottom with
  // non-empty bounds immediately.
  gfx::Point scroll_point = scroll_view->GetLocalBounds().CenterPoint();
  views::View::ConvertPointToTarget(scroll_view, widget_->GetRootView(),
                                    &scroll_point);
  const gfx::PointF scroll_point_f(scroll_point);
  ui::ScrollEvent fling_cancel(ui::EventType::kScrollFlingCancel,
                               scroll_point_f, scroll_point_f,
                               ui::EventTimeForNow(), ui::EF_NONE, 0, 0, 0, 0,
                               /*finger_count=*/2);
  widget_->OnScrollEvent(&fling_cancel);
  ui::ScrollEvent scroll_up(ui::EventType::kScroll, scroll_point_f,
                            scroll_point_f, ui::EventTimeForNow(), ui::EF_NONE,
                            0, 24, 0, 24, /*finger_count=*/2);
  widget_->OnScrollEvent(&scroll_up);
  views::test::RunScheduledLayout(widget_);
  EXPECT_TRUE(scroll_view->is_scrolling());
  EXPECT_LT(scroll_view->CurrentOffset().y(), scroll_bar->GetMaxPosition());
  EXPECT_TRUE(jump_to_bottom->GetVisible());
  EXPECT_FALSE(jump_to_bottom->bounds().IsEmpty());
  EXPECT_EQ(gfx::Size(kMaxWidthDip, kNumLinesExpanded * kLineHeightDip +
                                        2 * bubble_->GetCloseButtonForTesting()
                                                ->GetPreferredSize({})
                                                .height()),
            bubble_->GetPreferredSize({}));
  EXPECT_EQ(bubble_->GetHeaderForTesting()->GetBoundsInScreen().bottom(),
            scroll_view->GetBoundsInScreen().y());
  EXPECT_EQ(scroll_view->GetBoundsInScreen().bottom(),
            jump_to_bottom->GetBoundsInScreen().y());
  EXPECT_LE(jump_to_bottom->GetBoundsInScreen().bottom(),
            bubble_->GetBoundsInScreen().bottom());

  // Clicking Jump to bottom scrolls to the end and resumes auto-scrolling as
  // new words arrive.
  views::test::ButtonTestApi(jump_to_bottom)
      .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                  gfx::Point(), ui::EventTimeForNow(),
                                  ui::EF_LEFT_MOUSE_BUTTON,
                                  ui::EF_LEFT_MOUSE_BUTTON));
  views::test::RunScheduledLayout(widget_);
  EXPECT_EQ(scroll_bar->GetMaxPosition(), scroll_view->CurrentOffset().y());
  EXPECT_FALSE(jump_to_bottom->GetVisible());

  for (int i = 64; i < 80; ++i) {
    AddLine(base::StringPrintf("ABCDEF %d ", i));
  }
  views::test::RunScheduledLayout(widget_);
  EXPECT_EQ(scroll_bar->GetMaxPosition(), scroll_view->CurrentOffset().y());
  EXPECT_FALSE(jump_to_bottom->GetVisible());

  EXPECT_TRUE(
      scroll_bar->ScrollByAmount(views::ScrollBar::ScrollAmount::kStart));
  EXPECT_TRUE(jump_to_bottom->GetVisible());
  EXPECT_TRUE(scroll_bar->ScrollByAmount(views::ScrollBar::ScrollAmount::kEnd));
  EXPECT_FALSE(jump_to_bottom->GetVisible());

  // Verify rendered trailing fade gradient pixels in CaptionBubbleLabel and
  // scrollbar thumb contrast over the bubble background.
  views::Label* label = bubble_->GetLabelForTesting();
  ASSERT_TRUE(label);
  const int line_height = label->GetLineHeight();
  auto get_line_max_alpha = [&](int y_start, int y_end) {
    SkBitmap bitmap;
    {
      ui::CanvasPainter painter(&bitmap, label->size(), 1.0f,
                                SK_ColorTRANSPARENT,
                                /*is_pixel_canvas=*/true);
      label->Paint(views::PaintInfo::CreateRootPaintInfo(painter.context(),
                                                         label->size()));
    }
    uint32_t max_alpha = 0;
    for (int y = std::max(0, y_start); y < std::min(bitmap.height(), y_end);
         ++y) {
      for (int x = 0; x < bitmap.width(); ++x) {
        max_alpha = std::max(max_alpha, SkColorGetA(bitmap.getColor(x, y)));
      }
    }
    return max_alpha;
  };

  // At bottom (unpaused): bottom visible line is opaque.
  EXPECT_EQ(nullptr, scroll_view->layer());
  gfx::Rect vis_rect = scroll_view->GetVisibleRect();
  EXPECT_GE(
      get_line_max_alpha(vis_rect.bottom() - line_height, vis_rect.bottom()),
      250u);

  // Scrolled to top (paused): top 6.5 visible lines remain opaque while the
  // bottom 1.5 lines (lower half of line 7 and line 8) fade out smoothly to the
  // viewport bottom without slicing off line 8's baseline/descenders.
  EXPECT_TRUE(
      scroll_bar->ScrollByAmount(views::ScrollBar::ScrollAmount::kStart));
  views::test::RunScheduledLayout(widget_);
  vis_rect = scroll_view->GetVisibleRect();
  EXPECT_GE(get_line_max_alpha(vis_rect.bottom() - 3 * line_height,
                               vis_rect.bottom() - 2 * line_height),
            250u);
  EXPECT_GE(
      get_line_max_alpha(vis_rect.bottom() - 2 * line_height,
                         vis_rect.bottom() - line_height - line_height / 2),
      250u);
  const uint32_t line_7_lower_alpha =
      get_line_max_alpha(vis_rect.bottom() - line_height - line_height / 3,
                         vis_rect.bottom() - line_height);
  const uint32_t line_8_alpha =
      get_line_max_alpha(vis_rect.bottom() - line_height, vis_rect.bottom());
  const uint32_t line_8_bottom_alpha = get_line_max_alpha(
      vis_rect.bottom() - line_height / 3, vis_rect.bottom());
  EXPECT_LT(line_7_lower_alpha, 250u);
  EXPECT_GT(line_7_lower_alpha, 160u);
  EXPECT_LT(line_8_alpha, line_7_lower_alpha);
  EXPECT_GT(line_8_alpha, 64u);
  EXPECT_GT(line_8_bottom_alpha, 0u);
  EXPECT_LT(line_8_bottom_alpha, 96u);

  // Scrolled to a non-zero offset: verify the fade region tracks the scroll
  // viewport (`GetVisibleRect()`) rather than fixed label-top coordinates.
  scroll_view->ScrollToOffset(gfx::PointF(0, line_height / 2));
  views::test::RunScheduledLayout(widget_);
  vis_rect = scroll_view->GetVisibleRect();
  EXPECT_GE(get_line_max_alpha(vis_rect.bottom() - 3 * line_height,
                               vis_rect.bottom() - 2 * line_height),
            250u);
  EXPECT_LT(get_line_max_alpha(vis_rect.bottom() - line_height / 4,
                               vis_rect.bottom()),
            64u);

  // Scrollbar thumb renders with subtle grey contrast (clearly visible against
  // the dark bubble background and not overly bright), enforces minimum thumb
  // dimensions for long transcripts, and respects custom CaptionStyle
  // text_color.
  for (int i = 80; i < 360; ++i) {
    AddLine(base::StringPrintf("ABCDEF %d ", i));
  }
  views::test::RunScheduledLayout(widget_);
  ASSERT_FALSE(scroll_bar->children().empty());
  views::View* thumb = scroll_bar->children()[0];
  EXPECT_EQ(kScrollbarThumbThicknessDip, scroll_bar->GetThickness());
  EXPECT_EQ(kScrollbarThumbThicknessDip, thumb->width());
  EXPECT_EQ(kScrollbarThumbMinLengthDip, thumb->GetPreferredSize({}).height());
  EXPECT_EQ(kScrollbarThumbMinLengthDip, thumb->height());
  const SkColor bg_color = SkColorSetA(
      bubble_->background_color().ResolveToSkColor(bubble_->GetColorProvider()),
      SK_AlphaOPAQUE);
  auto get_thumb_color = [&]() {
    SkBitmap sb_bitmap;
    {
      ui::CanvasPainter painter(&sb_bitmap, scroll_bar->size(), 1.0f, bg_color,
                                /*is_pixel_canvas=*/true);
      scroll_bar->Paint(views::PaintInfo::CreateRootPaintInfo(
          painter.context(), scroll_bar->size()));
    }
    const gfx::Point thumb_center = thumb->bounds().CenterPoint();
    return sb_bitmap.getColor(thumb_center.x(), thumb_center.y());
  };
  const SkColor thumb_color = get_thumb_color();
  const int red_delta = static_cast<int>(SkColorGetR(thumb_color)) -
                        static_cast<int>(SkColorGetR(bg_color));
  EXPECT_GT(red_delta, 30);
  EXPECT_LT(red_delta, 50);

  ui::CaptionStyle caption_style;
  caption_style.text_color = "rgba(0, 255, 0, 1)";
  bubble_->UpdateCaptionStyle(caption_style);
  const SkColor styled_thumb_color = get_thumb_color();
  EXPECT_GT(SkColorGetG(styled_thumb_color), SkColorGetR(styled_thumb_color));

  // Entering the bubble from the right side into the compact frame margin
  // immediately adjacent to the scrollbar (inside BubbleFrameView's contents
  // bounds, outside CaptionBubble's inset local bounds) must reveal the
  // scrollbar when at the bottom, and moving back outside must hide it.
  EXPECT_TRUE(scroll_bar->ScrollByAmount(views::ScrollBar::ScrollAmount::kEnd));
  views::test::RunScheduledLayout(widget_);
  EXPECT_FALSE(jump_to_bottom->GetVisible());

  views::BubbleFrameView* frame_view = bubble_->GetBubbleFrameView();
  ASSERT_TRUE(frame_view);
  gfx::Rect frame_contents_in_screen = frame_view->GetContentsBounds();
  views::View::ConvertRectToScreen(frame_view, &frame_contents_in_screen);
  const gfx::Rect bubble_bounds_in_screen = bubble_->GetBoundsInScreen();
  const gfx::Point right_margin_point(
      frame_contents_in_screen.right() - 2,
      frame_contents_in_screen.CenterPoint().y());
  ASSERT_TRUE(frame_contents_in_screen.Contains(right_margin_point));
  ASSERT_FALSE(bubble_bounds_in_screen.Contains(right_margin_point));

#if !BUILDFLAG(IS_FUCHSIA)
  // FlatlandWindow::SetBoundsInDIP does not update Aura Window bounds in unit
  // tests, so EventGenerator cannot hit-test widget_->GetNativeWindow() on
  // Fuchsia.
  const gfx::Point outside_right_point(
      frame_contents_in_screen.right() + 20,
      frame_contents_in_screen.CenterPoint().y());
  ui::test::EventGenerator generator(views::GetRootWindow(widget_),
                                     widget_->GetNativeWindow());
  generator.set_target(ui::test::EventGenerator::Target::APPLICATION);
  task_environment()->FastForwardBy(base::Milliseconds(200));
  bubble_->UpdateControlsVisibility(false);
  task_environment()->FastForwardBy(base::Milliseconds(200));
  EXPECT_EQ(0.0f, scroll_bar->layer()->opacity());

  generator.MoveMouseTo(right_margin_point);
  task_environment()->FastForwardBy(base::Milliseconds(200));
  EXPECT_EQ(1.0f, scroll_bar->layer()->opacity());

  generator.MoveMouseTo(outside_right_point);
  task_environment()->FastForwardBy(base::Milliseconds(200));
  EXPECT_EQ(0.0f, scroll_bar->layer()->opacity());

  generator.set_current_screen_location(right_margin_point);
  generator.SendMouseEnter();
  task_environment()->FastForwardBy(base::Milliseconds(200));
  EXPECT_EQ(1.0f, scroll_bar->layer()->opacity());
#endif
}

}  // namespace
}  // namespace captions
