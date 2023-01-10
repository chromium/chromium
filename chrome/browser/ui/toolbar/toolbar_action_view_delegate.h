// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_ACTION_VIEW_DELEGATE_H_
#define CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_ACTION_VIEW_DELEGATE_H_

class ToolbarActionViewController;

namespace content {
class WebContents;
}

// The view for a ToolbarAction, which is controlled by a
// ToolbarActionViewController.
class ToolbarActionViewDelegate {
 public:
  // Returns the current web contents.
  virtual content::WebContents* GetCurrentWebContents() const = 0;

  // Updates the view to reflect current state.
  virtual void UpdateState() = 0;

  // Shows the context menu for the action as a fallback for performing another
  // action.
  virtual void ShowContextMenuAsFallback() = 0;

  // Called when a popup is shown. If |by_user| is true, then this was through
  // a direct user action (as oppposed to, e.g., an API call).
  virtual void OnPopupShown(bool by_user) {}

  // Called when a popup is closed.
  virtual void OnPopupClosed() {}

 protected:
  virtual ~ToolbarActionViewDelegate() {}
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_ACTION_VIEW_DELEGATE_H_
