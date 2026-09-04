// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_ORGANIZER_ORGANIZER_PANEL_CONTROLS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_ORGANIZER_ORGANIZER_PANEL_CONTROLS_VIEW_H_

#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/layout/flex_layout_view.h"

namespace gfx {
class Point;
}  // namespace gfx

namespace actions {
class ActionItem;
}  // namespace actions

namespace views {
class LabelButton;
}  // namespace views

// Contains the controls for the organizer panel, including the
// button to close the panel.
class OrganizerPanelControlsView : public views::FlexLayoutView {
  METADATA_HEADER(OrganizerPanelControlsView, views::View)

 public:
  explicit OrganizerPanelControlsView(actions::ActionItem* root_action_item);
  OrganizerPanelControlsView(const OrganizerPanelControlsView&) = delete;
  OrganizerPanelControlsView& operator=(const OrganizerPanelControlsView&) =
      delete;
  ~OrganizerPanelControlsView() override;

  bool IsPositionInWindowCaption(const gfx::Point& point);

  // Should be called when the text of the action item for toggling the panel
  // state changes.
  void UpdateTooltipText();

  // Sets the opacity of any buttons in this view.
  void SetButtonOpacity(float opacity);

  views::LabelButton* organizer_button_for_testing() {
    return organizer_button_;
  }

 private:
  void OnCloseButtonPressed();

  raw_ptr<views::LabelButton> organizer_button_ = nullptr;
  raw_ptr<actions::ActionItem> toggle_organizer_panel_action_item_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_ORGANIZER_ORGANIZER_PANEL_CONTROLS_VIEW_H_
