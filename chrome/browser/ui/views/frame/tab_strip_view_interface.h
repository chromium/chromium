// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_TAB_STRIP_VIEW_INTERFACE_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_TAB_STRIP_VIEW_INTERFACE_H_

#include <optional>

#include "components/tab_groups/tab_group_id.h"
#include "ui/gfx/geometry/rect.h"

class ProductSpecificationsButton;
class TabSearchButton;
class TabStripActionContainer;
class TabDragContext;
class Tab;
class TabStripObserver;

namespace glic {
class GlicButton;
}  // namespace glic

namespace views {
class View;
}  // namespace views

// This class serves as the single point of interaction for all consumers of
// tabstrip-related functionality. This should only be owned by BrowserView and
// backed by the View container responsible for managing the tabstrip.
class TabStripViewInterface {
 public:
  virtual ~TabStripViewInterface() = default;

  // -- Layout --
  virtual gfx::Size GetMinimumSize() const = 0;
  virtual gfx::Size GetPreferredSizeForView() const = 0;

  // -- Views --
  virtual TabSearchButton* GetTabSearchButton() const = 0;
  virtual TabStripActionContainer* GetTabStripActionContainer() const = 0;
  virtual ProductSpecificationsButton* GetProductSpecificationsButton()
      const = 0;
  virtual glic::GlicButton* GetGlicButton() const = 0;

  // -- View State Queries --
  virtual bool IsAnimating() const = 0;
  virtual void StopAnimating() = 0;
  virtual std::optional<int> GetFocusedTabIndex() const = 0;

  // -- UI anchoring --
  virtual Tab* GetTabAnchorViewAt(int tab_index) = 0;
  virtual views::View* GetTabGroupAnchorView(
      const tab_groups::TabGroupId& group) = 0;
  virtual gfx::Rect GetBoundsInScreenForView() = 0;

  // -- Drag and drop --
  virtual TabDragContext* GetDragContext() = 0;

  // -- Observers --
  virtual void SetTabStripObserver(TabStripObserver* observer) = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_TAB_STRIP_VIEW_INTERFACE_H_
