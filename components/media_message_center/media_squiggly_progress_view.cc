// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_message_center/media_squiggly_progress_view.h"

#include "base/i18n/number_formatting.h"
#include "base/i18n/rtl.h"
#include "cc/paint/paint_flags.h"
#include "components/strings/grit/components_strings.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/widget/widget.h"

namespace media_message_center {

namespace {

// The width of stroke to paint the progress foreground and background lines,
// and also the focus ring.
constexpr int kStrokeWidth = 2;

// The height of squiggly progress that user can click to seek to a new media
// position. This is slightly larger than the painted progress height.
constexpr int kProgressClickHeight = 16;

// Defines the x of where the painting of squiggly progress should start since
// we own the OnPaint() function.
constexpr int kWidthInset = 8;

// Defines the wave size of the squiggly progress.
constexpr int kProgressWavelength = 32;
constexpr int kProgressAmplitude = 2;

// Progress wave speed in pixels per second.
constexpr int kProgressPhaseSpeed = 28;

// The size of the rounded rectangle indicator at the end of the foreground
// squiggly progress.
constexpr gfx::SizeF kProgressIndicatorSize = gfx::SizeF(6, 14);

// The radius of the rounded rectangle indicator.
constexpr float kProgressIndicatorRadius = 3.0;

// Defines how long the animation for progress transitioning between squiggly
// and straight lines will take.
constexpr base::TimeDelta kSlideAnimationDuration = base::Milliseconds(200);

// Defines how frequently the progress will be updated.
constexpr base::TimeDelta kProgressUpdateFrequency = base::Milliseconds(100);

// Used to set the height of the whole view.
constexpr auto kInsideInsets = gfx::Insets::VH(16, 0);

// Defines the radius of the focus ring around squiggly progress.
constexpr float kFocusRingRadius = 18.0f;

// Defines how much the current media position will change for increment.
constexpr base::TimeDelta kCurrentPositionChange = base::Seconds(5);

int RoundToPercent(double fractional_value) {
  return static_cast<int>(fractional_value * 100);
}

}  // namespace

MediaSquigglyProgressView::MediaSquigglyProgressView(
    ui::ColorId playing_foreground_color_id,
    ui::ColorId playing_background_color_id,
    ui::ColorId paused_foreground_color_id,
    ui::ColorId paused_background_color_id,
    ui::ColorId focus_ring_color_id,
    base::RepeatingCallback<void(bool)> dragging_callback,
    base::RepeatingCallback<void(double)> seek_callback)
    : playing_foreground_color_id_(playing_foreground_color_id),
      playing_background_color_id_(playing_background_color_id),
      paused_foreground_color_id_(paused_foreground_color_id),
      paused_background_color_id_(paused_background_color_id),
      focus_ring_color_id_(focus_ring_color_id),
      dragging_callback_(std::move(dragging_callback)),
      seek_callback_(std::move(seek_callback)),
      slide_animation_(this) {
  SetInsideBorderInsets(kInsideInsets);
  SetFlipCanvasOnPaintForRTLUI(true);
  SetAccessibilityProperties(
      ax::mojom::Role::kProgressIndicator,
      l10n_util::GetStringUTF16(
          IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_TIME_SCRUBBER));
  SetFocusBehavior(FocusBehavior::ALWAYS);

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

gfx::Size MediaSquigglyProgressView::CalculatePreferredSize() const {
  return GetContentsBounds().size();
}

void MediaSquigglyProgressView::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  View::GetAccessibleNodeData(node_data);
  node_data->SetValue(base::FormatPercent(RoundToPercent(current_value_)));
  node_data->AddAction(ax::mojom::Action::kIncrement);
  node_data->AddAction(ax::mojom::Action::kDecrement);
}

bool MediaSquigglyProgressView::HandleAccessibleAction(
    const ui::AXActionData& action_data) {
  double new_value;
  if (action_data.action == ax::mojom::Action::kIncrement) {
    new_value = CalculateNewValue(current_position_ + kCurrentPositionChange);
  } else if (action_data.action == ax::mojom::Action::kDecrement) {
    new_value = CalculateNewValue(current_position_ - kCurrentPositionChange);
  } else {
    return views::View::HandleAccessibleAction(action_data);
  }
  if (new_value != current_value_) {
    seek_callback_.Run(new_value);
    return true;
  }
  return false;
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
  const int progress_width = static_cast<int>(view_width * current_value_);

  // Create the paint flags which will be reused for painting.
  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(kStrokeWidth);
  flags.setAntiAlias(true);
  flags.setColor(color_provider->GetColor(
      is_paused_ ? paused_foreground_color_id_ : playing_foreground_color_id_));

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

  // Paint the progress rectangle indicator.
  flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->DrawRoundRect(
      gfx::RectF(
          gfx::PointF(progress_width - kProgressIndicatorSize.width() / 2,
                      (view_height - kProgressIndicatorSize.height()) / 2),
          kProgressIndicatorSize),
      kProgressIndicatorRadius, flags);

  // Paint the background straight line.
  if (progress_width + kProgressIndicatorSize.width() / 2 < view_width) {
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setColor(
        color_provider->GetColor(is_paused_ ? paused_background_color_id_
                                            : playing_background_color_id_));
    canvas->DrawLine(
        gfx::PointF(progress_width + kProgressIndicatorSize.width() / 2,
                    view_height / 2),
        gfx::PointF(view_width, view_height / 2), flags);
  }
  canvas->Restore();

  // Paint the focus ring in the end on the original canvas.
  if (HasFocus()) {
    cc::PaintFlags border;
    border.setStyle(cc::PaintFlags::kStroke_Style);
    border.setStrokeWidth(kStrokeWidth);
    border.setAntiAlias(true);
    border.setColor(color_provider->GetColor(focus_ring_color_id_));
    canvas->DrawRoundRect(
        gfx::Rect(kStrokeWidth, kStrokeWidth,
                  GetContentsBounds().width() - kStrokeWidth * 2,
                  GetContentsBounds().height() - kStrokeWidth * 2),
        kFocusRingRadius, border);
  }
}

void MediaSquigglyProgressView::OnFocus() {
  views::View::OnFocus();
  SchedulePaint();
}

void MediaSquigglyProgressView::OnBlur() {
  views::View::OnBlur();
  SchedulePaint();
}

bool MediaSquigglyProgressView::OnMousePressed(const ui::MouseEvent& event) {
  if (is_live_ || !event.IsOnlyLeftMouseButton() ||
      !IsValidSeekPosition(event.x(), event.y())) {
    return false;
  }

  OnProgressDragStarted();
  HandleSeeking(event.x());
  return true;
}

bool MediaSquigglyProgressView::OnMouseDragged(const ui::MouseEvent& event) {
  HandleSeeking(event.x());
  return true;
}

void MediaSquigglyProgressView::OnMouseReleased(const ui::MouseEvent& event) {
  HandleSeeking(event.x());
  OnProgressDragEnded();
}

bool MediaSquigglyProgressView::OnKeyPressed(const ui::KeyEvent& event) {
  if (is_live_) {
    return false;
  }
  int direction = 1;
  switch (event.key_code()) {
    case ui::VKEY_LEFT:
      direction = base::i18n::IsRTL() ? 1 : -1;
      break;
    case ui::VKEY_RIGHT:
      direction = base::i18n::IsRTL() ? -1 : 1;
      break;
    case ui::VKEY_UP:
      direction = 1;
      break;
    case ui::VKEY_DOWN:
      direction = -1;
      break;
    default:
      return false;
  }
  double new_value =
      CalculateNewValue(current_position_ + direction * kCurrentPositionChange);
  if (new_value != current_value_) {
    seek_callback_.Run(new_value);
    return true;
  }
  return false;
}

void MediaSquigglyProgressView::OnGestureEvent(ui::GestureEvent* event) {
  if (is_live_ || !IsValidSeekPosition(event->x(), event->y())) {
    return;
  }

  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN:
      OnProgressDragStarted();
      [[fallthrough]];
    case ui::ET_GESTURE_SCROLL_BEGIN:
    case ui::ET_GESTURE_SCROLL_UPDATE:
      HandleSeeking(event->x());
      event->SetHandled();
      break;
    case ui::ET_GESTURE_END:
      HandleSeeking(event->x());
      event->SetHandled();
      if (event->details().touch_points() <= 1) {
        OnProgressDragEnded();
      }
      break;
    default:
      break;
  }
}

