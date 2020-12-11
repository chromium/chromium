// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_ACTIONS_BAR_BUBBLE_DELEGATE_H_
#define CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_ACTIONS_BAR_BUBBLE_DELEGATE_H_

#include <string>

#include "base/callback_forward.h"
#include "base/strings/string16.h"
#include "ui/base/ui_base_types.h"

namespace gfx {
struct VectorIcon;
}

// A delegate for a generic bubble that hangs off the toolbar actions bar.
class ToolbarActionsBarBubbleDelegate {
 public:
  enum CloseAction {
    CLOSE_LEARN_MORE,
    CLOSE_EXECUTE,
    CLOSE_DISMISS_USER_ACTION,
    CLOSE_DISMISS_DEACTIVATION,
  };

  // Content populating an optional view, containing an image icon and/or
  // (linked) text, in the bubble.
  struct ExtraViewInfo {
    ExtraViewInfo() : resource(nullptr), is_learn_more(false) {}

    // The resource defining the image icon. If has a value of null, then no
    // image icon will be added.
    const gfx::VectorIcon* resource;

    // Text in the view. If this is an empty string, no text will be added.
    base::string16 text;

    // If the struct's text is nonempty and this value is true, then a button
    // with the (?) image is displayed and the text is set as the tooltip. If
    // this value is false, the text is treated as a label.
    bool is_learn_more;
  };

  virtual ~ToolbarActionsBarBubbleDelegate() {}

  // Returns true if the bubble should (still) be shown. Since bubbles are
  // sometimes shown asynchronously, they may be invalid by the time they would
  // be displayed.
  virtual bool ShouldShow() = 0;

  // Returns true if the bubble should close on deactivation.
  virtual bool ShouldCloseOnDeactivate() = 0;

  // Gets the text for the bubble's heading (title).
  virtual base::string16 GetHeadingText() = 0;

  // Gets the text for the bubble's body.
  // |anchored_to_action| is true if the bubble is being anchored to a specific
  // action (rather than the overflow menu or the full container).
  virtual base::string16 GetBodyText(bool anchored_to_action) = 0;

  // Gets the text for an optional item list to display. If this returns an
  // empty string, no list will be added.
  virtual base::string16 GetItemListText() = 0;

  // Gets the text for the main button on the bubble; this button will
  // correspond with ACTION_EXECUTE.
  virtual base::string16 GetActionButtonText() = 0;

  // Gets the text for a second button on the bubble; this button will
  // correspond with ACTION_DISMISS. If this returns an empty string, no
  // button will be added.
  virtual base::string16 GetDismissButtonText() = 0;

  // Returns the button that should be set to the default.
  virtual ui::DialogButton GetDefaultDialogButton() = 0;

  // Returns the id of the action to point to, or the empty string if the
  // bubble should point to the center of the actions container.
  virtual std::string GetAnchorActionId() = 0;

  // Called when the bubble is shown. Accepts a callback from platform-specifc
  // ui code to close the bubble.
  virtual void OnBubbleShown(const base::Closure& close_bubble_callback) = 0;

  // Called when the bubble is closed with the type of action the user took.
  virtual void OnBubbleClosed(CloseAction action) = 0;

  // Returns the ExtraViewInfo struct associated with the bubble delegate. If
  // this returns a nullptr, no extra view (image icon and/or (linked) text) is
  // added to the bubble.
  virtual std::unique_ptr<ExtraViewInfo> GetExtraViewInfo() = 0;
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_ACTIONS_BAR_BUBBLE_DELEGATE_H_
