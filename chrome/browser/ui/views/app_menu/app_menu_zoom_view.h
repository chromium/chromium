// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_ZOOM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_ZOOM_VIEW_H_

#include <optional>
#include <vector>

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "ui/actions/actions.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/color/color_id.h"
#include "ui/views/layout/box_layout_view.h"

class BrowserWindowInterface;

namespace content {
class WebContents;
}  // namespace content

namespace views {
class ActionViewController;
class ImageButton;
class Label;
}  // namespace views

// Custom horizontal container view for Zoom controls in ActionAppMenu.
class AppMenuZoomView : public views::BoxLayoutView {
  METADATA_HEADER(AppMenuZoomView, views::BoxLayoutView)

 public:
  AppMenuZoomView(
      BrowserWindowInterface* browser_window_interface,
      views::ActionViewController* action_view_controller,
      base::flat_map<int, raw_ptr<actions::BaseAction>>& command_to_action_map,
      actions::BaseAction* zoom_row_action_item);

  AppMenuZoomView(const AppMenuZoomView&) = delete;
  AppMenuZoomView& operator=(const AppMenuZoomView&) = delete;
  ~AppMenuZoomView() override;

  views::Label* zoom_label_for_testing() const { return zoom_label_; }

 private:
  // Returns the maximum pixel width required to display any zoom percentage
  // string for the zoom label.
  int GetZoomLabelMaxWidth() const;

  // Creates the zoom child controls (-, %, +, and fullscreen buttons), for the
  // zoom menu item.
  void BuildZoomChildControls(
      actions::BaseAction* zoom_row_action_item,
      views::ActionViewController* action_view_controller,
      base::flat_map<int, raw_ptr<actions::BaseAction>>& command_to_action_map);

  // Helper function that creates and returns a button for the zoom menu item
  // with the appropriate stylings.
  std::unique_ptr<views::ImageButton> CreateZoomButton(
      actions::ActionItem* zoom_child);

  // Returns the active web contents, or nullptr if unavailable.
  content::WebContents* GetActiveWebContents() const;

  raw_ptr<BrowserWindowInterface> browser_window_interface_;
  raw_ptr<views::Label> zoom_label_ = nullptr;
  base::CallbackListSubscription zoom_label_subscription_;
  std::vector<base::CallbackListSubscription> button_subscriptions_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_ZOOM_VIEW_H_
