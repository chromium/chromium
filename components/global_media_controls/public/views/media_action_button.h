// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_VIEWS_MEDIA_ACTION_BUTTON_H_
#define COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_VIEWS_MEDIA_ACTION_BUTTON_H_

#include "ui/views/controls/button/image_button.h"

namespace global_media_controls {

// Media action button ID casted from media session action starts from 0, so we
// use -1 for buttons not associated with any media session action, such as the
// start casting button.
inline constexpr int kEmptyMediaActionButtonId = -1;

// Template of media action buttons that are displayed on media item UIs.
class COMPONENT_EXPORT(GLOBAL_MEDIA_CONTROLS) MediaActionButton
    : public views::ImageButton {
  METADATA_HEADER(MediaActionButton, views::ImageButton)

 public:
  MediaActionButton(PressedCallback callback,
                    int button_id,
                    int tooltip_text_id,
                    int icon_size,
                    const gfx::VectorIcon& vector_icon,
                    gfx::Size button_size,
                    ui::ColorId foreground_color_id,
                    ui::ColorId foreground_disabled_color_id,
                    ui::ColorId focus_ring_color_id);
  MediaActionButton(const MediaActionButton&) = delete;
  MediaActionButton& operator=(const MediaActionButton&) = delete;
  ~MediaActionButton() override;

  // Update the button when its state has updated.
  void Update(int button_id,
              const gfx::VectorIcon& vector_icon,
              int tooltip_text_id,
              ui::ColorId foreground_color_id);

  // Update the button tooltip text.
  void UpdateText(int tooltip_text_id);

  // Update the button icon.
  void UpdateIcon(const gfx::VectorIcon& vector_icon);

 private:
  const int icon_size_;
  ui::ColorId foreground_color_id_;
  const ui::ColorId foreground_disabled_color_id_;
};

}  // namespace global_media_controls

#endif  // COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_VIEWS_MEDIA_ACTION_BUTTON_H_
