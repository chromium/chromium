// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_TAB_STRIP_REGION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_TAB_STRIP_REGION_VIEW_H_

#include <optional>

#include "components/tab_groups/tab_group_id.h"
#include "ui/views/accessible_pane_view.h"

class TabDragContext;
class TabStripObserver;

// This class serves as the single point of interaction for all consumers of
// tabstrip-related functionality. This should only be owned by BrowserView and
// backed by the View container responsible for managing the tabstrip.
class TabStripRegionView : public views::AccessiblePaneView {
 public:
  ~TabStripRegionView() override = default;

  // -- View State Queries --
  virtual bool IsTabStripEditable() const = 0;
  virtual void SetTabStripNotEditableForTesting() const = 0;
  virtual bool IsTabStripCloseable() const = 0;
  virtual bool IsAnimating() const = 0;
  virtual void StopAnimating() = 0;
  virtual void UpdateLoadingAnimations(const base::TimeDelta& elapsed_time) = 0;
  virtual std::optional<int> GetFocusedTabIndex() const = 0;

  // -- UI anchoring --
  virtual views::View* GetTabAnchorViewAt(int tab_index) = 0;
  virtual views::View* GetTabGroupAnchorView(
      const tab_groups::TabGroupId& group) = 0;

  // -- Drag and drop --
  virtual TabDragContext* GetDragContext() = 0;

  // -- Observers --
  virtual void SetTabStripObserver(TabStripObserver* observer) = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_TAB_STRIP_REGION_VIEW_H_
