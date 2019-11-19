// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_CONTAINER_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_CONTAINER_H_

#include <string>

#include "base/callback_forward.h"

class ToolbarActionViewController;
class ToolbarActionsBarBubbleDelegate;

// An interface for containers in the toolbar that host extensions.
class ExtensionsContainer {
 public:
  // Returns the action for the given |id|, if one exists.
  virtual ToolbarActionViewController* GetActionForId(
      const std::string& action_id) = 0;

  // Get the currently popped out action if any.
  // TODO(pbos): Consider supporting multiple popped out actions for bubbles
  // that relate to more than one extension.
  virtual ToolbarActionViewController* GetPoppedOutAction() const = 0;

  // Returns true if the given |action| is visible on the toolbar.
  virtual bool IsActionVisibleOnToolbar(
      const ToolbarActionViewController* action) const = 0;

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

  // Pops out a given |action|, ensuring it is visible.
  // |is_sticky| refers to whether or not the action will stay popped out if
  // the overflow menu is opened.
  // |closure| will be called once any animation is complete.
  virtual void PopOutAction(ToolbarActionViewController* action,
                            bool is_sticky,
                            const base::Closure& closure) = 0;

  // Displays the given |bubble| once the toolbar is no longer animating.
  virtual void ShowToolbarActionBubble(
      std::unique_ptr<ToolbarActionsBarBubbleDelegate> bubble) = 0;

  // Same as above, but uses PostTask() in all cases.
  virtual void ShowToolbarActionBubbleAsync(
      std::unique_ptr<ToolbarActionsBarBubbleDelegate> bubble) = 0;
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_CONTAINER_H_
