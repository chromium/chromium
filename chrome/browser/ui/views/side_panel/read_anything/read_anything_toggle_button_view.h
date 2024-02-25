// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_TOGGLE_BUTTON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_TOGGLE_BUTTON_VIEW_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_button_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/button/image_button.h"

class ReadAnythingToggleButtonView : public views::ToggleImageButton {
  METADATA_HEADER(ReadAnythingToggleButtonView, views::ToggleImageButton)
 public:
  ReadAnythingToggleButtonView(bool initial_toggled_state,
                               views::ImageButton::PressedCallback callback,
                               const std::u16string& toggled_tooltip,
                               const std::u16string& untoggled_tooltip);
  ReadAnythingToggleButtonView(const ReadAnythingToggleButtonView&) = delete;
  ReadAnythingToggleButtonView& operator=(const ReadAnythingToggleButtonView&) =
      delete;
  ~ReadAnythingToggleButtonView() override;

  void UpdateIcons(const gfx::VectorIcon& toggled_icon,
                   const gfx::VectorIcon& untoggled_icon,
                   int icon_size,
                   ui::ColorId icon_color,
                   ui::ColorId focus_ring_color);

  bool IsGroupFocusTraversable() const override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_TOGGLE_BUTTON_VIEW_H_
