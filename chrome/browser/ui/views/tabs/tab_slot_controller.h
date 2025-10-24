// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_SLOT_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_SLOT_CONTROLLER_H_

#include <optional>
#include <string>

#include "build/build_config.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/tabs/tab_types.h"
#include "chrome/browser/ui/views/tabs/tab_strip_types.h"
#include "components/tab_groups/tab_group_id.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/mojom/menu_source_type.mojom-forward.h"

class Browser;
class Tab;
class TabGroup;
class TabSlotView;

enum class BrowserFrameActiveState;

namespace gfx {
class Point;
}  // namespace gfx
namespace tab_groups {
enum class TabGroupColorId;
class TabGroupId;
}  // namespace tab_groups
namespace ui {
class Event;
class ListSelectionModel;
class LocatedEvent;
class MouseEvent;
}  // namespace ui
namespace views {
class View;
}

// Controller for tabs and group headers.
class TabSlotController {
 public:
  enum HoverCardUpdateType {
    kHover,
    kFocus,
    kTabDataChanged,
    kAnimating,
    kTabRemoved,
    kSelectionChanged,
    kEvent
  };

  enum class Liveness { kAlive, kDeleted };

  virtual const ui::ListSelectionModel& GetSelectionModel() const = 0;

  // Returns the tab at `index`.
  virtual Tab* tab_at(int index) const = 0;

  // Selects the tab. `event` is the event that causes `tab` to be selected.
  virtual void SelectTab(Tab* tab, const ui::Event& event) = 0;

  // Extends the selection from the anchor to `tab`.
  virtual void ExtendSelectionTo(Tab* tab) = 0;

  // Toggles whether `tab` is selected.
  virtual void ToggleSelected(Tab* tab) = 0;

  // Adds the selection from the anchor to `tab`.
  virtual void AddSelectionFromAnchorTo(Tab* tab) = 0;

  // Closes the tab.
  virtual void CloseTab(Tab* tab, CloseTabSource source) = 0;

  // Toggles whether tab-wide audio muting is active.
  virtual void ToggleTabAudioMute(Tab* tab) = 0;

  // Attempts to shift the specified tab towards the end of the tabstrip by one
  // index.
  virtual void ShiftTabNext(Tab* tab) = 0;

  // Attempts to shift the specified tab towards the start of the tabstrip by
  // one index.
  virtual void ShiftTabPrevious(Tab* tab) = 0;

  // Attempts to move the specified tab to the beginning of the tabstrip (or the
  // beginning of the unpinned tab region if the tab is not pinned).
  virtual void MoveTabFirst(Tab* tab) = 0;

  // Attempts to move the specified tab to the end of the tabstrip (or the end
  // of the pinned tab region if the tab is pinned).
  virtual void MoveTabLast(Tab* tab) = 0;

  // Switches the collapsed state of a tab group. Returns false if the state was
  // not successfully switched.
  virtual void ToggleTabGroupCollapsedState(
      tab_groups::TabGroupId group,
      ToggleTabGroupCollapsedStateOrigin origin) = 0;
  void ToggleTabGroupCollapsedState(tab_groups::TabGroupId group) {
    ToggleTabGroupCollapsedState(
        group, ToggleTabGroupCollapsedStateOrigin::kMenuAction);
  }

  // Notify this controller of a bubble opening/closing in the tabstrip.
  virtual void NotifyTabstripBubbleOpened() = 0;
  virtual void NotifyTabstripBubbleClosed() = 0;

  // Shows a context menu for the tab at the specified point in screen coords.
  virtual void ShowContextMenuForTab(Tab* tab,
                                     const gfx::Point& p,
                                     ui::mojom::MenuSourceType source_type) = 0;

  // Returns whether `tab` is the active tab. The active tab is the one whose
  // content is shown in the browser.
  virtual bool IsActiveTab(const Tab* tab) const = 0;

  // Returns whether `tab` is selected.
  virtual bool IsTabSelected(const Tab* tab) const = 0;

  // Returns whether `tab` is pinned.
  virtual bool IsTabPinned(const Tab* tab) const = 0;

  // Returns whether `tab` is the first in the model.
  virtual bool IsTabFirst(const Tab* tab) const = 0;

  // Returns true if any tab or one of its children has focus.
  virtual bool IsFocusInTabs() const = 0;

  // Returns true if The tab should have a compacted leading edge.
  virtual bool ShouldCompactLeadingEdge() const = 0;

  // Potentially starts a drag for the specified Tab.
  virtual void MaybeStartDrag(
      TabSlotView* source,
      const ui::LocatedEvent& event,
      const ui::ListSelectionModel& original_selection) = 0;

  // Continues dragging a Tab. May enter a nested event loop - returns
  // Liveness::kDeleted if `this` was destroyed during this nested event loop,
  // and Liveness::kAlive if `this` is still alive.
  [[nodiscard]] virtual Liveness ContinueDrag(
      views::View* view,
      const ui::LocatedEvent& event) = 0;

