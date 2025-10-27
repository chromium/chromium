// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_CONTAINER_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_CONTAINER_VIEWS_H_

#include <memory>
#include <optional>
#include <string>

#include "chrome/browser/ui/extensions/extension_popup_types.h"
#include "chrome/browser/ui/extensions/extensions_container.h"
#include "chrome/browser/ui/toolbar/toolbar_action_hover_card_types.h"
#include "extensions/common/extension_id.h"

class ToolbarActionView;
class ToolbarActionViewController;

// ExtensionsContainer with views-specific additional methods.
//
// While ExtensionsContainer provides a minimal set of APIs for non-UI code,
// this interface provides additional methods for views-specific extension UI
// subcomponents to interact with the extension toolbar.
class ExtensionsContainerViews : public ExtensionsContainer {
 public:
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

  // Pops out `action_id`, ensuring it is visible. `closure` will be called once
  // any animation is complete.
  virtual void PopOutAction(const extensions::ExtensionId& action_id,
                            base::OnceClosure closure) = 0;

  // Updates the hover card for `action_view` based on `update_type`.
  virtual void UpdateToolbarActionHoverCard(
      ToolbarActionView* action_view,
      ToolbarActionHoverCardUpdateType update_type) = 0;

  // Collapses the confirmation on the request access button, effectively
  // hiding the button. Does nothing if the confirmation is not showing
  // anymore.
  virtual void CollapseConfirmation() = 0;

  // Shows the context menu for the action as a fallback for performing another
  // action.
  virtual void ShowContextMenuAsFallback(
      const extensions::ExtensionId& action_id) = 0;

  // Called when a popup is shown. If `by_user` is true, then this was through
  // a direct user action (as opposed to, e.g., an API call).
  virtual void OnPopupShown(const extensions::ExtensionId& action_id,
                            bool by_user) = 0;

  // Called when a popup is closed.
  virtual void OnPopupClosed(const extensions::ExtensionId& action_id) = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_CONTAINER_VIEWS_H_
