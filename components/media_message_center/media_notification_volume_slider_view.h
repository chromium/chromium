// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_VOLUME_SLIDER_VIEW_H_
#define COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_VOLUME_SLIDER_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/view.h"

namespace media_message_center {

class COMPONENT_EXPORT(MEDIA_MESSAGE_CENTER) MediaNotificationVolumeSliderView
    : public views::View {
  METADATA_HEADER(MediaNotificationVolumeSliderView, views::View)

 public:
  explicit MediaNotificationVolumeSliderView(
      base::RepeatingCallback<void(float)> set_volume_callback);
  ~MediaNotificationVolumeSliderView() override;

  void UpdateColor(SkColor foreground, SkColor background);
  void SetVolume(float volume);
  void SetMute(bool mute);

  // views::View
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool OnMouseWheel(const ui::MouseWheelEvent& event) override;
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  void HandleMouseOrGestureEvent(float location_x);
  void HandleVolumeChangeWithDelta(bool volume_up, float delta);

  float volume_ = 0.0;
  bool mute_ = false;
  SkColor foreground_color_ = gfx::kPlaceholderColor;
  SkColor background_color_ = gfx::kPlaceholderColor;

  const base::RepeatingCallback<void(float)> set_volume_callback_;
};

}  // namespace media_message_center

#endif  // COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_VOLUME_SLIDER_VIEW_H_
