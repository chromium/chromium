// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACTION_VIEW_DELEGATE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACTION_VIEW_DELEGATE_VIEWS_H_

#include "chrome/browser/ui/toolbar/toolbar_action_view_delegate.h"

namespace views {
class Button;
class FocusManager;
}

// The views-specific methods necessary for a ToolbarActionViewDelegate.
class ToolbarActionViewDelegateViews : public ToolbarActionViewDelegate {
 public:
  // Returns the FocusManager to use when registering accelerators.
  virtual views::FocusManager* GetFocusManagerForAccelerator() = 0;

  // Returns the reference button for the extension action's popup. Rather than
  // relying on the button being a MenuButton, the button returned should have a
  // MenuButtonController. This is part of the ongoing work from
  // http://crbug.com/901183 to simplify the button hierarchy by migrating
  // controller logic into a separate class leaving MenuButton as an empty class
  // to be deprecated.
  virtual views::Button* GetReferenceButtonForPopup() = 0;

 protected:
  ~ToolbarActionViewDelegateViews() override {}
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACTION_VIEW_DELEGATE_VIEWS_H_
