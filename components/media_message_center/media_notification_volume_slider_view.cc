// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_message_center/media_notification_volume_slider_view.h"

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/controls/focus_ring.h"

namespace media_message_center {

namespace {
constexpr int kThumbRadius = 4;
constexpr int kSliderHeight = 2;
constexpr float kScrollVolumeDelta = 0.1;
constexpr float kKeyVolumeDelta = 0.05;
}  // anonymous namespace

MediaNotificationVolumeSliderView::MediaNotificationVolumeSliderView(
    base::RepeatingCallback<void(float)> set_volume_callback)
    : set_volume_callback_(set_volume_callback) {
  views::FocusRing::Install(this);
  SetFocusBehavior(FocusBehavior::ALWAYS);
}

MediaNotificationVolumeSliderView::~MediaNotificationVolumeSliderView() =
    default;

void MediaNotificationVolumeSliderView::UpdateColor(SkColor foreground,
                                                    SkColor background) {
  foreground_color_ = foreground;
  background_color_ = background;
  SchedulePaint();
}

void MediaNotificationVolumeSliderView::SetVolume(float volume) {
  volume_ = std::min(1.0f, std::max(0.0f, volume));
  SchedulePaint();
}

void MediaNotificationVolumeSliderView::SetMute(bool mute) {
  mute_ = mute;
  SchedulePaint();
}

bool MediaNotificationVolumeSliderView::OnMousePressed(
    const ui::MouseEvent& event) {
  HandleMouseOrGestureEvent(event.x());
  return true;
}

bool MediaNotificationVolumeSliderView::OnMouseDragged(
    const ui::MouseEvent& event) {
  HandleMouseOrGestureEvent(event.x());
  return true;
}

void MediaNotificationVolumeSliderView::OnGestureEvent(
    ui::GestureEvent* event) {
  if (event->type() == ui::EventType::kGestureTap) {
    HandleMouseOrGestureEvent(event->x());
  }
}

bool MediaNotificationVolumeSliderView::OnKeyPressed(
    const ui::KeyEvent& event) {
  if (event.key_code() == ui::KeyboardCode::VKEY_UP ||
      event.key_code() == ui::KeyboardCode::VKEY_RIGHT) {
    HandleVolumeChangeWithDelta(true /* volume_up */, kKeyVolumeDelta);
    return true;
  }

  if (event.key_code() == ui::KeyboardCode::VKEY_DOWN ||
      event.key_code() == ui::KeyboardCode::VKEY_LEFT) {
    HandleVolumeChangeWithDelta(false /* volume_up */, kKeyVolumeDelta);
    return true;
  }

  return false;
}

bool MediaNotificationVolumeSliderView::OnMouseWheel(
    const ui::MouseWheelEvent& event) {
  if (event.y_offset() == 0)
    return false;

  HandleVolumeChangeWithDelta(event.y_offset() > 0 /* volume_up */,
                              kScrollVolumeDelta);
  return true;
}

void MediaNotificationVolumeSliderView::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);
  float volume = mute_ ? 0.0 : volume_;

  gfx::Rect content_bound = GetContentsBounds();
  int offset_y = (content_bound.height() - kSliderHeight) / 2;

  // Draw background bar taking entire content width and |kSliderHeight|.
  SkPath background_path;
  background_path.addRoundRect(
      gfx::RectToSkRect(
          gfx::Rect(0, offset_y, content_bound.width(), kSliderHeight)),
      kSliderHeight / 2, kSliderHeight / 2);

  cc::PaintFlags background_flags;
  background_flags.setStyle(cc::PaintFlags::kFill_Style);
  background_flags.setAntiAlias(true);
  background_flags.setColor(background_color_);
  canvas->DrawPath(background_path, background_flags);

  SkPath foreground_path;

  // The effective length of the volume slider bar is the content width minus
  // the thumb size because we want the thumb completely stays inside the
  // slider.
  int foreground_width =
      static_cast<int>((content_bound.width() - 2 * kThumbRadius) * volume) +
      kThumbRadius;
  foreground_path.addRoundRect(
      gfx::RectToSkRect(
          gfx::Rect(0, offset_y, foreground_width, kSliderHeight)),
      kSliderHeight / 2, kSliderHeight / 2);

  cc::PaintFlags foreground_flags;
  foreground_flags.setStyle(cc::PaintFlags::kFill_Style);
  foreground_flags.setAntiAlias(true);
  foreground_flags.setColor(foreground_color_);
  canvas->DrawPath(foreground_path, foreground_flags);

  // Draw thumb.
  int thumb_offset_x = foreground_width - kThumbRadius;
  int thumb_offset_y = (content_bound.height() - 2 * kThumbRadius) / 2;
  SkPath thumb_path;
  thumb_path.addRoundRect(
      gfx::RectToSkRect(gfx::Rect(thumb_offset_x, thumb_offset_y,
                                  2 * kThumbRadius, 2 * kThumbRadius)),
      kThumbRadius, kThumbRadius);

  canvas->DrawPath(thumb_path, foreground_flags);
}

void MediaNotificationVolumeSliderView::HandleMouseOrGestureEvent(
    float location_x) {
  float new_volume = (location_x - kThumbRadius) /
                     (GetContentsBounds().width() - 2 * kThumbRadius);
  new_volume = std::min(1.0f, std::max(0.0f, new_volume));
  set_volume_callback_.Run(new_volume);
}

void MediaNotificationVolumeSliderView::HandleVolumeChangeWithDelta(
    bool volume_up,
    float delta) {
  float new_volume = volume_up ? volume_ + delta : volume_ - delta;
  new_volume = std::min(1.0f, std::max(0.0f, new_volume));
  set_volume_callback_.Run(new_volume);
}

BEGIN_METADATA(MediaNotificationVolumeSliderView)
END_METADATA

}  // namespace media_message_center
