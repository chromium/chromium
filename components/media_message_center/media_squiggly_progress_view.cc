// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_message_center/media_squiggly_progress_view.h"

#include "base/i18n/number_formatting.h"
#include "cc/paint/paint_flags.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/widget/widget.h"

namespace media_message_center {

namespace {

// The width of stroke to paint the progress foreground and background lines.
constexpr int kProgressStrokeWidth = 2;

// The height of squiggly progress that user can click to seek to a new media
// position. This is slightly larger than the painted progress height.
constexpr int kProgressClickHeight = 10;

// Defines the x of where the painting of squiggly progress should start since
// we own the OnPaint() function.
constexpr int kWidthInset = 8;

// Defines the wave size of the squiggly progress.
constexpr int kProgressWavelength = 32;
constexpr int kProgressAmplitude = 2;

// The radius of the circle at the end of the foreground squiggly progress. This
// should be larger than the progress amplitude to cover it.
constexpr int kProgressCircleRadius = 5;

// Progress wave speed in pixels per second.
constexpr int kProgressPhaseSpeed = 28;

// Defines how long the animation for progress transitioning between squiggly
// and straight lines will take.
constexpr base::TimeDelta kSlideAnimationDuration = base::Milliseconds(200);

// Defines how frequently the progress will be updated.
constexpr base::TimeDelta kProgressUpdateFrequency = base::Milliseconds(100);

// Used to set the height of the whole view.
constexpr auto kInsideInsets = gfx::Insets::VH(16, 0);

int RoundToPercent(double fractional_value) {
  return static_cast<int>(fractional_value * 100);
}

}  // namespace

MediaSquigglyProgressView::MediaSquigglyProgressView(
    ui::ColorId foreground_color_id,
    ui::ColorId background_color_id,
    base::RepeatingCallback<void(double)> seek_callback)
    : foreground_color_id_(foreground_color_id),
      background_color_id_(background_color_id),
      seek_callback_(std::move(seek_callback)),
      slide_animation_(this) {
  SetInsideBorderInsets(kInsideInsets);
  SetFlipCanvasOnPaintForRTLUI(true);
  SetAccessibilityProperties(ax::mojom::Role::kProgressIndicator);

  slide_animation_.SetSlideDuration(kSlideAnimationDuration);
}

MediaSquigglyProgressView::~MediaSquigglyProgressView() = default;

///////////////////////////////////////////////////////////////////////////////
// gfx::AnimationDelegate implementations:

void MediaSquigglyProgressView::AnimationProgressed(
    const gfx::Animation* animation) {
  CHECK(animation == &slide_animation_);
  progress_amp_fraction_ = animation->GetCurrentValue();
  OnPropertyChanged(&progress_amp_fraction_, views::kPropertyEffectsPaint);
}

///////////////////////////////////////////////////////////////////////////////
// views::View implementations:

void MediaSquigglyProgressView::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  View::GetAccessibleNodeData(node_data);
  node_data->SetValue(base::FormatPercent(RoundToPercent(current_value_)));
}

void MediaSquigglyProgressView::VisibilityChanged(View* starting_from,
                                                  bool is_visible) {
  MaybeNotifyAccessibilityValueChanged();
}

void MediaSquigglyProgressView::AddedToWidget() {
  MaybeNotifyAccessibilityValueChanged();
}

void MediaSquigglyProgressView::OnPaint(gfx::Canvas* canvas) {
  const auto* color_provider = GetColorProvider();
  const int view_width = GetContentsBounds().width() - kWidthInset * 2;
  const int view_height = GetContentsBounds().height();
  const int progress_width =
      static_cast<int>(view_width * std::min(current_value_, 1.0) + 0.5);

  // Create the paint flags which will be reused for painting.
  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(kProgressStrokeWidth);
  flags.setAntiAlias(true);
  flags.setColor(color_provider->GetColor(foreground_color_id_));

  // Translate the canvas to avoid painting anything in the width inset.
  canvas->Save();
  canvas->Translate(gfx::Vector2d(kWidthInset, 0));

  // Create a foreground squiggly progress path longer than the required length
  // and truncate it later in canvas. If the media is paused, this will become
  // a straight line.
  SkPath progress_path;
  int current_x = -phase_offset_ - kProgressWavelength / 2;
  int current_amp =
      static_cast<int>(kProgressAmplitude * progress_amp_fraction_);
  progress_path.moveTo(current_x, 0);
  while (current_x <= progress_width) {
    int mid_x = current_x + kProgressWavelength / 4;
    int next_x = current_x + kProgressWavelength / 2;
    int next_amp = -current_amp;
    progress_path.cubicTo(mid_x, current_amp, mid_x, next_amp, next_x,
                          next_amp);
    current_x = next_x;
    current_amp = next_amp;
  }
  progress_path.offset(0, view_height / 2);

  // Paint the foreground squiggly progress in a clipped rect.
  canvas->Save();
  canvas->ClipRect(gfx::Rect(0, 0, progress_width, view_height));
  canvas->DrawPath(progress_path, flags);
  canvas->Restore();

  // Paint the progress circle indicator.
  flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->DrawCircle(gfx::Point(progress_width, view_height / 2),
                     kProgressCircleRadius, flags);

  // Paint the background straight line.
  if (progress_width + kProgressCircleRadius < view_width) {
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setColor(color_provider->GetColor(background_color_id_));
    canvas->DrawLine(
        gfx::PointF(progress_width + kProgressCircleRadius, view_height / 2),
        gfx::PointF(view_width, view_height / 2), flags);
  }
  canvas->Restore();
}

