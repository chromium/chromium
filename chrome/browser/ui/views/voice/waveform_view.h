// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_VOICE_WAVEFORM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_VOICE_WAVEFORM_VIEW_H_

#include <optional>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/views/voice/waveform_mode.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/views/view.h"

namespace gfx {
class InfiniteAnimation;
}

namespace voice {

// A custom View that draws an animated voice waveform consisting of vertical
// rounded bars. The bars are driven by the mic volume in `WaveformMode::kLive`,
// and a traveling wave animation plays in `WaveformMode::kProcessing`.
class WaveformView : public views::View, public gfx::AnimationDelegate {
  METADATA_HEADER(WaveformView, views::View)

 public:
  static constexpr size_t kDefaultFullSizeBarCount = 9;
  static constexpr size_t kCompactFullSizeBarCount = 5;

  explicit WaveformView(bool full_size);
  WaveformView(const WaveformView&) = delete;
  WaveformView& operator=(const WaveformView&) = delete;
  ~WaveformView() override;

  bool full_size() const { return full_size_; }

  void SetBarCount(size_t bar_count);
  size_t bar_count() const { return bars_.size(); }

  void SetCollapseWhenIdle(bool collapse_when_idle);

  // Sets what the waveform draws.
  void SetMode(WaveformMode mode);
  WaveformMode mode() const { return mode_; }

  float audio_level_for_testing() const { return audio_level_; }

  // Drives the wave with real mic volume (0.0 to 1.0).
  void SetAudioLevel(float level);

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void OnPaint(gfx::Canvas* canvas) override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(WaveformViewTest, ProcessingWaveAnimation);
  FRIEND_TEST_ALL_PREFIXES(WaveformViewTest, AudioLevelMath);

  struct AnimationState {
    float center_y = 0.0f;
    float size = 0.0f;
  };
  // Animation update ticks (running at 60 FPS).
  void UpdatePhysics(base::TimeDelta delta);
  float GetTargetHeightForBar(size_t index,
                              float min_height,
                              float max_height) const;
  AnimationState GetProcessingAnimationState(size_t index,
                                             base::TimeTicks now) const;

  size_t GetCenterBarIndex() const;
  bool IsCollapsed() const;

  const bool full_size_;
  bool collapse_when_idle_ = false;

  WaveformMode mode_ = WaveformMode::kIdle;

  // Animation timer and tracking.
  std::unique_ptr<gfx::InfiniteAnimation> animation_;
  base::TimeTicks last_update_time_;
  base::TimeTicks processing_start_time_;

  // Audio level and ripple history.
  float audio_level_ = 0.0f;
  std::optional<float> active_volume_;
  float last_volume_ = 0.0f;
  bool previous_read_empty_ = false;
  std::vector<float> audio_history_;
  base::TimeDelta history_timer_;

  // Physics state for the bars.
  struct BarState {
    float height = 4.0f;
    float target_height = 4.0f;
  };
  std::vector<BarState> bars_;
};

}  // namespace voice

#endif  // CHROME_BROWSER_UI_VIEWS_VOICE_WAVEFORM_VIEW_H_
