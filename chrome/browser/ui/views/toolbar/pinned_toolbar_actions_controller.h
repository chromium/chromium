// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_PINNED_TOOLBAR_ACTIONS_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_PINNED_TOOLBAR_ACTIONS_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "ui/actions/action_id.h"

class PinnedActionToolbarButton;
class PinnedToolbarActionsContainer;

// `PinnedToolbarActionsController` is used to allow features to interact with
// their feature's pinnable toolbar button in the PinnedToolbarActionsContainer.
class PinnedToolbarActionsController {
 public:
  explicit PinnedToolbarActionsController(
      PinnedToolbarActionsContainer* container);
  PinnedToolbarActionsController(const PinnedToolbarActionsController&) =
      delete;
  PinnedToolbarActionsController& operator=(
      const PinnedToolbarActionsController&) = delete;
  ~PinnedToolbarActionsController();

  // Tears down the controller.
  void TearDown();

  // Toggle visibility of the action button in the toolbar.
  void ShowActionEphemerallyInToolbar(actions::ActionId id, bool is_active);

  // Returns whether the action is popped out.
  bool IsActionPoppedOut(actions::ActionId id);

  // Returns the button for the action.
  PinnedActionToolbarButton* GetButtonFor(actions::ActionId id);

 private:
  raw_ptr<PinnedToolbarActionsContainer> container_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_PINNED_TOOLBAR_ACTIONS_CONTROLLER_H_
