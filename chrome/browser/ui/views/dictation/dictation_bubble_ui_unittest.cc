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

  // Update audio level. Note the boost factor in WaveformView::SetAudioLevel.
  bubble->UpdateAudioLevel(0.05f);
  EXPECT_FLOAT_EQ(waveform_view->audio_level_for_testing(), 0.5f);

  bubble->UpdateAudioLevel(0.2f);
  EXPECT_FLOAT_EQ(waveform_view->audio_level_for_testing(), 1.0f);
}

}  // namespace dictation
