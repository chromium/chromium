// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_MENU_ACTION_APP_MENU_ZOOM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_APP_MENU_ACTION_APP_MENU_ZOOM_VIEW_H_

#include <optional>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "ui/actions/actions.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/color/color_id.h"
#include "ui/views/layout/box_layout_view.h"

class BrowserWindowInterface;

namespace views {
class ActionViewController;
class ImageButton;
}  // namespace views

// Custom horizontal container view for Zoom controls in ActionAppMenu.
class ActionAppMenuZoomView : public views::BoxLayoutView {
  METADATA_HEADER(ActionAppMenuZoomView, views::BoxLayoutView)

 public:
  ActionAppMenuZoomView(
      BrowserWindowInterface* browser_window_interface,
      views::ActionViewController* action_view_controller,
      base::flat_map<int, raw_ptr<actions::ActionItem>>& command_to_action_map,
      actions::BaseAction* zoom_row_action_item);

  ActionAppMenuZoomView(const ActionAppMenuZoomView&) = delete;
  ActionAppMenuZoomView& operator=(const ActionAppMenuZoomView&) = delete;
  ~ActionAppMenuZoomView() override;

 private:
  // Creates the zoom child controls (-, +, and fullscreen buttons), for the
  // zoom menu item.
  void BuildZoomChildControls(
      actions::BaseAction* zoom_row_action_item,
      views::ActionViewController* action_view_controller,
      base::flat_map<int, raw_ptr<actions::ActionItem>>& command_to_action_map);

  // Helper function that creates and returns a button for the zoom menu item
  // with the appropriate stylings.
  std::unique_ptr<views::ImageButton> CreateZoomButton(
      actions::ActionItem* zoom_child);

  raw_ptr<BrowserWindowInterface> browser_window_interface_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_MENU_ACTION_APP_MENU_ZOOM_VIEW_H_
