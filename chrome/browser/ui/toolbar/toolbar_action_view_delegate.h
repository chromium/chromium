// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_ACTION_VIEW_DELEGATE_H_
#define CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_ACTION_VIEW_DELEGATE_H_

class ToolbarActionViewController;

// The view for a ToolbarAction, which is controlled by a
// ToolbarActionViewController.
class ToolbarActionViewDelegate {
 public:
  // Updates the view to reflect current state.
  virtual void UpdateState() = 0;

 protected:
  virtual ~ToolbarActionViewDelegate() = default;
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_ACTION_VIEW_DELEGATE_H_
