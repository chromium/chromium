// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_MENU_ACTION_APP_MENU_ZOOM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_APP_MENU_ACTION_APP_MENU_ZOOM_VIEW_H_

#include <optional>
#include <vector>

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/zoom/zoom_observer.h"
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

namespace zoom {
class ZoomController;
}

// Custom horizontal container view for Zoom controls in ActionAppMenu.
class ActionAppMenuZoomView : public views::BoxLayoutView,
                              public zoom::ZoomObserver {
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

  // zoom::ZoomObserver:
  void OnZoomChanged(
      const zoom::ZoomController::ZoomChangedEventData& data) override;
  void OnZoomControllerDestroyed(
      zoom::ZoomController* zoom_controller) override;

  views::Label* zoom_label_for_testing() const { return zoom_label_; }

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

  // Updates the zoom label text and enables/disables the zoom buttons based on
  // the current zoom percent.
  void UpdateZoomControls();

  // Updates the fullscreen button's enabled state, tooltip, and accessible name
  // based on whether fullscreen is supported and whether the window is in
  // fullscreen mode.
  void UpdateFullScreenButton();

  // Returns the active web contents, or nullptr if unavailable.
  content::WebContents* GetActiveWebContents() const;

  // Returns the current zoom percentage for the active web contents.
  int GetCurrentZoomPercent() const;

  raw_ptr<BrowserWindowInterface> browser_window_interface_;
  raw_ptr<views::Label> zoom_label_ = nullptr;
  raw_ptr<views::ImageButton> zoom_minus_button_ = nullptr;
  raw_ptr<views::ImageButton> zoom_plus_button_ = nullptr;
  raw_ptr<views::ImageButton> zoom_fullscreen_button_ = nullptr;
  base::ScopedObservation<zoom::ZoomController, zoom::ZoomObserver>
      zoom_observation_{this};
  std::vector<base::CallbackListSubscription> button_subscriptions_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_MENU_ACTION_APP_MENU_ZOOM_VIEW_H_