///////////////////////////////////////////////////////////////////////////////
// MediaSquigglyProgressView implementations:

void MediaSquigglyProgressView::UpdateProgress(
    const media_session::MediaPosition& media_position) {
  // Always stop the timer since it may have been triggered by an old media
  // position and the timer will be re-started if needed.
  update_progress_timer_.Stop();

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

  current_position_ = media_position.GetPosition();
  media_duration_ = media_position.duration();
  is_live_ = media_duration_.is_max();

  double new_value = CalculateNewValue(current_position_);
  if (new_value != current_value_) {
    current_value_ = new_value;
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
        base::BindOnce(&MediaSquigglyProgressView::UpdateProgress,
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

void MediaSquigglyProgressView::OnProgressDragStarted() {
  // Pause the media only once if it is playing when the user starts dragging
  // the progress line.
  if (!is_paused_ && !paused_for_dragging_) {
    dragging_callback_.Run(/*pause=*/true);
    paused_for_dragging_ = true;
  }
}

void MediaSquigglyProgressView::OnProgressDragEnded() {
  // Un-pause the media when the user finishes dragging the progress line if the
  // media was playing before dragging.
  if (paused_for_dragging_) {
    dragging_callback_.Run(/*pause=*/false);
    paused_for_dragging_ = false;
  }
}

void MediaSquigglyProgressView::HandleSeeking(double location) {
  double view_width = GetContentsBounds().width() - kWidthInset * 2;
  double seek_to_progress =
      std::min(view_width, std::max(0.0, location - kWidthInset)) / view_width;
  if (base::i18n::IsRTL()) {
    seek_to_progress = 1.0 - seek_to_progress;
  }
  seek_callback_.Run(seek_to_progress);
}

double MediaSquigglyProgressView::CalculateNewValue(
    base::TimeDelta new_position) {
  double new_value = 0.0;
  if (new_position >= media_duration_ || is_live_) {
    new_value = 1.0;
  } else if (media_duration_.is_positive() && new_position.is_positive()) {
    new_value = new_position / media_duration_;
  }
  return new_value;
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
