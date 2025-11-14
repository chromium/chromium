// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LENS_OVERLAY_HOMEWORK_PAGE_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LENS_OVERLAY_HOMEWORK_PAGE_ACTION_CONTROLLER_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class Profile;
class ScopedWindowCallToAction;

// Controller for the Lens Overlay "Homework" page action chip that appears in
// the omnibox.
//
// This controller is responsible for the complete lifecycle of the homework
// chip. It encapsulates the logic to determine if the chip should be shown,
// manages its visibility via the PageActionController, and handles user
// interactions. An instance of this class is created and owned by
// `tabs::TabFeatures` and is associated with a specific tab.
class LensOverlayHomeworkPageActionController {
 public:
  DECLARE_USER_DATA(LensOverlayHomeworkPageActionController);

  LensOverlayHomeworkPageActionController(
      tabs::TabInterface& tab_interface,
      Profile& profile,
      page_actions::PageActionController& page_action_controller);
  ~LensOverlayHomeworkPageActionController();

  LensOverlayHomeworkPageActionController(
      const LensOverlayHomeworkPageActionController&) = delete;
  const LensOverlayHomeworkPageActionController& operator=(
      const LensOverlayHomeworkPageActionController&) = delete;

  // Retrieves the controller instance for a given tab.
  static LensOverlayHomeworkPageActionController* From(tabs::TabInterface& tab);

  // Called to update the visibility of the homework page action icon. This
  // method checks the various criteria in `ShouldShow()` and then either shows
  // or hides the icon and its associated suggestion chip.
  void UpdatePageActionIcon();

  // Handles user interaction with the homework page action chip.
  void HandlePageActionEvent(bool is_from_keyboard);

 private:
  // Determines whether the page action icon should be shown.
  bool ShouldShow();

  // Unowned reference to the profile associated with `tab_`.
  const raw_ref<Profile> profile_;

  // Reference to the tab interface, which provides access to tab-specific
  // features.
  const raw_ref<tabs::TabInterface> tab_;

  // Unowned reference to the page action controller that will coordinate
  // requests from this object.
  const raw_ref<page_actions::PageActionController> page_action_controller_;

  // Associates this controller with the tab's UnownedUserDataHost.
  ui::ScopedUnownedUserData<LensOverlayHomeworkPageActionController>
      scoped_unowned_user_data_;

  std::unique_ptr<ScopedWindowCallToAction> scoped_window_call_to_action_ptr_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LENS_OVERLAY_HOMEWORK_PAGE_ACTION_CONTROLLER_H_
