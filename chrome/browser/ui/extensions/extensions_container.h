// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_CONTAINER_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_CONTAINER_H_

#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/extensions/extension_popup_types.h"
#include "chrome/browser/ui/toolbar/toolbar_action_hover_card_types.h"
#include "extensions/common/extension_id.h"

class ToolbarActionViewController;
class ToolbarActionsBarBubbleDelegate;
class ToolbarActionView;

// An interface for containers in the toolbar that host extensions.
class ExtensionsContainer {
 public:
  // Returns the action for the given |id|, if one exists.
  virtual ToolbarActionViewController* GetActionForId(
      const std::string& action_id) = 0;

  // Get the currently popped out action id, if any.
  // TODO(pbos): Consider supporting multiple popped out actions for bubbles
  // that relate to more than one extension.
  virtual std::optional<extensions::ExtensionId> GetPoppedOutActionId()
      const = 0;

  // Called when the context menu of a toolbar action with `action_id` is
  // opened, so the container can perform any necessary setup.
  virtual void OnContextMenuShownFromToolbar(const std::string& action_id) {}

  // Called when the context menu of a toolbar action is closed, so the
  // container can perform any necessary cleanup.
  virtual void OnContextMenuClosedFromToolbar() {}

  // Returns true if the action pointed by `action_id` is visible on the
  // toolbar.
  virtual bool IsActionVisibleOnToolbar(const std::string& action_id) const = 0;

  // Undoes the current "pop out"; i.e., moves the popped out action back into
  // overflow.
  virtual void UndoPopOut() = 0;

  // Sets the active popup owner to be |popup_owner|.
  virtual void SetPopupOwner(ToolbarActionViewController* popup_owner) = 0;

  // Hides the actively showing popup, if any.
  virtual void HideActivePopup() = 0;

  // Closes the overflow menu, if it was open. Returns whether or not the
  // overflow menu was closed.
  virtual bool CloseOverflowMenuIfOpen() = 0;

  // Pops out `action_id`, ensuring it is visible. `closure` will be called once
  // any animation is complete.
  virtual void PopOutAction(const extensions::ExtensionId& action_id,
                            base::OnceClosure closure) = 0;

  // Shows the popup for the action with |id| as the result of an API call,
  // returning true if a popup is shown and invoking |callback| upon completion.
  virtual bool ShowToolbarActionPopupForAPICall(const std::string& action_id,
                                                ShowPopupCallback callback) = 0;

  // Displays the given |bubble| once the toolbar is no longer animating.
  virtual void ShowToolbarActionBubble(
      std::unique_ptr<ToolbarActionsBarBubbleDelegate> bubble) = 0;

  // Toggle the Extensions menu (as if the user clicked the puzzle piece icon).
  virtual void ToggleExtensionsMenu() = 0;

  // Whether there are any Extensions registered with the ExtensionsContainer.
  virtual bool HasAnyExtensions() const = 0;

  // Updates the hover card for `action_view` based on `update_type`.
  virtual void UpdateToolbarActionHoverCard(
      ToolbarActionView* action_view,
      ToolbarActionHoverCardUpdateType update_type) = 0;

  // Collapses the confirmation on the request access button, effectively
  // hiding the button. Does nothing if the confirmation is not showing
  // anymore.
  virtual void CollapseConfirmation() = 0;
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_CONTAINER_H_
