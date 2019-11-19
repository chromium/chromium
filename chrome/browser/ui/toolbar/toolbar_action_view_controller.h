// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_ACTION_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_ACTION_VIEW_CONTROLLER_H_

#include "base/strings/string16.h"
#include "ui/gfx/image/image.h"

namespace content {
class WebContents;
}

namespace gfx {
class Size;
}

namespace ui {
class MenuModel;
}

class ToolbarActionViewDelegate;

// The basic controller class for an action that is shown on the toolbar -
// an extension action (like browser actions) or a component action (like
// Media Router).
class ToolbarActionViewController {
 public:
  // The status of the extension's interaction for the page. This is independent
  // of the action's clickability.
  enum class PageInteractionStatus {
    // The extension cannot run on the page.
    kNone,
    // The extension tried to access the page, but is pending user approval.
    kPending,
    // The extension has permission to run on the page.
    kActive,
  };

  virtual ~ToolbarActionViewController() {}

  // Returns the unique ID of this particular action. For extensions, this is
  // the extension id; for component actions, this is the name of the component.
  virtual std::string GetId() const = 0;

  // Sets the view delegate, which can handle most of the front-end logic.
  virtual void SetDelegate(ToolbarActionViewDelegate* delegate) = 0;

  // Returns the icon to use for the given |web_contents| and |size|.
  virtual gfx::Image GetIcon(content::WebContents* web_contents,
                             const gfx::Size& size) = 0;

  // Returns the name of the action, which can be separate from the accessible
  // name or name for the tooltip.
  virtual base::string16 GetActionName() const = 0;

  // Returns the accessible name to use for the given |web_contents|.
  // May be passed null, or a |web_contents| that returns -1 for
  // |SessionTabHelper::IdForTab(..)|.
  virtual base::string16 GetAccessibleName(content::WebContents* web_contents)
      const = 0;

  // Returns the tooltip to use for the given |web_contents|.
  virtual base::string16 GetTooltip(content::WebContents* web_contents)
      const = 0;

  // Returns true if the action should be enabled on the given |web_contents|.
  virtual bool IsEnabled(content::WebContents* web_contents) const = 0;

  // Returns true if the action wants to run, and should be popped out of the
  // overflow menu on the given |web_contents|.
  virtual bool WantsToRun(content::WebContents* web_contents) const = 0;

  // Returns true if the action has a popup for the given |web_contents|.
  virtual bool HasPopup(content::WebContents* web_contents) const = 0;

  // Returns whether there is currently a popup visible.
  virtual bool IsShowingPopup() const = 0;

  // Hides the current popup, if one is visible.
  virtual void HidePopup() = 0;

  // Returns the native view for the popup, if one is active.
  virtual gfx::NativeView GetPopupNativeView() = 0;

  // Returns the context menu model, or null if no context menu should be shown.
  virtual ui::MenuModel* GetContextMenu() = 0;

  // Called when a context menu has closed so the controller can perform any
  // necessary cleanup.
  virtual void OnContextMenuClosed() {}

  // Executes the default action (which is typically showing the popup). If
  // |by_user| is true, then this was through a direct user action (as oppposed
  // to, e.g., an API call).
  // Returns true if a popup is shown.
  virtual bool ExecuteAction(bool by_user) = 0;

  // Updates the current state of the action.
  virtual void UpdateState() = 0;

  // Returns true if clicking on an otherwise-disabled action should open the
  // context menu.
  virtual bool DisabledClickOpensMenu() const = 0;

  // Registers an accelerator. Called when the view is added to the hierarchy.
  // Unregistering any commands is the responsibility of the controller.
  virtual void RegisterCommand() {}

  // Returns the PageInteractionStatus for the current page.
  virtual PageInteractionStatus GetPageInteractionStatus(
      content::WebContents* web_contents) const = 0;
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_ACTION_VIEW_CONTROLLER_H_