  // Ends dragging a Tab. Returns whether the tab has been destroyed.
  virtual bool EndDrag(EndDragReason reason) = 0;

  // Returns the tab that contains the specified point in tabstrip coordinates,
  // or null if there is no tab that contains the specified point.
  virtual Tab* GetTabAt(const gfx::Point& point) = 0;

  // Returns the tab at offset `offset` from the current tab in the model order.
  // Returns nullptr if that offset does not result in a valid model index.
  virtual Tab* GetAdjacentTab(const Tab* tab, int offset) = 0;

  // Returns a vector of all split tabs in a split with the provided tab.
  // Returns an empty vector if `tab` is not a split tab.
  virtual std::vector<Tab*> GetTabsInSplit(const Tab* tab) = 0;

  // Invoked when a mouse event occurs on `source`.
  virtual void OnMouseEventInTab(views::View* source,
                                 const ui::MouseEvent& event) = 0;

  // Updates hover-card content, anchoring and visibility based on what tab is
  // hovered and whether the card should be shown. Providing a nullptr for `tab`
  // will cause the tab hover card to be hidden. `update_type` is used to decide
  // how the show, hide, or update will be processed.
  virtual void UpdateHoverCard(Tab* tab, HoverCardUpdateType update_type) = 0;

  // Returns true if the hover card is showing for the given tab.
  virtual bool HoverCardIsShowingForTab(Tab* tab) = 0;

  // Updates the hover effect for all affected tabs when a hover happens on
  // `tab`.
  virtual void ShowHover(Tab* tab, TabStyle::ShowHoverStyle style) = 0;

  // Hides the hover effect for all affected tabs when a hover happens on
  // `tab`.
  virtual void HideHover(Tab* tab, TabStyle::HideHoverStyle style) = 0;

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

  // Returns the color of the separator between the tabs.
  virtual SkColor GetTabSeparatorColor() const = 0;

  // Returns the tab foreground color of the the text based on `active` and the
  // activation state of the window.
  virtual SkColor GetTabForegroundColor(TabActive active) const = 0;

  // Returns the background tab image resource ID if the image has been
  // customized, directly or indirectly, by the theme.
  virtual std::optional<int> GetCustomBackgroundId(
      BrowserFrameActiveState active_state) const = 0;

  // Returns the accessible tab name for this tab.
  virtual std::u16string GetAccessibleTabName(const Tab* tab) const = 0;

  // Returns opacity for hover effect on a tab with `range_parameter` between
  // 0 and 1, where 0 gives the minimum opacity suitable for wider tabs and 1
  // gives maximum opacity suitable for narrower tabs.
  virtual float GetHoverOpacityForTab(float range_parameter) const = 0;

  // Returns opacity for use on tab hover radial highlight.
  virtual float GetHoverOpacityForRadialHighlight() const = 0;

  // Returns TabGroup of the given `group`.
  virtual TabGroup* GetTabGroup(const tab_groups::TabGroupId& id) const = 0;

  // Returns the displayed title of the given `group`.
  virtual std::u16string GetGroupTitle(
      const tab_groups::TabGroupId& group) const = 0;

  // Returns the string describing the contents of the given `group`.
  virtual std::u16string GetGroupContentString(
      const tab_groups::TabGroupId& group) const = 0;

  // Returns the color ID of the given `group`.
  virtual tab_groups::TabGroupColorId GetGroupColorId(
      const tab_groups::TabGroupId& group) const = 0;

  // Returns the `group` collapsed state. Returns false if the group does not
  // exist or is not collapsed.
  // NOTE: This method signature is duplicated in TabContainerController; the
  // methods are intended to have equivalent semantics so they can share an
  // implementation.
  virtual bool IsGroupCollapsed(const tab_groups::TabGroupId& group) const = 0;

  // Returns the actual painted color of the given `group`, which depends on the
  // current theme.
  virtual SkColor GetPaintedGroupColor(
      const tab_groups::TabGroupColorId& color_id) const = 0;

  // Attempts to move the specified group to the left.
  virtual void ShiftGroupLeft(const tab_groups::TabGroupId& group) = 0;

  // Attempts to move the specified group to the right.
  virtual void ShiftGroupRight(const tab_groups::TabGroupId& group) = 0;

  virtual Browser* GetBrowser() = 0;

  // See BrowserFrameView::IsFrameCondensed().
  virtual bool IsFrameCondensed() const = 0;

#if BUILDFLAG(IS_CHROMEOS)
  // Returns whether the current app instance is locked for OnTask. Only
  // relevant for non-web browser scenarios.
  virtual bool IsLockedForOnTask() = 0;
#endif

 protected:
  virtual ~TabSlotController() = default;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_SLOT_CONTROLLER_H_
