// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_TYPES_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_TYPES_H_
// Enum passed to EndDrag().
enum EndDragReason {
  // Complete the drag.
  END_DRAG_COMPLETE,

  // Cancel/revert the drag.
  END_DRAG_CANCEL,

  // The drag should end as the result of a capture lost.
  END_DRAG_CAPTURE_LOST,

  // The model mutated.
  END_DRAG_MODEL_ADDED_TAB,
};

// Source of the call to CloseTab().
enum CloseTabSource {
  CLOSE_TAB_FROM_MOUSE,
  CLOSE_TAB_FROM_TOUCH,
};

// Source of the call to ToggleTabGroup(). The source of the call can trigger
// different behaviors such as logging and different animations. Tests will
// generally use |kMenuAction| unless testing a particular code path.
enum class ToggleTabGroupCollapsedStateOrigin {
  // Use when triggering a submenu action or other automated process such as
  // "Add tab to group".
  kMenuAction,
  // Use when a mouse click interacts with the tab group header.
  kMouse,
  // Use when a keyboard event (SPACE or ENTER) interacts with the tab group
  // header while focused via keyboard navigation.
  kKeyboard,
  // Use when a gesture control (TAP) interacts with the tab group header.
  kGesture,
  // Use when tabs in the group are selected. This causes a collapsed group to
  // expand if the range of tabs include a collapsed group.
  kTabsSelected,
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_TYPES_H_
