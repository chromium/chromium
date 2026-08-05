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
constexpr int kFullSizeViewWidth = 63;
constexpr size_t kFullSizeBarCount = 11;

constexpr int kNonFullSizeViewWidth = 20;
constexpr int kNonFullSizeViewHeight = 20;
constexpr size_t kNonFullSizeBarCount = 4;

constexpr float kFullSizeBarSpacing = 6.0f;
constexpr float kNonFullSizeBarSpacing = 5.0f;
constexpr float kBarWidth = 3.0f;
constexpr float kBarCornerRadius = 1.5f;

// Height limits.
constexpr float kMinBarHeight = 3.0f;
constexpr float kFullSizeMaxBarHeight = 20.0f;
constexpr float kNonFullSizeMaxBarHeight = 14.0f;

// Physics parameters.
constexpr float kSpringConstant = 600.0f;
constexpr float kDampingCoefficient = 35.0f;

}  // namespace

WaveformView::WaveformView(bool full_size) : full_size_(full_size) {
  const size_t bar_count =
      full_size_ ? kFullSizeBarCount : kNonFullSizeBarCount;
  bars_.resize(bar_count);
  audio_history_.resize(GetCenterBarIndex() + 1, 0.0f);
  animation_ = std::make_unique<gfx::InfiniteAnimation>(this);
}

WaveformView::~WaveformView() = default;

size_t WaveformView::GetCenterBarIndex() const {
  return bars_.size() / 2;
}

void WaveformView::SetState(UiState state) {
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
  const int width = full_size_ ? kFullSizeViewWidth : kNonFullSizeViewWidth;
  const int height = full_size_ ? layout_provider->GetDistanceMetric(
                                      DISTANCE_TOAST_BUBBLE_HEIGHT_CONTENT)
                                : kNonFullSizeViewHeight;
  return gfx::Size(width, height);
}

void WaveformView::OnPaint(gfx::Canvas* canvas) {
  const float bar_spacing =
      full_size_ ? kFullSizeBarSpacing : kNonFullSizeBarSpacing;
  const float total_bars_width = (bars_.size() - 1) * bar_spacing + kBarWidth;
  const float start_x = (width() - total_bars_width) / 2.0f;
  const int center_y = height() / 2;

  cc::PaintFlags flags;
  flags.setColor(GetColorProvider()->GetColor(ui::kColorSysOnSurface));
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);

  for (size_t i = 0; i < bars_.size(); ++i) {
    const float bar_x = start_x + i * bar_spacing;
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

  if (state_ == UiState::kTranscribing) {
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

  const float max_bar_height =
      full_size_ ? kFullSizeMaxBarHeight : kNonFullSizeMaxBarHeight;

  for (size_t i = 0; i < bars_.size(); ++i) {
    bars_[i].target_height =
        GetTargetHeightForBar(i, time_sec, kMinBarHeight, max_bar_height);
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
    } else if (bars_[i].height > max_bar_height) {
      bars_[i].height = max_bar_height;
      bars_[i].velocity = 0.0f;
    }
  }
}

float WaveformView::GetTargetHeightForBar(size_t index,
                                          double time_sec,
                                          float min_height,
                                          float max_height) const {
  switch (state_) {
    case UiState::kTranscribing: {
      const size_t center_bar_index = GetCenterBarIndex();
      const int dist = std::abs(static_cast<int>(index) -
                                static_cast<int>(center_bar_index));
      const float sensitivity = 1.0f - (dist * 0.1f);
      return min_height +
             audio_history_[dist] * (max_height - min_height) * sensitivity;
    }
    case UiState::kInitializing:
    case UiState::kInactive:
    case UiState::kFinalizing:
      return min_height;
  }
}

BEGIN_METADATA(WaveformView)
END_METADATA

}  // namespace dictation
