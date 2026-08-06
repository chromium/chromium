// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/dictation/dictation_overlay_view.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "chrome/browser/dictation/test_util.h"
#include "chrome/browser/ui/views/dictation/ui_state.h"
#include "chrome/browser/ui/views/dictation/waveform_view.h"
#include "chrome/browser/ui/views/dictation/waveform_view_button.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace dictation {

class DictationOverlayViewTest : public ChromeViewsTestBase {
 public:
  DictationOverlayViewTest() = default;
  DictationOverlayViewTest(const DictationOverlayViewTest&) = delete;
  DictationOverlayViewTest& operator=(const DictationOverlayViewTest&) = delete;
  ~DictationOverlayViewTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    parent_widget_ =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    parent_widget_->Show();
  }

  void TearDown() override {
    parent_widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  std::unique_ptr<views::Widget> parent_widget_;
  base::test::ScopedFeatureList scoped_feature_list_{
      CreateEnablingFeatureList()};
};

TEST_F(DictationOverlayViewTest, ShowAndReposition) {
  auto overlay = std::make_unique<DictationOverlayView>(
      parent_widget_->GetNativeView(), base::DoNothing());

  overlay->Show();
  views::Widget* widget = overlay->GetWidget();
  ASSERT_NE(widget, nullptr);
  EXPECT_TRUE(widget->IsVisible());

  gfx::Point selection_point(100, 200);
  overlay->UpdatePosition(selection_point);
  EXPECT_EQ(overlay->GetAnchorRect(), gfx::Rect(selection_point, gfx::Size()));
}

TEST_F(DictationOverlayViewTest, StateTransitionsUpdateSubviews) {
  auto overlay = std::make_unique<DictationOverlayView>(
      parent_widget_->GetNativeView(), base::DoNothing());

  overlay->Show();
  views::View* contents_view = overlay->GetContentsView();
  ASSERT_NE(contents_view, nullptr);

  auto* tracker = views::ElementTrackerViews::GetInstance();
  auto context = views::ElementTrackerViews::GetContextForView(contents_view);

  views::View* mic_button = tracker->GetFirstMatchingView(
      DictationOverlayView::kMicButtonElementIdForTesting, context);
  views::View* waveform_view = tracker->GetFirstMatchingView(
      DictationOverlayView::kWaveformElementIdForTesting, context);
  views::View* finalizing_image = tracker->GetFirstMatchingView(
      DictationOverlayView::kFinalizingImageElementIdForTesting, context);

  ASSERT_NE(mic_button, nullptr);
  ASSERT_NE(waveform_view, nullptr);
  ASSERT_NE(finalizing_image, nullptr);

  // Initial state (kInactive): mic_button visible, waveform and finalizing
  // hidden.
  EXPECT_EQ(overlay->state_for_testing(), UiState::kInactive);
  EXPECT_TRUE(mic_button->GetVisible());
  EXPECT_FALSE(waveform_view->GetVisible());
  EXPECT_FALSE(finalizing_image->GetVisible());

  // Transition to kInitializing: mic_button visible.
  overlay->SetState(UiState::kInitializing);
  EXPECT_EQ(overlay->state_for_testing(), UiState::kInitializing);
  EXPECT_TRUE(mic_button->GetVisible());
  EXPECT_FALSE(waveform_view->GetVisible());
  EXPECT_FALSE(finalizing_image->GetVisible());

  // Transition to kTranscribing: waveform visible, mic and finalizing hidden.
  overlay->SetState(UiState::kTranscribing);
  EXPECT_EQ(overlay->state_for_testing(), UiState::kTranscribing);
  EXPECT_FALSE(mic_button->GetVisible());
  EXPECT_TRUE(waveform_view->GetVisible());
  EXPECT_FALSE(finalizing_image->GetVisible());

  // Transition to kFinalizing: finalizing visible, mic and waveform hidden.
  overlay->SetState(UiState::kFinalizing);
  EXPECT_EQ(overlay->state_for_testing(), UiState::kFinalizing);
  EXPECT_FALSE(mic_button->GetVisible());
  EXPECT_FALSE(waveform_view->GetVisible());
  EXPECT_TRUE(finalizing_image->GetVisible());

  // Transition back to kInactive: mic_button visible.
  overlay->SetState(UiState::kInactive);
  EXPECT_EQ(overlay->state_for_testing(), UiState::kInactive);
  EXPECT_TRUE(mic_button->GetVisible());
  EXPECT_FALSE(waveform_view->GetVisible());
  EXPECT_FALSE(finalizing_image->GetVisible());
}