bool MediaSquigglyProgressView::OnMousePressed(const ui::MouseEvent& event) {
  if (is_live_ || !event.IsOnlyLeftMouseButton() ||
      !IsValidSeekPosition(event.x(), event.y())) {
    return false;
  }

  HandleSeeking(event.location());
  return true;
}

void MediaSquigglyProgressView::OnGestureEvent(ui::GestureEvent* event) {
  if (is_live_ || event->type() != ui::ET_GESTURE_TAP ||
      !IsValidSeekPosition(event->x(), event->y())) {
    return;
  }

  HandleSeeking(event->location());
  event->SetHandled();
}

///////////////////////////////////////////////////////////////////////////////
// MediaSquigglyProgressView implementations:

void MediaSquigglyProgressView::UpdateProgress(
    const media_session::MediaPosition& media_position) {
  is_live_ = media_position.duration().is_max();

  bool is_paused = media_position.playback_rate() == 0;
  if (is_paused_ != is_paused) {
    if (is_paused_) {
      // Progress path becomes squiggly as media starts to play.
      slide_animation_.Reset(0);
      slide_animation_.Show();
    } else {
      // Progress path becomes straight as media stops playing.
      slide_animation_.Reset(1);
      slide_animation_.Hide();
    }
    is_paused_ = is_paused;
  }

  // If the media is paused and |update_progress_timer_| is still running, stop
  // the timer.
  if (is_paused_ && update_progress_timer_.IsRunning()) {
    update_progress_timer_.Stop();
  }

  const base::TimeDelta current_position = media_position.GetPosition();
  const base::TimeDelta duration = media_position.duration();
  const double progress_value =
      (is_live_ || duration.is_zero() || current_position > duration)
          ? 1.0
          : current_position / duration;
  if (current_value_ != progress_value) {
    current_value_ = progress_value;
    MaybeNotifyAccessibilityValueChanged();
    OnPropertyChanged(&current_value_, views::kPropertyEffectsPaint);
  }

  if (!is_paused_) {
    if (!slide_animation_.is_animating()) {
      // Update the progress wavelength phase offset to create wave animation.
      phase_offset_ +=
          static_cast<int>(kProgressUpdateFrequency.InMillisecondsF() / 1000 *
                           kProgressPhaseSpeed);
      phase_offset_ %= kProgressWavelength;
      OnPropertyChanged(&phase_offset_, views::kPropertyEffectsPaint);
    }

    update_progress_timer_.Start(
        FROM_HERE, kProgressUpdateFrequency,
        base::BindRepeating(&MediaSquigglyProgressView::UpdateProgress,
                            base::Unretained(this), media_position));
  }
}

void MediaSquigglyProgressView::MaybeNotifyAccessibilityValueChanged() {
  if (!GetWidget() || !GetWidget()->IsVisible() ||
      RoundToPercent(current_value_) == last_announced_percentage_) {
    return;
  }
  last_announced_percentage_ = RoundToPercent(current_value_);
  NotifyAccessibilityEvent(ax::mojom::Event::kValueChanged, true);
}

void MediaSquigglyProgressView::HandleSeeking(const gfx::Point& location) {
  double seek_to_progress = static_cast<double>(location.x() - kWidthInset) /
                            (GetContentsBounds().width() - kWidthInset * 2);
  seek_callback_.Run(seek_to_progress);
}

bool MediaSquigglyProgressView::IsValidSeekPosition(int x, int y) {
  return (kWidthInset <= x) &&
         (x <= GetContentsBounds().width() - kWidthInset) &&
         ((GetContentsBounds().height() - kProgressClickHeight) / 2 <= y) &&
         (y <= (GetContentsBounds().height() + kProgressClickHeight) / 2);
}

// Helper functions for testing:
double MediaSquigglyProgressView::current_value_for_testing() const {
  return current_value_;
}

bool MediaSquigglyProgressView::is_paused_for_testing() const {
  return is_paused_;
}

bool MediaSquigglyProgressView::is_live_for_testing() const {
  return is_live_;
}

BEGIN_METADATA(MediaSquigglyProgressView, views::View)
END_METADATA

}  // namespace media_message_center
