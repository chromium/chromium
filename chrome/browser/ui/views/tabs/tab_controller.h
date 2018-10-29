// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_CONTROLLER_H_

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip_types.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ui_base_types.h"

class Tab;

namespace gfx {
class Path;
class Point;
class Rect;
}
namespace ui {
class ListSelectionModel;
class LocatedEvent;
class MouseEvent;
}
namespace views {
class View;
}

// Controller for tabs.
class TabController {
 public:
  virtual const ui::ListSelectionModel& GetSelectionModel() const = 0;

  // Returns true if multiple selection is supported.
  virtual bool SupportsMultipleSelection() = 0;

  // Returns where the new tab button should be placed. This is needed to
  // determine which tab separators need to be faded in/out while animating into
  // position.
  virtual NewTabButtonPosition GetNewTabButtonPosition() const = 0;

  // Returns true if the close button for the given tab is forced to be hidden.
  virtual bool ShouldHideCloseButtonForTab(Tab* tab) const = 0;

  // Returns true if the close button on an inactive tab should be shown on
  // mouse hover. This is predicated on ShouldHideCloseButtonForInactiveTabs()
  // returning true.
  virtual bool ShouldShowCloseButtonOnHover() = 0;

  // Returns true if ShouldPaintTab() could return a non-empty clip path.
  virtual bool MaySetClip() = 0;

  // Selects the tab.
  virtual void SelectTab(Tab* tab) = 0;

  // Extends the selection from the anchor to |tab|.
  virtual void ExtendSelectionTo(Tab* tab) = 0;

  // Toggles whether |tab| is selected.
  virtual void ToggleSelected(Tab* tab) = 0;

  // Adds the selection from the anchor to |tab|.
  virtual void AddSelectionFromAnchorTo(Tab* tab) = 0;

  // Closes the tab.
  virtual void CloseTab(Tab* tab, CloseTabSource source) = 0;

  // Shows a context menu for the tab at the specified point in screen coords.
  virtual void ShowContextMenuForTab(Tab* tab,
                                     const gfx::Point& p,
                                     ui::MenuSourceType source_type) = 0;

  // Returns whether |tab| is the active tab. The active tab is the one whose
  // content is shown in the browser.
  virtual bool IsActiveTab(const Tab* tab) const = 0;

  // Returns whether |tab| is selected.
  virtual bool IsTabSelected(const Tab* tab) const = 0;

  // Returns whether |tab| is pinned.
  virtual bool IsTabPinned(const Tab* tab) const = 0;

  // Returns whether |tab| is the first or last one visible.
  virtual bool IsFirstVisibleTab(const Tab* tab) const = 0;
  virtual bool IsLastVisibleTab(const Tab* tab) const = 0;

  // Returns whether the strip is painting in single-tab mode.  This is true in
  // a subset of the cases where ther is exactly one tab.
  virtual bool SingleTabMode() const = 0;

  // Potentially starts a drag for the specified Tab.
  virtual void MaybeStartDrag(
      Tab* tab,
      const ui::LocatedEvent& event,
      const ui::ListSelectionModel& original_selection) = 0;

  // Continues dragging a Tab.
  virtual void ContinueDrag(views::View* view,
                            const ui::LocatedEvent& event) = 0;

  // Ends dragging a Tab. Returns whether the tab has been destroyed.
  virtual bool EndDrag(EndDragReason reason) = 0;

  // Returns the tab that contains the specified point in tabstrip coordinates,
  // or null if there is no tab that contains the specified point.
  virtual Tab* GetTabAt(const gfx::Point& point) = 0;

  // Returns the tab at offset |offset| from the current tab in the model order.
  // Returns nullptr if that offset does not result in a valid model index.
  virtual const Tab* GetAdjacentTab(const Tab* tab, int offset) = 0;

  // Invoked when a mouse event occurs on |source|.
  virtual void OnMouseEventInTab(views::View* source,
                                 const ui::MouseEvent& event) = 0;

  // Returns whether |tab| needs to be painted. When this returns true, |clip|
  // is set to the path which should be clipped out of the current tab's region
  // (for hit testing or painting), if any.  |clip| is only non-empty when
  // stacking tabs; if it is empty, no clipping is needed.
  virtual bool ShouldPaintTab(const Tab* tab, float scale, gfx::Path* clip) = 0;

  // Returns the thickness of the stroke around the active tab in DIP.  Returns
  // 0 if there is no stroke.
  virtual int GetStrokeThickness() const = 0;

  // Returns true if tab loading throbbers can be painted to a composited layer.
  // This can only be done when the TabController can guarantee that nothing
  // in the same window will redraw on top of the the favicon area of any tab.
  virtual bool CanPaintThrobberToLayer() const = 0;

  // Returns whether the shapes of background tabs are visible against the
  // frame.
  virtual bool HasVisibleBackgroundTabShapes() const = 0;

  // Returns COLOR_TOOLBAR_TOP_SEPARATOR[,_INACTIVE] depending on the activation
  // state of the window.
  virtual SkColor GetToolbarTopSeparatorColor() const = 0;

  // Returns the color of the separator between the tabs.
  virtual SkColor GetTabSeparatorColor() const = 0;

  // Returns the tab background color based on both the |state| of the tab and
  // the activation state of the window.
  virtual SkColor GetTabBackgroundColor(TabState state) const = 0;

  // Returns the tab foreground color of the the text based on both the |state|
  // of the tab and the activation state of the window.
  virtual SkColor GetTabForegroundColor(TabState state) const = 0;

  // Returns the resource ID for the image to use as the tab background.
  // |custom_image| is an outparam set to true if either the tab or the frame
  // background images have been customized; see implementation comments.
  virtual int GetBackgroundResourceId(
      bool* has_custom_image,
      BrowserNonClientFrameView::ActiveState active_state =
          BrowserNonClientFrameView::kUseCurrent) const = 0;

  // If the given tab is animating to its target destination, this returns the
  // target bounds. If the tab isn't moving this will return the current bounds
  // of the given tab.
  virtual gfx::Rect GetTabAnimationTargetBounds(const Tab* tab) = 0;

  // Returns the accessible tab name for this tab.
  virtual base::string16 GetAccessibleTabName(const Tab* tab) const = 0;

  // Returns opacity for hover effect on a tab with |range_parameter| between
  // 0 and 1, where 0 gives the minimum opacity suitable for wider tabs and 1
  // gives maximum opacity suitable for narrower tabs.
  virtual float GetHoverOpacityForTab(float range_parameter) const = 0;

  // Returns opacity for use on tab hover radial highlight.
  virtual float GetHoverOpacityForRadialHighlight() const = 0;

 protected:
  virtual ~TabController() {}
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_CONTROLLER_H_
