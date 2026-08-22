// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/dictation/waveform_view.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "base/check_op.h"
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
constexpr size_t kFullSizeBarCount = 9;

constexpr int kNonFullSizeViewWidth = 20;
constexpr int kNonFullSizeViewHeight = 20;
constexpr size_t kNonFullSizeBarCount = 3;

constexpr float kFullSizeBarSpacing = 6.0f;
constexpr float kNonFullSizeBarSpacing = 5.0f;
constexpr float kBarWidth = 2.0f;
constexpr float kBarCornerRadius = 0.5f;

// Height limits.
constexpr float kMinBarHeight = 4.0f;
constexpr float kFullSizeMaxBarHeight = 20.0f;
constexpr float kNonFullSizeMaxBarHeight = 14.0f;

// Lerp animation parameters.
constexpr float kBubblingSpeed = 0.40f;

// Audio sensitivity & response curve parameters.
constexpr float kSilenceThreshold = 0.005f;
constexpr float kAudioSensitivity = 8.0f;
constexpr float kInflectionInput = 0.75f;
constexpr float kInflectionOutputPercent = 0.75f;

// Finalizing wave animation parameters.
constexpr base::TimeDelta kFinalizingWaveTravelDuration =
    base::Milliseconds(700);
constexpr base::TimeDelta kFinalizingWavePauseDuration = base::Milliseconds(50);
constexpr float kFinalizingWaveAmplitude = 5.0f;
constexpr float kFinalizingBaseDotSize = 2.0f;
constexpr float kFinalizingHighlightDotSize = 3.5f;
constexpr double kFinalizingPacketHalfWidth = 2.5;

float MapRange(float value,
               float from_min,
               float from_max,
               float to_min,
               float to_max) {
  CHECK_LT(from_min, from_max);
  CHECK_LT(to_min, to_max);
  const float input_range = from_max - from_min;
  const float progress = (value - from_min) / input_range;
  return to_min + progress * (to_max - to_min);
}

float CalculateBarHeight(float min_bar_height,
                         float max_bar_height,
                         float raw_level) {
  if (raw_level <= 0.0f) {
    return min_bar_height;
  }
  const float normalized_level =
      std::sqrt(std::clamp(raw_level * kAudioSensitivity, 0.0f, 1.0f));
  if (normalized_level <= kSilenceThreshold) {
    return min_bar_height;
  }
  const float inflection_output = max_bar_height * kInflectionOutputPercent;
  if (normalized_level < kInflectionInput) {
    return MapRange(normalized_level, kSilenceThreshold, kInflectionInput,
                    min_bar_height, inflection_output);
  }
  return MapRange(normalized_level, kInflectionInput, 1.0f, inflection_output,
                  max_bar_height);
}

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
  if (state_ == UiState::kFinalizing) {
    finalizing_start_time_ = base::TimeTicks::Now();
  } else {
    finalizing_start_time_ = base::TimeTicks();
  }
  if (state_ == UiState::kInactive) {
    std::fill(audio_history_.begin(), audio_history_.end(), 0.0f);
    active_volume_ = std::nullopt;
    last_volume_ = 0.0f;
    previous_read_empty_ = false;
  }
  InvalidateLayout();
  SchedulePaint();
}

void WaveformView::SetAudioLevel(float level) {
  audio_level_ = std::clamp(level, 0.0f, 1.0f);
  active_volume_ = audio_level_;
}

WaveformView::AnimationState WaveformView::GetFinalizingAnimationState(
    size_t index,
    base::TimeTicks now) const {
  const float baseline_y =
      height() > 0 ? height() / 2.0f : GetPreferredSize().height() / 2.0f;
  if (finalizing_start_time_.is_null()) {
    return {baseline_y, kFinalizingBaseDotSize};
  }

  const base::TimeDelta elapsed = now - finalizing_start_time_;
  const base::TimeDelta cycle_period =
      kFinalizingWaveTravelDuration + kFinalizingWavePauseDuration;
  const base::TimeDelta cycle_time = elapsed % cycle_period;

  double wave_weight = 0.0;
  if (cycle_time < kFinalizingWaveTravelDuration) {
    const double progress = cycle_time / kFinalizingWaveTravelDuration;
    const double bar_count = static_cast<double>(bars_.size());
    const double crest_pos =
        -kFinalizingPacketHalfWidth +
        progress * (bar_count - 1 + 2.0 * kFinalizingPacketHalfWidth);
    const double dist = std::abs(static_cast<double>(index) - crest_pos);
    if (dist < kFinalizingPacketHalfWidth) {
      wave_weight = 0.5 * (1.0 + std::cos(std::numbers::pi_v<double> * dist /
                                          kFinalizingPacketHalfWidth));
    }
  }

  const float weight = static_cast<float>(wave_weight);
  return {
      baseline_y - kFinalizingWaveAmplitude * weight,
      std::lerp(kFinalizingBaseDotSize, kFinalizingHighlightDotSize, weight)};
}