TEST_F(DictationOverlayViewTest, AudioLevelPropagatesToWaveform) {
  auto overlay = std::make_unique<DictationOverlayView>(
      parent_widget_->GetNativeView(), base::DoNothing());
  overlay->Show();

  views::View* contents_view = overlay->GetContentsView();
  ASSERT_NE(contents_view, nullptr);

  views::View* waveform_view_raw =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          DictationOverlayView::kWaveformElementIdForTesting,
          views::ElementTrackerViews::GetContextForView(contents_view));
  ASSERT_NE(waveform_view_raw, nullptr);

  auto* waveform_view =
      views::AsViewClass<WaveformViewButton>(waveform_view_raw);
  ASSERT_NE(waveform_view, nullptr);

  EXPECT_FALSE(waveform_view->full_size());

  EXPECT_FLOAT_EQ(waveform_view->audio_level_for_testing(), 0.0f);

  overlay->UpdateAudioLevel(0.05f);
  EXPECT_FLOAT_EQ(waveform_view->audio_level_for_testing(), 0.5f);
}

TEST_F(DictationOverlayViewTest, SubviewSizingAndMargin) {
  auto overlay = std::make_unique<DictationOverlayView>(
      parent_widget_->GetNativeView(), base::DoNothing());
  overlay->Show();

  views::View* contents_view = overlay->GetContentsView();
  ASSERT_NE(contents_view, nullptr);

  auto* tracker = views::ElementTrackerViews::GetInstance();
  auto context = views::ElementTrackerViews::GetContextForView(contents_view);

  views::View* mic_button = tracker->GetFirstMatchingView(
      DictationOverlayView::kMicButtonElementIdForTesting, context);
  views::View* waveform_view = tracker->GetFirstMatchingView(
      DictationOverlayView::kWaveformElementIdForTesting, context);
  views::View* finalizing_image = tracker->GetFirstMatchingView(
      DictationOverlayView::kFinalizingImageElementIdForTesting, context);

  ASSERT_NE(mic_button, nullptr);
  ASSERT_NE(waveform_view, nullptr);
  ASSERT_NE(finalizing_image, nullptr);

  // Subviews are sized to 20x20.
  EXPECT_EQ(mic_button->GetPreferredSize(), gfx::Size(20, 20));
  EXPECT_EQ(waveform_view->GetPreferredSize(), gfx::Size(20, 20));
  EXPECT_EQ(finalizing_image->GetPreferredSize(), gfx::Size(20, 20));

  // Inactive state overlay preferred size is a 32x32 circle (20px content +
  // 12px inset).
  EXPECT_EQ(contents_view->GetPreferredSize(), gfx::Size(32, 32));

  // Transcribing state overlay preferred size remains a 32x32 circle.
  overlay->SetState(UiState::kTranscribing);
  EXPECT_EQ(contents_view->GetPreferredSize(), gfx::Size(32, 32));
}

TEST_F(DictationOverlayViewTest, ClicksToggleActiveStream) {
  int toggle_count = 0;
  auto overlay = std::make_unique<DictationOverlayView>(
      parent_widget_->GetNativeView(),
      base::BindLambdaForTesting([&toggle_count]() { toggle_count++; }));
  overlay->Show();

  views::View* contents_view = overlay->GetContentsView();
  ASSERT_NE(contents_view, nullptr);

  auto* tracker = views::ElementTrackerViews::GetInstance();
  auto context = views::ElementTrackerViews::GetContextForView(contents_view);

  views::View* mic_button = tracker->GetFirstMatchingView(
      DictationOverlayView::kMicButtonElementIdForTesting, context);
  views::View* waveform_view = tracker->GetFirstMatchingView(
      DictationOverlayView::kWaveformElementIdForTesting, context);
  views::View* finalizing_image = tracker->GetFirstMatchingView(
      DictationOverlayView::kFinalizingImageElementIdForTesting, context);

  ASSERT_NE(mic_button, nullptr);
  ASSERT_NE(waveform_view, nullptr);
  ASSERT_NE(finalizing_image, nullptr);

  ui::MouseEvent click_event(ui::EventType::kMousePressed, gfx::Point(),
                             gfx::Point(), base::TimeTicks::Now(), 0, 0);

  views::test::ButtonTestApi(views::AsViewClass<views::Button>(mic_button))
      .NotifyClick(click_event);
  EXPECT_EQ(toggle_count, 1);

  overlay->SetState(UiState::kInitializing);
  views::test::ButtonTestApi(views::AsViewClass<views::Button>(mic_button))
      .NotifyClick(click_event);
  EXPECT_EQ(toggle_count, 2);

  overlay->SetState(UiState::kTranscribing);
  views::test::ButtonTestApi(views::AsViewClass<views::Button>(waveform_view))
      .NotifyClick(click_event);
  EXPECT_EQ(toggle_count, 3);
}

}  // namespace dictation
