// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/voice/waveform_view.h"

#include <algorithm>
#include <memory>

#include "base/time/time.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace voice {

class WaveformViewTest : public ChromeViewsTestBase {
 protected:
  std::unique_ptr<WaveformView> CreateFullSizeWaveformView() {
    return std::make_unique<WaveformView>(/*full_size=*/true);
  }
};

TEST_F(WaveformViewTest, ProcessingWaveAnimation) {
  auto waveform_view = CreateFullSizeWaveformView();
  waveform_view->SetMode(WaveformMode::kProcessing);

  const base::TimeTicks start_time = base::TimeTicks::Now();
  const int baseline_y = waveform_view->GetPreferredSize().height() / 2;

  // Sample wave animation state during the travel window (e.g. at 300ms).
  float min_size = 100.0f;
  float max_size = 0.0f;
  float max_lift = 0.0f;
  for (size_t i = 0; i < WaveformView::kDefaultFullSizeBarCount; ++i) {
    const WaveformView::AnimationState animation_state =
        waveform_view->GetProcessingAnimationState(
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
  for (size_t i = 0; i < WaveformView::kDefaultFullSizeBarCount; ++i) {
    const WaveformView::AnimationState pause_animation_state =
        waveform_view->GetProcessingAnimationState(i, pause_time);
    EXPECT_FLOAT_EQ(pause_animation_state.size, 2.0f);
    EXPECT_FLOAT_EQ(pause_animation_state.center_y, baseline_y);
  }
}

TEST_F(WaveformViewTest, AudioLevelMath) {
  auto waveform_view = CreateFullSizeWaveformView();
  waveform_view->SetMode(WaveformMode::kLive);

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

TEST_F(WaveformViewTest, DoesNotCollapseWhenIdleByDefault) {
  auto waveform_view = CreateFullSizeWaveformView();
  ASSERT_EQ(waveform_view->mode(), WaveformMode::kIdle);
  EXPECT_GT(waveform_view->GetPreferredSize().width(), 0);
  EXPECT_GT(waveform_view->GetPreferredSize().height(), 0);
}

TEST_F(WaveformViewTest, CollapsesOnlyWhenIdleIfRequested) {
  auto waveform_view = CreateFullSizeWaveformView();
  waveform_view->SetCollapseWhenIdle(true);

  EXPECT_EQ(waveform_view->mode(), WaveformMode::kIdle);
  EXPECT_EQ(waveform_view->GetPreferredSize(), gfx::Size(0, 0));

  for (WaveformMode mode : {WaveformMode::kWaiting, WaveformMode::kLive,
                            WaveformMode::kProcessing}) {
    waveform_view->SetMode(mode);
    EXPECT_GT(waveform_view->GetPreferredSize().width(), 0);
    EXPECT_GT(waveform_view->GetPreferredSize().height(), 0);
  }

  waveform_view->SetMode(WaveformMode::kIdle);
  EXPECT_EQ(waveform_view->GetPreferredSize(), gfx::Size(0, 0));

  waveform_view->SetCollapseWhenIdle(false);
  EXPECT_GT(waveform_view->GetPreferredSize().width(), 0);
  EXPECT_GT(waveform_view->GetPreferredSize().height(), 0);
}

}  // namespace voice
