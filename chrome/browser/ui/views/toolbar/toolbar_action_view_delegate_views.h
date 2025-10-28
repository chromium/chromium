// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACTION_VIEW_DELEGATE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACTION_VIEW_DELEGATE_VIEWS_H_

#include "chrome/browser/ui/toolbar/toolbar_action_view_delegate.h"

// The views-specific methods necessary for a ToolbarActionViewDelegate.
class ToolbarActionViewDelegateViews : public ToolbarActionViewDelegate {
 public:
  // TODO(crbug.com/448199168): Remove this class.

 protected:
  ~ToolbarActionViewDelegateViews() override = default;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACTION_VIEW_DELEGATE_VIEWS_H_
