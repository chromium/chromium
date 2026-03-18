// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_PINNED_TOOLBAR_ACTIONS_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_PINNED_TOOLBAR_ACTIONS_H_

#include "chrome/browser/ui/views/toolbar/toolbar_controller.h"
#include "ui/actions/action_id.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

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
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_PINNED_TOOLBAR_ACTIONS_H_
