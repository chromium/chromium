// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/dictation/dictation_bubble_ui.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/dictation/test_util.h"
#include "chrome/browser/ui/views/dictation/ui_state.h"
#include "chrome/browser/ui/views/dictation/waveform_view.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace dictation {

namespace {

constexpr size_t kBarCount = 9;

}  // namespace

class DictationBubbleUiTest : public ChromeViewsTestBase {
 public:
  DictationBubbleUiTest() = default;
  DictationBubbleUiTest(const DictationBubbleUiTest&) = delete;
  DictationBubbleUiTest& operator=(const DictationBubbleUiTest&) = delete;
  ~DictationBubbleUiTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    anchor_widget_ =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    anchor_view_ =
        anchor_widget_->SetContentsView(std::make_unique<views::View>());
    anchor_widget_->Show();
  }

  void TearDown() override {
    anchor_view_ = nullptr;
    anchor_widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  std::unique_ptr<views::Widget> anchor_widget_;
  raw_ptr<views::View> anchor_view_ = nullptr;
  base::test::ScopedFeatureList scoped_feature_list_{
      CreateEnablingFeatureList()};
};

TEST_F(DictationBubbleUiTest, StatePropagatesToWaveform) {
  auto bubble = std::make_unique<DictationBubbleUi>(
      anchor_view_, base::DoNothing(), base::DoNothing());
  bubble->Show();

  views::View* contents_view = bubble->GetContentsView();
  ASSERT_NE(contents_view, nullptr);

  views::View* waveform_view_raw =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          DictationBubbleUi::kWaveformElementIdForTesting,
          views::ElementTrackerViews::GetContextForView(contents_view));
  ASSERT_NE(waveform_view_raw, nullptr);

  auto* waveform_view = views::AsViewClass<WaveformView>(waveform_view_raw);
  ASSERT_NE(waveform_view, nullptr);

  EXPECT_TRUE(waveform_view->full_size());

  // Initial state should be kInactive.
  EXPECT_EQ(waveform_view->state(), UiState::kInactive);

  // Transition to kInitializing.
  bubble->SetState(UiState::kInitializing);
  EXPECT_EQ(waveform_view->state(), UiState::kInitializing);

  // Transition to kTranscribing.
  bubble->SetState(UiState::kTranscribing);
  EXPECT_EQ(waveform_view->state(), UiState::kTranscribing);

  // Transition to kFinalizing.
  bubble->SetState(UiState::kFinalizing);
  EXPECT_EQ(waveform_view->state(), UiState::kFinalizing);

  // Transition back to kInactive.
  bubble->SetState(UiState::kInactive);
  EXPECT_EQ(waveform_view->state(), UiState::kInactive);
}

TEST_F(DictationBubbleUiTest, AudioLevelPropagatesToWaveform) {
  auto bubble = std::make_unique<DictationBubbleUi>(
      anchor_view_, base::DoNothing(), base::DoNothing());
  bubble->Show();

  views::View* contents_view = bubble->GetContentsView();
  ASSERT_NE(contents_view, nullptr);

  views::View* waveform_view_raw =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          DictationBubbleUi::kWaveformElementIdForTesting,
          views::ElementTrackerViews::GetContextForView(contents_view));
  ASSERT_NE(waveform_view_raw, nullptr);

  auto* waveform_view = views::AsViewClass<WaveformView>(waveform_view_raw);
  ASSERT_NE(waveform_view, nullptr);

  // Initial audio level should be 0.
  EXPECT_FLOAT_EQ(waveform_view->audio_level_for_testing(), 0.0f);

  // Update audio level.
  bubble->UpdateAudioLevel(0.05f);
  EXPECT_FLOAT_EQ(waveform_view->audio_level_for_testing(), 0.05f);

  bubble->UpdateAudioLevel(0.2f);
  EXPECT_FLOAT_EQ(waveform_view->audio_level_for_testing(), 0.2f);
}

TEST_F(DictationBubbleUiTest, FinalizingWaveAnimation) {
  auto bubble = std::make_unique<DictationBubbleUi>(
      anchor_view_, base::DoNothing(), base::DoNothing());
  bubble->Show();

  views::View* contents_view = bubble->GetContentsView();
  ASSERT_NE(contents_view, nullptr);

  views::View* waveform_view_raw =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          DictationBubbleUi::kWaveformElementIdForTesting,
          views::ElementTrackerViews::GetContextForView(contents_view));
  ASSERT_NE(waveform_view_raw, nullptr);

  auto* waveform_view = views::AsViewClass<WaveformView>(waveform_view_raw);
  ASSERT_NE(waveform_view, nullptr);

  bubble->SetState(UiState::kFinalizing);

  const base::TimeTicks start_time = base::TimeTicks::Now();
  const int baseline_y = waveform_view->GetPreferredSize().height() / 2;

  // Sample wave animation state during the travel window (e.g. at 300ms).
  float min_size = 100.0f;
  float max_size = 0.0f;
  float max_lift = 0.0f;
  for (size_t i = 0; i < kBarCount; ++i) {
    const WaveformView::AnimationState animation_state =
        waveform_view->GetFinalizingAnimationState(
            i, start_time + base::Milliseconds(300));
    min_size = std::min(min_size, animation_state.size);
    max_size = std::max(max_size, animation_state.size);
    max_lift = std::max(max_lift, baseline_y - animation_state.center_y);

    EXPECT_GE(animation_state.size, 2.0f);
    EXPECT_LE(animation_state.size, 3.5f);
  }

  // There should be a highlighted crest (max size larger than min size)
  EXPECT_GT(max_size, min_size);
  EXPECT_GT(max_lift, 0.0f);

  // During the pause window (e.g. at 725ms with 700ms travel + 50ms pause),
  // all dots should rest at baseline.
  const base::TimeTicks pause_time = start_time + base::Milliseconds(725);
  for (size_t i = 0; i < kBarCount; ++i) {
    const WaveformView::AnimationState pause_animation_state =
        waveform_view->GetFinalizingAnimationState(i, pause_time);
    EXPECT_FLOAT_EQ(pause_animation_state.size, 2.0f);
    EXPECT_FLOAT_EQ(pause_animation_state.center_y, baseline_y);
  }
}