gfx::Size WaveformView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  if (state_ == UiState::kInactive || state_ == UiState::kInitializing) {
    return gfx::Size(0, 0);
  }
  const auto* const layout_provider = ChromeLayoutProvider::Get();
  const int width = full_size_ ? kFullSizeViewWidth : kNonFullSizeViewWidth;
  const int height = full_size_ ? layout_provider->GetDistanceMetric(
                                      DISTANCE_TOAST_BUBBLE_HEIGHT_CONTENT)
                                : kNonFullSizeViewHeight;
  return gfx::Size(width, height);
}

void WaveformView::OnPaint(gfx::Canvas* canvas) {
  // Painting is skipped in kInactive and kInitializing states as the parent
  // view is expected to hide or collapse this view in those states.
  if (state_ == UiState::kInactive || state_ == UiState::kInitializing) {
    return;
  }

  const float bar_spacing =
      full_size_ ? kFullSizeBarSpacing : kNonFullSizeBarSpacing;
  const float total_bars_width = (bars_.size() - 1) * bar_spacing + kBarWidth;
  const float start_x = (width() - total_bars_width) / 2.0f;
  const int baseline_y = height() / 2;

  cc::PaintFlags flags;
  flags.setColor(GetColorProvider()->GetColor(ui::kColorSysOnSurface));
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);

  if (state_ == UiState::kFinalizing) {
    const base::TimeTicks now = base::TimeTicks::Now();
    for (size_t i = 0; i < bars_.size(); ++i) {
      const float dot_x = start_x + i * bar_spacing + kBarWidth / 2.0f;
      const AnimationState animation_state =
          GetFinalizingAnimationState(i, now);
      const gfx::RectF rect(
          dot_x - animation_state.size / 2.0f,
          animation_state.center_y - animation_state.size / 2.0f,
          animation_state.size, animation_state.size);
      canvas->DrawRoundRect(rect, animation_state.size / 2.0f, flags);
    }
    return;
  }

  for (size_t i = 0; i < bars_.size(); ++i) {
    const float bar_x = start_x + i * bar_spacing;
    const float bar_h = bars_[i].height;
    const gfx::RectF rect(bar_x, baseline_y - bar_h / 2.0f, kBarWidth, bar_h);
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
  if (state_ == UiState::kFinalizing) {
    return;
  }

  if (state_ == UiState::kTranscribing) {
    // Propagate the audio level from the center outwards.
    history_timer_ += delta;
    if (history_timer_ >= base::Milliseconds(45)) {
      history_timer_ = base::TimeDelta();
      for (size_t i = audio_history_.size() - 1; i > 0; --i) {
        audio_history_[i] = audio_history_[i - 1];
      }
      if (active_volume_.has_value()) {
        previous_read_empty_ = false;
        last_volume_ = active_volume_.value();
        audio_history_[0] = active_volume_.value();
        active_volume_ = std::nullopt;
      } else {
        if (!previous_read_empty_) {
          audio_history_[0] = last_volume_;
          previous_read_empty_ = true;
        } else {
          audio_history_[0] = 0.0f;
        }
      }
    }
  }

  const float max_bar_height =
      full_size_ ? kFullSizeMaxBarHeight : kNonFullSizeMaxBarHeight;

  for (size_t i = 0; i < bars_.size(); ++i) {
    bars_[i].target_height =
        GetTargetHeightForBar(i, kMinBarHeight, max_bar_height);
    bars_[i].height +=
        (bars_[i].target_height - bars_[i].height) * kBubblingSpeed;
    bars_[i].height =
        std::clamp(bars_[i].height, kMinBarHeight, max_bar_height);
  }
}

float WaveformView::GetTargetHeightForBar(size_t index,
                                          float min_height,
                                          float max_height) const {
  switch (state_) {
    case UiState::kInitializing:
    case UiState::kTranscribing: {
      const size_t center_bar_index = GetCenterBarIndex();
      const size_t dist = index > center_bar_index ? index - center_bar_index
                                                   : center_bar_index - index;
      const float raw_level = audio_history_[dist];
      const float raw_height =
          CalculateBarHeight(min_height, max_height, raw_level);
      const float taper =
          1.0f - (static_cast<float>(dist) / (center_bar_index + 1.0f));
      return std::max(min_height, raw_height * taper);
    }
    case UiState::kInactive:
    case UiState::kFinalizing:
      return min_height;
  }
}

BEGIN_METADATA(WaveformView)
END_METADATA

}  // namespace dictation
