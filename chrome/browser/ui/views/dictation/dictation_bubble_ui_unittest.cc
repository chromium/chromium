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
#include "chrome/browser/ui/views/voice/waveform_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

namespace dictation {

using voice::WaveformMode;
using voice::WaveformView;

namespace {

views::View* FindViewByElementId(views::View* root, ui::ElementIdentifier id) {
  if (!root) {
    return nullptr;
  }
  if (root->GetProperty(views::kElementIdentifierKey) == id) {
    return root;
  }
  for (views::View* child : root->children()) {
    if (views::View* found = FindViewByElementId(child, id)) {
      return found;
    }
  }
  return nullptr;
}

}  // namespace

class DictationBubbleUiTest : public ChromeViewsTestBase,
                              public testing::WithParamInterface<bool> {
 public:
  DictationBubbleUiTest()
      : scoped_feature_list_(CreateEnablingFeatureList(GetParam())) {}
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
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(DictationBubbleUiTest, StatePropagatesToWaveform) {
  auto bubble = std::make_unique<DictationBubbleUi>(
      anchor_view_, base::DoNothing(), base::DoNothing(),
      /*show_reviewing_page_status=*/false);
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
  EXPECT_EQ(waveform_view->mode(), WaveformMode::kIdle);

  // Transition to kInitializing.
  bubble->SetState(UiState::kInitializing);
  EXPECT_EQ(waveform_view->mode(), WaveformMode::kWaiting);

  // Transition to kTranscribing.
  bubble->SetState(UiState::kTranscribing);
  EXPECT_EQ(waveform_view->mode(), WaveformMode::kLive);

  // Transition to kFinalizing.
  bubble->SetState(UiState::kFinalizing);
  EXPECT_EQ(waveform_view->mode(), WaveformMode::kProcessing);

  // Transition back to kInactive.
  bubble->SetState(UiState::kInactive);
  EXPECT_EQ(waveform_view->mode(), WaveformMode::kIdle);
}

TEST_P(DictationBubbleUiTest, StatePropagatesToToggleButton) {
  auto bubble = std::make_unique<DictationBubbleUi>(
      anchor_view_, base::DoNothing(), base::DoNothing(),
      /*show_reviewing_page_status=*/false);
  bubble->Show();

  views::View* contents_view = bubble->GetContentsView();
  ASSERT_NE(contents_view, nullptr);

  views::View* toggle_button_raw =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          DictationBubbleUi::kToggleButtonElementIdForTesting,
          views::ElementTrackerViews::GetContextForView(contents_view));
  ASSERT_NE(toggle_button_raw, nullptr);

  auto* toggle_button =
      views::AsViewClass<views::MdTextButton>(toggle_button_raw);
  ASSERT_NE(toggle_button, nullptr);

  const bool session_ends_on_stream_end = GetParam();

  bubble->SetState(UiState::kTranscribing);
  EXPECT_TRUE(toggle_button->GetEnabled());

  bubble->SetState(UiState::kFinalizing);
  EXPECT_FALSE(toggle_button->GetEnabled());

  bubble->SetState(UiState::kInactive);
  EXPECT_EQ(toggle_button->GetEnabled(), !session_ends_on_stream_end);
}

TEST_P(DictationBubbleUiTest, AudioLevelPropagatesToWaveform) {
  auto bubble = std::make_unique<DictationBubbleUi>(
      anchor_view_, base::DoNothing(), base::DoNothing(),
      /*show_reviewing_page_status=*/false);
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

TEST_P(DictationBubbleUiTest, WaveformSizing) {
  auto bubble = std::make_unique<DictationBubbleUi>(
      anchor_view_, base::DoNothing(), base::DoNothing(),
      /*show_reviewing_page_status=*/false);
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

  const bool session_ends_on_stream_end = GetParam();

  // Inactive state
  EXPECT_EQ(waveform_view->mode(), WaveformMode::kIdle);
  if (session_ends_on_stream_end) {
    EXPECT_GT(waveform_view->GetPreferredSize().width(), 0);
    EXPECT_GT(waveform_view->GetPreferredSize().height(), 0);
  } else {
    EXPECT_EQ(waveform_view->GetPreferredSize(), gfx::Size(0, 0));
  }

  // Initializing state
  bubble->SetState(UiState::kInitializing);
  EXPECT_EQ(waveform_view->mode(), WaveformMode::kWaiting);
  if (session_ends_on_stream_end) {
    EXPECT_GT(waveform_view->GetPreferredSize().width(), 0);
    EXPECT_GT(waveform_view->GetPreferredSize().height(), 0);
  } else {
    EXPECT_GT(waveform_view->GetPreferredSize().width(), 0);
  }

  // Transcribing state
  bubble->SetState(UiState::kTranscribing);
  EXPECT_EQ(waveform_view->mode(), WaveformMode::kLive);
  EXPECT_GT(waveform_view->GetPreferredSize().width(), 0);

  // Transitioning back to inactive state
  bubble->SetState(UiState::kInactive);
  EXPECT_EQ(waveform_view->mode(), WaveformMode::kIdle);
  if (session_ends_on_stream_end) {
    EXPECT_GT(waveform_view->GetPreferredSize().width(), 0);
    EXPECT_GT(waveform_view->GetPreferredSize().height(), 0);
  } else {
    EXPECT_EQ(waveform_view->GetPreferredSize(), gfx::Size(0, 0));
  }
}

TEST_P(DictationBubbleUiTest, ReviewingPageStatusInitialLayout) {
  auto bubble = std::make_unique<DictationBubbleUi>(
      anchor_view_, base::DoNothing(), base::DoNothing(),
      /*show_reviewing_page_status=*/true);
  bubble->Show();

  views::View* contents_view = bubble->GetContentsView();
  ASSERT_NE(contents_view, nullptr);

  views::View* separator = FindViewByElementId(
      contents_view, DictationBubbleUi::kSeparatorElementIdForTesting);
  views::View* label_raw = FindViewByElementId(
      contents_view,
      DictationBubbleUi::kReviewingPageStatusLabelElementIdForTesting);
  views::View* toggle_button = FindViewByElementId(
      contents_view, DictationBubbleUi::kToggleButtonElementIdForTesting);

  ASSERT_NE(separator, nullptr);
  ASSERT_NE(label_raw, nullptr);
  ASSERT_NE(toggle_button, nullptr);

  EXPECT_TRUE(separator->GetVisible());
  EXPECT_TRUE(label_raw->GetVisible());
  EXPECT_FALSE(toggle_button->GetVisible());

  auto* label = views::AsViewClass<views::Label>(label_raw);
  ASSERT_NE(label, nullptr);
  EXPECT_EQ(label->GetText(),
            l10n_util::GetStringUTF16(IDS_DICTATION_REVIEWING_PAGE));

  views::View* waveform_raw = FindViewByElementId(
      contents_view, DictationBubbleUi::kWaveformElementIdForTesting);
  auto* waveform_view = views::AsViewClass<WaveformView>(waveform_raw);
  ASSERT_NE(waveform_view, nullptr);
  EXPECT_EQ(waveform_view->bar_count(), WaveformView::kCompactFullSizeBarCount);
}

TEST_P(DictationBubbleUiTest, ReviewingPageStatusAutoDismissesAfterTimeout) {
  auto bubble = std::make_unique<DictationBubbleUi>(
      anchor_view_, base::DoNothing(), base::DoNothing(),
      /*show_reviewing_page_status=*/true);
  bubble->SetReviewingPageStatusDurationForTesting(base::Milliseconds(60));
  bubble->Show();

  views::View* contents_view = bubble->GetContentsView();
  ASSERT_NE(contents_view, nullptr);

  views::View* separator = FindViewByElementId(
      contents_view, DictationBubbleUi::kSeparatorElementIdForTesting);
  views::View* label = FindViewByElementId(
      contents_view,
      DictationBubbleUi::kReviewingPageStatusLabelElementIdForTesting);
  views::View* toggle_button = FindViewByElementId(
      contents_view, DictationBubbleUi::kToggleButtonElementIdForTesting);
  views::View* waveform_raw = FindViewByElementId(
      contents_view, DictationBubbleUi::kWaveformElementIdForTesting);
  auto* waveform_view = views::AsViewClass<WaveformView>(waveform_raw);

  ASSERT_NE(separator, nullptr);
  ASSERT_NE(label, nullptr);
  ASSERT_NE(toggle_button, nullptr);
  ASSERT_NE(waveform_view, nullptr);

  // Before timeout, context sharing remains active with 5 waveform bars.
  task_environment()->FastForwardBy(base::Milliseconds(40));
  EXPECT_TRUE(separator->IsDrawn());
  EXPECT_TRUE(label->IsDrawn());
  EXPECT_FALSE(toggle_button->IsDrawn());
  EXPECT_EQ(waveform_view->bar_count(), WaveformView::kCompactFullSizeBarCount);
  const int height_with_status = contents_view->GetPreferredSize().height();

  // At timeout, context sharing dismisses, Done button appears, and waveform
  // expands to 9 bars.
  task_environment()->FastForwardBy(base::Milliseconds(30));
  EXPECT_FALSE(separator->IsDrawn());
  EXPECT_FALSE(label->IsDrawn());
  EXPECT_TRUE(toggle_button->IsDrawn());
  EXPECT_EQ(waveform_view->bar_count(), WaveformView::kDefaultFullSizeBarCount);

  // Bubble height remains unchanged.
  EXPECT_EQ(contents_view->GetPreferredSize().height(), height_with_status);
}

TEST_P(DictationBubbleUiTest,
       ReviewingPageStatusPausesOnHoverAndResumesOnExit) {
  auto bubble = std::make_unique<DictationBubbleUi>(
      anchor_view_, base::DoNothing(), base::DoNothing(),
      /*show_reviewing_page_status=*/true);
  bubble->SetReviewingPageStatusDurationForTesting(base::Milliseconds(60));
  bubble->Show();

  views::View* contents_view = bubble->GetContentsView();
  ASSERT_NE(contents_view, nullptr);

  views::View* separator = FindViewByElementId(
      contents_view, DictationBubbleUi::kSeparatorElementIdForTesting);
  views::View* label = FindViewByElementId(
      contents_view,
      DictationBubbleUi::kReviewingPageStatusLabelElementIdForTesting);
  views::View* toggle_button = FindViewByElementId(
      contents_view, DictationBubbleUi::kToggleButtonElementIdForTesting);

  ASSERT_NE(separator, nullptr);
  ASSERT_NE(label, nullptr);
  ASSERT_NE(toggle_button, nullptr);

  // Advance 20ms (40ms remaining).
  task_environment()->FastForwardBy(base::Milliseconds(20));
  EXPECT_TRUE(separator->IsDrawn());
  EXPECT_FALSE(toggle_button->IsDrawn());

  // Mouse enters the toast view
  ASSERT_NE(bubble->GetWidget(), nullptr);
  ui::test::EventGenerator generator(views::GetRootWindow(bubble->GetWidget()),
                                     bubble->GetWidget()->GetNativeWindow());
  generator.MoveMouseTo(contents_view->GetBoundsInScreen().CenterPoint());

  // Advance 80ms while hovered (total 100ms elapsed > 60ms duration),
  // confirming the timer is paused and did not expire.
  task_environment()->FastForwardBy(base::Milliseconds(80));
  EXPECT_TRUE(separator->IsDrawn());
  EXPECT_TRUE(label->IsDrawn());
  EXPECT_FALSE(toggle_button->IsDrawn());

  // Mouse exits
  generator.MoveMouseTo(gfx::Point(-100, -100));

  // 25ms after unhovering, still visible.
  task_environment()->FastForwardBy(base::Milliseconds(25));
  EXPECT_TRUE(separator->IsDrawn());
  EXPECT_FALSE(toggle_button->IsDrawn());

  // 45ms after unhovering, dismissed.
  task_environment()->FastForwardBy(base::Milliseconds(20));
  EXPECT_FALSE(separator->IsDrawn());
  EXPECT_FALSE(label->IsDrawn());
  EXPECT_TRUE(toggle_button->IsDrawn());
}

TEST_P(DictationBubbleUiTest,
       ReviewingPageStatusPausesOnFocusAndResumesOnBlur) {
  auto bubble = std::make_unique<DictationBubbleUi>(
      anchor_view_, base::DoNothing(), base::DoNothing(),
      /*show_reviewing_page_status=*/true);
  bubble->SetReviewingPageStatusDurationForTesting(base::Milliseconds(60));
  bubble->Show();

  views::View* contents_view = bubble->GetContentsView();
  ASSERT_NE(contents_view, nullptr);

  views::View* separator = FindViewByElementId(
      contents_view, DictationBubbleUi::kSeparatorElementIdForTesting);
  views::View* label = FindViewByElementId(
      contents_view,
      DictationBubbleUi::kReviewingPageStatusLabelElementIdForTesting);
  views::View* toggle_button = FindViewByElementId(
      contents_view, DictationBubbleUi::kToggleButtonElementIdForTesting);
  views::View* close_button = FindViewByElementId(
      contents_view, DictationBubbleUi::kCloseButtonElementIdForTesting);

  ASSERT_NE(separator, nullptr);
  ASSERT_NE(label, nullptr);
  ASSERT_NE(toggle_button, nullptr);
  ASSERT_NE(close_button, nullptr);

  // Advance 20ms (40ms remaining).
  task_environment()->FastForwardBy(base::Milliseconds(20));

  // Focus a child view
  close_button->RequestFocus();
  EXPECT_TRUE(close_button->HasFocus());

  // Advance 80ms while focused (total 100ms elapsed > 60ms duration),
  // confirming the timer is paused and did not expire.
  task_environment()->FastForwardBy(base::Milliseconds(80));
  EXPECT_TRUE(separator->IsDrawn());
  EXPECT_TRUE(label->IsDrawn());
  EXPECT_FALSE(toggle_button->IsDrawn());

  // Clear focus
  contents_view->GetFocusManager()->ClearFocus();

  // 25ms after blur, still visible.
  task_environment()->FastForwardBy(base::Milliseconds(25));
  EXPECT_TRUE(separator->IsDrawn());
  EXPECT_FALSE(toggle_button->IsDrawn());

  // 45ms after blur, dismissed.
  task_environment()->FastForwardBy(base::Milliseconds(20));
  EXPECT_FALSE(separator->IsDrawn());
  EXPECT_FALSE(label->IsDrawn());
  EXPECT_TRUE(toggle_button->IsDrawn());
}

TEST_P(DictationBubbleUiTest, ReviewingPageStatusAudioLevelPropagates) {
  auto bubble = std::make_unique<DictationBubbleUi>(
      anchor_view_, base::DoNothing(), base::DoNothing(),
      /*show_reviewing_page_status=*/true);
  bubble->Show();

  views::View* contents_view = bubble->GetContentsView();
  ASSERT_NE(contents_view, nullptr);

  views::View* separator = FindViewByElementId(
      contents_view, DictationBubbleUi::kSeparatorElementIdForTesting);
  views::View* label = FindViewByElementId(
      contents_view,
      DictationBubbleUi::kReviewingPageStatusLabelElementIdForTesting);
  views::View* waveform_view_raw = FindViewByElementId(
      contents_view, DictationBubbleUi::kWaveformElementIdForTesting);
  ASSERT_NE(waveform_view_raw, nullptr);
  auto* waveform_view = views::AsViewClass<WaveformView>(waveform_view_raw);
  ASSERT_NE(waveform_view, nullptr);

  bubble->SetState(UiState::kTranscribing);
  bubble->UpdateAudioLevel(0.35f);

  EXPECT_EQ(waveform_view->mode(), WaveformMode::kLive);
  EXPECT_FLOAT_EQ(waveform_view->audio_level_for_testing(), 0.35f);
  EXPECT_TRUE(separator->GetVisible());
  EXPECT_TRUE(label->GetVisible());
}

INSTANTIATE_TEST_SUITE_P(All, DictationBubbleUiTest, testing::Bool());

}  // namespace dictation
