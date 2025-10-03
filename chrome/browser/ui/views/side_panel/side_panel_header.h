// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_HEADER_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_HEADER_H_

#include "ui/views/view.h"

namespace views {
class ImageView;
class Label;
class ImageButton;
class ToggleImageButton;
}  // namespace views

// SidePanelHeader is a view with custom Layout override to draw on top of the
// Side Panel border. The header is added as a separate view over the side panel
// border so it can process events since the border cannot process events.
class SidePanelHeader : public views::View {
 public:
  using TogglePinStateCallback = base::RepeatingClosure;
  using OpenInNewTabCallback = base::RepeatingClosure;
  using OpenMoreInfoMenuCallback = base::RepeatingClosure;
  using CloseSidePanelCallback = base::RepeatingClosure;

  SidePanelHeader(TogglePinStateCallback toggle_pin_state_callback,
                  OpenInNewTabCallback open_in_new_tab_callback,
                  OpenMoreInfoMenuCallback open_more_info_menu_callback,
                  CloseSidePanelCallback close_side_panel_callback);

  ~SidePanelHeader() override;

  void Layout(PassKey) override;

  views::ImageView* panel_icon() { return panel_icon_; }

  views::Label* panel_title() { return panel_title_; }

  views::ToggleImageButton* header_pin_button() { return header_pin_button_; }

  views::ImageButton* header_more_info_button() {
    return header_more_info_button_;
  }

 private:
  raw_ptr<views::ImageView> panel_icon_ = nullptr;
  raw_ptr<views::Label> panel_title_ = nullptr;
  raw_ptr<views::ToggleImageButton> header_pin_button_ = nullptr;
  raw_ptr<views::ImageButton> header_open_in_new_tab_button_ = nullptr;
  raw_ptr<views::ImageButton> header_more_info_button_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_HEADER_H_
