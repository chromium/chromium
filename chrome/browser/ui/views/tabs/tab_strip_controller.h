// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_CONTROLLER_H_

#include <vector>

#include "base/strings/string16.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip_types.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ui_base_types.h"

class Tab;
class TabGroupVisualData;
class TabGroupId;
class TabStrip;

namespace gfx {
class Point;
}

namespace ui {
class Event;
class ListSelectionModel;
}

// Model/Controller for the TabStrip.
// NOTE: All indices used by this class are in model coordinates.
class TabStripController {
 public:
  virtual ~TabStripController() {}

  // Returns the selection model of the tabstrip.
  virtual const ui::ListSelectionModel& GetSelectionModel() const = 0;

  // Returns the number of tabs in the model.
  virtual int GetCount() const = 0;

  // Returns true if |index| is a valid model index.
  virtual bool IsValidIndex(int index) const = 0;

  // Returns true if the tab at |index| is the active tab. The active tab is the
  // one whose content is shown.
  virtual bool IsActiveTab(int index) const = 0;

  // Returns the index of the active tab.
  virtual int GetActiveIndex() const = 0;

  // Returns true if the selected index is selected.
  virtual bool IsTabSelected(int index) const = 0;

  // Returns true if the selected index is pinned.
  virtual bool IsTabPinned(int index) const = 0;

  // Select the tab at the specified index in the model.
  // |event| is the input event that triggers the tab selection.
  virtual void SelectTab(int index, const ui::Event& event) = 0;

  // Extends the selection from the anchor to the specified index in the model.
  virtual void ExtendSelectionTo(int index) = 0;

  // Toggles the selection of the specified index in the model.
  virtual void ToggleSelected(int index) = 0;

  // Adds the selection the anchor to |index|.
  virtual void AddSelectionFromAnchorTo(int index) = 0;

  // Prepares to close a tab. If closing the tab might require (for example) a
  // user prompt, triggers that prompt and returns false, indicating that the
  // current close operation should not proceed. If this method returns true,
  // closing can proceed.
  virtual bool BeforeCloseTab(int index, CloseTabSource source) = 0;

  // Closes the tab at the specified index in the model.
  virtual void CloseTab(int index, CloseTabSource source) = 0;

  // Ungroups the tabs at the specified index in the model.
  virtual void UngroupAllTabsInGroup(TabGroupId group) = 0;

  // Adds a new tab to end of the tab group.
  virtual void AddNewTabInGroup(TabGroupId group) = 0;

  // Moves the tab at |start_index| so that it is now at |final_index|, sliding
  // any tabs in between left or right as appropriate.
  virtual void MoveTab(int start_index, int final_index) = 0;

  // Shows a context menu for the tab at the specified point in screen coords.
  virtual void ShowContextMenuForTab(Tab* tab,
                                     const gfx::Point& p,
                                     ui::MenuSourceType source_type) = 0;

  // Returns true if the associated TabStrip's delegate supports tab moving or
  // detaching. Used by the Frame to determine if dragging on the Tab
  // itself should move the window in cases where there's only one
  // non drag-able Tab.
  virtual int HasAvailableDragActions() const = 0;

  // Notifies controller of a drop index update.
  virtual void OnDropIndexUpdate(int index, bool drop_before) = 0;

  // Creates the new tab.
  virtual void CreateNewTab() = 0;

  // Creates a new tab, and loads |location| in the tab. If |location| is a
  // valid URL, then simply loads the URL, otherwise this can open a
  // search-result page for |location|.
  virtual void CreateNewTabWithLocation(const base::string16& location) = 0;

  // Invoked if the stacked layout (on or off) might have changed.
  virtual void StackedLayoutMaybeChanged() = 0;

  // Notifies controller that the user started dragging this tabstrip's tabs.
  virtual void OnStartedDragging() = 0;

  // Notifies controller that the user stopped dragging this tabstrip's tabs.
  // This is also called when the tabs that the user is dragging were detached
  // from this tabstrip but the user is still dragging the tabs.
  virtual void OnStoppedDragging() = 0;

  // Notifies controller that the index of the tab with keyboard focus changed
  // to |index|.
  virtual void OnKeyboardFocusedTabChanged(base::Optional<int> index) = 0;

  // Returns the TabGroupVisualData instance for the given |group|.
  virtual const TabGroupVisualData* GetVisualDataForGroup(
      TabGroupId group) const = 0;

  virtual void SetVisualDataForGroup(TabGroupId group,
                                     TabGroupVisualData visual_data) = 0;

  // Returns the list of tabs in the given |group|.
  virtual std::vector<int> ListTabsInGroup(TabGroupId group) const = 0;

  // Determines whether the top frame is condensed vertically, as when the
  // window is maximized. If true, the top frame is just the height of a tab,
  // rather than having extra vertical space above the tabs.
  virtual bool IsFrameCondensed() const = 0;

  // Returns whether the shapes of background tabs are visible against the
  // frame.
  virtual bool HasVisibleBackgroundTabShapes() const = 0;

  // Returns whether the shapes of background tabs are visible against the
  // frame for either active or inactive windows.
  virtual bool EverHasVisibleBackgroundTabShapes() const = 0;

  // Returnes whether the window frame is being painted as active. This
  // determines which colors are used in the tab strip.
  virtual bool ShouldPaintAsActiveFrame() const = 0;

  // Returns whether tab strokes can ever be drawn. If true, strokes will only
  // be drawn if necessary.
  virtual bool CanDrawStrokes() const = 0;

  // Returns the color of the browser frame for the given window activation
  // state.
  virtual SkColor GetFrameColor(BrowserFrameActiveState active_state) const = 0;

  // Returns COLOR_TOOLBAR_TOP_SEPARATOR[,_INACTIVE] depending on the activation
  // state of the window.
  virtual SkColor GetToolbarTopSeparatorColor() const = 0;

  // For non-transparent windows, returns the background tab image resource ID
  // if the image has been customized, directly or indirectly, by the theme.
  virtual base::Optional<int> GetCustomBackgroundId(
      BrowserFrameActiveState active_state) const = 0;

  // Returns the accessible tab name.
  virtual base::string16 GetAccessibleTabName(const Tab* tab) const = 0;

  // Returns the profile associated with the Tabstrip.
  virtual Profile* GetProfile() const = 0;

  virtual const Browser* GetBrowser() const = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_CONTROLLER_H_
