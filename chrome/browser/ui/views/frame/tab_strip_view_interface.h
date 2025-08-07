// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_TAB_STRIP_VIEW_INTERFACE_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_TAB_STRIP_VIEW_INTERFACE_H_

#include "chrome/browser/ui/views/tabs/dragging/tab_drag_context.h"
#include "chrome/browser/ui/views/tabs/tab_strip_observer.h"
#include "components/tab_groups/tab_group_id.h"
#include "ui/views/view.h"

// This class serves as the single point of interaction for all consumers of
// tabstrip-related functionality. This should only be owned by BrowserView and
// backed by the View container responsible for managing the tabstrip.
class TabStripViewInterface {
 public:
  // -- Layout --
  virtual gfx::Size GetMinimumSize() const = 0;
  virtual gfx::Size GetPreferredSize() const = 0;

  // -- View State Queries --
  virtual bool IsAnimating() const = 0;
  virtual int GetFocusedTabIndex() const = 0;

  // -- UI anchoring --
  virtual views::View* GetTabAnchorViewAt(int tab_index) = 0;
  virtual views::View* GetTabGroupAnchorView(
      const tab_groups::TabGroupId& group) = 0;

  // -- Drag and drop --
  virtual TabDragContext* GetDragContext() = 0;

  // -- Observers --
  virtual void SetTabStripObserver(TabStripObserver* observer) = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_TAB_STRIP_VIEW_INTERFACE_H_
