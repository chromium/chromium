// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/dictation/waveform_view.h"

#include <algorithm>
#include <cmath>

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/animation/infinite_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect_f.h"
namespace dictation {

namespace {

// Sizing and layout.
constexpr int kViewWidth = 63;
constexpr size_t kBarCount = 11;
constexpr size_t kCenterBarIndex = kBarCount / 2;
constexpr size_t kAudioHistorySize = kCenterBarIndex + 1;
constexpr float kBarSpacing = 6.0f;
constexpr float kBarWidth = 3.0f;
constexpr float kBarCornerRadius = 1.5f;

// Height limits.
constexpr float kMinBarHeight = 3.0f;
constexpr float kMaxBarHeight = 20.0f;

// Physics parameters.
constexpr float kSpringConstant = 600.0f;
constexpr float kDampingCoefficient = 35.0f;

}  // namespace

WaveformView::WaveformView() {
  bars_.resize(kBarCount);
  audio_history_.resize(kAudioHistorySize, 0.0f);
  animation_ = std::make_unique<gfx::InfiniteAnimation>(this);
}

WaveformView::~WaveformView() = default;

void WaveformView::SetState(DictationBubbleUi::State state) {
  if (state_ == state) {
    return;
  }
  state_ = state;
}

void WaveformView::SetAudioLevel(float level) {
  // An experimentally determined factor to boost the input audio level (0.0 to
  // 1.0) into a range that works well for visualization. This ensures the
  // waveform is lively and responsive even at lower input volumes.
  constexpr float kBoostFactor = 10.0f;
  audio_level_ = std::clamp(level * kBoostFactor, 0.0f, 1.0f);
}

gfx::Size WaveformView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const auto* const layout_provider = ChromeLayoutProvider::Get();
  return gfx::Size(kViewWidth, layout_provider->GetDistanceMetric(
                                   DISTANCE_TOAST_BUBBLE_HEIGHT_CONTENT));
}

void WaveformView::OnPaint(gfx::Canvas* canvas) {
  const int start_x = (width() - kViewWidth) / 2;
  const int center_y = height() / 2;

  cc::PaintFlags flags;
  flags.setColor(GetColorProvider()->GetColor(ui::kColorSysOnSurface));
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);

  for (size_t i = 0; i < bars_.size(); ++i) {
    const float bar_x = start_x + i * kBarSpacing;
    const float bar_h = bars_[i].height;
    const gfx::RectF rect(bar_x, center_y - bar_h / 2.0f, kBarWidth, bar_h);
    canvas->DrawRoundRect(rect, kBarCornerRadius, flags);
  }
}

void WaveformView::AddedToWidget() {
  last_update_time_ = base::TimeTicks::Now();
  animation_->Start();
}

void WaveformView::RemovedFromWidget() {
  animation_->Stop();
}

void WaveformView::AnimationProgressed(const gfx::Animation* animation) {
  const base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta delta = now - last_update_time_;
  last_update_time_ = now;

  UpdatePhysics(delta);
  SchedulePaint();
}

void WaveformView::UpdatePhysics(base::TimeDelta delta) {
  const double dt = std::min(delta.InSecondsF(), 0.05);

  if (state_ == DictationBubbleUi::State::kTranscribing) {
    // Propagate the audio level from the center outwards.
    history_timer_ += delta;
    if (history_timer_ >= base::Milliseconds(30)) {
      history_timer_ = base::TimeDelta();
      for (size_t i = audio_history_.size() - 1; i > 0; --i) {
        audio_history_[i] = audio_history_[i - 1];
      }
      audio_history_[0] = audio_level_;
    }
  }

  const double time_sec =
      (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();

  for (size_t i = 0; i < bars_.size(); ++i) {
    bars_[i].target_height =
        GetTargetHeightForBar(i, time_sec, kMinBarHeight, kMaxBarHeight);
  }

  // Run the spring-damper integration loop.
  for (size_t i = 0; i < bars_.size(); ++i) {
    const float diff = bars_[i].target_height - bars_[i].height;
    const float spring_force = diff * kSpringConstant;
    const float damping_force = -bars_[i].velocity * kDampingCoefficient;
    const float accel = spring_force + damping_force;

    bars_[i].velocity += accel * dt;
    bars_[i].height += bars_[i].velocity * dt;

    // Clamp heights and zero velocities at the boundaries.
    if (bars_[i].height < kMinBarHeight) {
      bars_[i].height = kMinBarHeight;
      bars_[i].velocity = 0.0f;
    } else if (bars_[i].height > kMaxBarHeight) {
      bars_[i].height = kMaxBarHeight;
      bars_[i].velocity = 0.0f;
    }
  }
}

float WaveformView::GetTargetHeightForBar(size_t index,
                                          double time_sec,
                                          float min_height,
                                          float max_height) const {
  switch (state_) {
    case DictationBubbleUi::State::kTranscribing: {
      const int dist =
          std::abs(static_cast<int>(index) - static_cast<int>(kCenterBarIndex));
      const float sensitivity = 1.0f - (dist * 0.1f);
      return min_height +
             audio_history_[dist] * (max_height - min_height) * sensitivity;
    }
    case DictationBubbleUi::State::kInitializing:
    case DictationBubbleUi::State::kInactive:
    case DictationBubbleUi::State::kFinalizing:
      return min_height;
  }
}

BEGIN_METADATA(WaveformView)
END_METADATA

}  // namespace dictation
