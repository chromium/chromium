// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_PINNED_TOOLBAR_ACTIONS_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_PINNED_TOOLBAR_ACTIONS_H_

#include "chrome/browser/ui/views/toolbar/toolbar_controller.h"
#include "ui/actions/action_id.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class PinnedActionToolbarButton;

// The PinnedToolbarActions class is a virtual interface, defining access to the
// window's pinned toolbar actions component.  This class exists so that
// cross-platform components like the browser command system can talk to the
// platform specific implementations of the pinned toolbar actions control.  It
// also allows the pinned toolbar actions to be mocked for testing.
class PinnedToolbarActions : public ToolbarController::PinnedActionsDelegate {
 public:
  // TODO(https://crbug.com/363743077): This method is almost but not quite
  // identical to ShowActionEphemerallyInToolbar(). This doesn't make sense and
  // one should be removed.
  virtual void UpdateActionState(actions::ActionId id, bool is_active) = 0;
  // Updates whether the button is shown ephemerally in the toolbar (in the
  // popped out region unless also pinned) regardless of whether it is active.
  virtual void ShowActionEphemerallyInToolbar(actions::ActionId id,
                                              bool show) = 0;
  virtual bool IsActionPinned(actions::ActionId id) = 0;
  virtual bool IsActionPoppedOut(actions::ActionId id) = 0;
  virtual bool IsActionPinnedOrPoppedOut(actions::ActionId id) = 0;

  // Queues an action to take place after the current animation completes.
  virtual void PostOrQueueActionAfterAnimation(base::OnceClosure action) = 0;

  // Gets a pointer to the download button.
  // TODO(https://crbug.com/474063115): Change this to a non-Views return type.
  virtual ToolbarButton* GetDownloadButton() = 0;

  // Returns BubbleAnchor for the action.
  virtual views::BubbleAnchor GetBubbleAnchor(actions::ActionId action_id) = 0;

  // Returns the ChromeLabs button, or nullptr if ChromeLabs is not supported by
  // the PinnedToolbarActions implementation being used.
  virtual PinnedActionToolbarButton* GetChromeLabsButton() = 0;

  // Set |id|'s pinned state to |pin| and announce it.
  virtual void UpdatePinnedStateAndAnnounce(actions::ActionId id, bool pin) = 0;

  // Returns the ElementIdentifier for an action. GetBubbleAnchor() should
  // generally be used for anchoring to pinned toolbar action buttons, but if
  // you want an action to have a specific ElementIdentifier, feel free to add
  // it to this method.
  static ui::ElementIdentifier GetElementIdentifierForAction(
      actions::ActionId id);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_PINNED_TOOLBAR_ACTIONS_H_
