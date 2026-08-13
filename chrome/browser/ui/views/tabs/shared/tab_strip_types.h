// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_SHARED_TAB_STRIP_TYPES_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_SHARED_TAB_STRIP_TYPES_H_

#include "chrome/browser/ui/tabs/tab_enums.h"

// Enum passed to EndDrag().
enum class EndDragReason {
  // Complete the drag.
  kComplete,

  // Cancel/revert the drag.
  kCancel,

  // The drag should end as the result of a capture lost.
  kCaptureLost,

  // The model mutated.
  kModelAddedTab,
};

// Source of the call to ToggleTabGroup(). The source of the call can trigger
// different behaviors such as logging and different animations. Tests will
// generally use `kMenuAction` unless testing a particular code path.
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

enum class TabStripOrientation {
  kHorizontal,
  kVertical,
};

// Reasons for placing a freezing vote on a tab.
enum class FreezingVoteReason {
  // For the tabs inside a collapsed tab group.
  kCollapsedGroup,

  // For the unpinned tabs outside a focused tab group.
  kFocusedGroup,
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_SHARED_TAB_STRIP_TYPES_H_