TEST_F(DictationBubbleUiTest, AudioLevelMath) {
  auto bubble = std::make_unique<DictationBubbleUi>(
      anchor_view_, base::DoNothing(), base::DoNothing());
  bubble->Show();
  bubble->SetState(UiState::kTranscribing);

  views::View* contents_view = bubble->GetContentsView();
  ASSERT_NE(contents_view, nullptr);
  views::View* waveform_view_raw =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          DictationBubbleUi::kWaveformElementIdForTesting,
          views::ElementTrackerViews::GetContextForView(contents_view));
  ASSERT_NE(waveform_view_raw, nullptr);

  auto* waveform_view = views::AsViewClass<WaveformView>(waveform_view_raw);
  ASSERT_NE(waveform_view, nullptr);

  // Constants that match what WaveformView uses.
  const float kMinBarHeight = 4.0f;
  const float kMaxBarHeight = 20.0f;
  const size_t center_index = waveform_view->GetCenterBarIndex();

  // Test Silence (0.0f level). Should result in minimum height.
  waveform_view->SetAudioLevel(0.0f);
  waveform_view->UpdatePhysics(base::Milliseconds(50));
  float height_silence = waveform_view->GetTargetHeightForBar(
      center_index, kMinBarHeight, kMaxBarHeight);
  EXPECT_FLOAT_EQ(height_silence, kMinBarHeight);

  // Test Small noise (0.05f level). Should still be relatively small, but above
  // min. We advance physics again to propagate it to audio_history_[0].
  waveform_view->SetAudioLevel(0.05f);
  waveform_view->UpdatePhysics(base::Milliseconds(50));
  float height_small = waveform_view->GetTargetHeightForBar(
      center_index, kMinBarHeight, kMaxBarHeight);
  EXPECT_GT(height_small, kMinBarHeight);

  // Test Max level (1.0f level). Should be fully at max height.
  waveform_view->SetAudioLevel(1.0f);
  waveform_view->UpdatePhysics(base::Milliseconds(50));
  float height_max = waveform_view->GetTargetHeightForBar(
      center_index, kMinBarHeight, kMaxBarHeight);
  EXPECT_GT(height_max, height_small);
  EXPECT_FLOAT_EQ(height_max, kMaxBarHeight);
}

TEST_F(DictationBubbleUiTest, WaveformCollapseWhenInactive) {
  auto bubble = std::make_unique<DictationBubbleUi>(
      anchor_view_, base::DoNothing(), base::DoNothing());
  bubble->Show();

  views::View* contents_view = bubble->GetContentsView();
  ASSERT_NE(contents_view, nullptr);

  views::View* waveform_view_raw =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          DictationBubbleUi::kWaveformElementIdForTesting,
          views::ElementTrackerViews::GetContextForView(contents_view));
  ASSERT_NE(waveform_view_raw, nullptr);

  auto* waveform_view = views::AsViewClass<WaveformView>(waveform_view_raw);
  ASSERT_NE(waveform_view, nullptr);

  // Inactive state
  EXPECT_EQ(waveform_view->state(), UiState::kInactive);
  EXPECT_EQ(waveform_view->GetPreferredSize(), gfx::Size(0, 0));

  // Initializing state
  bubble->SetState(UiState::kInitializing);
  EXPECT_EQ(waveform_view->state(), UiState::kInitializing);
  EXPECT_EQ(waveform_view->GetPreferredSize(), gfx::Size(0, 0));

  // Transcribing state
  bubble->SetState(UiState::kTranscribing);
  EXPECT_EQ(waveform_view->state(), UiState::kTranscribing);
  EXPECT_GT(waveform_view->GetPreferredSize().width(), 0);

  // Transitioning back to inactive state
  bubble->SetState(UiState::kInactive);
  EXPECT_EQ(waveform_view->state(), UiState::kInactive);
  EXPECT_EQ(waveform_view->GetPreferredSize(), gfx::Size(0, 0));
}

}  // namespace dictation
