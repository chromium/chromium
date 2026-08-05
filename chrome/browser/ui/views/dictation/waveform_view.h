// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DICTATION_WAVEFORM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DICTATION_WAVEFORM_VIEW_H_

#include <vector>

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/views/dictation/ui_state.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/views/view.h"

namespace gfx {
class InfiniteAnimation;
}

namespace dictation {

// A custom View that draws an animated voice waveform consisting of vertical
// rounded bars. The animation only plays during transcribing, using a
// spring-damper physics simulation driven by the audio level.
class WaveformView : public views::View, public gfx::AnimationDelegate {
  METADATA_HEADER(WaveformView, views::View)

 public:
  explicit WaveformView(bool full_size);
  WaveformView(const WaveformView&) = delete;
  WaveformView& operator=(const WaveformView&) = delete;
  ~WaveformView() override;

  bool full_size() const { return full_size_; }

  // Set the current dictation state to control the animation behavior.
  void SetState(UiState state);
  UiState state() const { return state_; }

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
  // Animation update ticks (running at 60 FPS).
  void UpdatePhysics(base::TimeDelta delta);
  float GetTargetHeightForBar(size_t index,
                              double time_sec,
                              float min_height,
                              float max_height) const;

  size_t GetCenterBarIndex() const;

  const bool full_size_;

  UiState state_ = UiState::kInactive;

  // Animation timer and tracking.
  std::unique_ptr<gfx::InfiniteAnimation> animation_;
  base::TimeTicks last_update_time_;

  // Audio level and ripple history.
  float audio_level_ = 0.0f;
  std::vector<float> audio_history_;
  base::TimeDelta history_timer_;

  // Physics state for the bars.
  struct BarState {
    float height = 3.0f;
    float target_height = 3.0f;
    float velocity = 0.0f;
  };
  std::vector<BarState> bars_;

};

}  // namespace dictation

#endif  // CHROME_BROWSER_UI_VIEWS_DICTATION_WAVEFORM_VIEW_H_
