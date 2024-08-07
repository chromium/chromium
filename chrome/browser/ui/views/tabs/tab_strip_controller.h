// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_CONTROLLER_H_

#include <optional>
#include <string>
#include <vector>

#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip_types.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/range/range.h"

class Browser;
class Tab;
class TabStrip;

namespace gfx {
class Point;
}

namespace tab_groups {
enum class TabGroupColorId;
class TabGroupId;
class TabGroupVisualData;
}  // namespace tab_groups

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

  // Returns the index of the active tab, or nullopt if no tab is active.
  virtual std::optional<int> GetActiveIndex() const = 0;

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
  // user prompt, triggers that prompt passing in the callback ownership to it.
  // Otherwise it runs the callback.
  virtual void OnCloseTab(int index,
                          CloseTabSource source,
                          base::OnceCallback<void()> callback) = 0;

  // Closes the tab at the specified index in the model.
  virtual void CloseTab(int index) = 0;

  // Toggles audio muting for the tab at the specified index in the model.
  virtual void ToggleTabAudioMute(int index) = 0;

  // Adds a tab to an existing tab group.
  virtual void AddTabToGroup(int model_index,
                             const tab_groups::TabGroupId& group) = 0;

  // Removes a tab from its tab group.
  virtual void RemoveTabFromGroup(int model_index) = 0;

  // Moves the tab at |start_index| so that it is now at |final_index|, sliding
  // any tabs in between left or right as appropriate.
  virtual void MoveTab(int start_index, int final_index) = 0;

  // Moves all the tabs in |group| so that it is now at |final_index|, sliding
  // any tabs in between left or right as appropriate.
  virtual void MoveGroup(const tab_groups::TabGroupId& group,
                         int final_index) = 0;

  // Toggles the collapsed state of `group`. Collapsed becomes expanded.
  // Expanded becomes collapsed. `origin` should be used to denote the way in
  // which the tab group was toggled (Ex: From a context menu, mouse press,
  // touch/gesture control, etc). Tests will default to `kMenuAction` unless
  // specified otherwise.
  virtual void ToggleTabGroupCollapsedState(
      const tab_groups::TabGroupId group,
      ToggleTabGroupCollapsedStateOrigin origin =
          ToggleTabGroupCollapsedStateOrigin::kMenuAction) = 0;

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
  virtual void OnDropIndexUpdate(std::optional<int> index,
                                 bool drop_before) = 0;

  // Creates the new tab.
  virtual void CreateNewTab() = 0;

  // Creates a new tab, and loads |location| in the tab. If |location| is a
  // valid URL, then simply loads the URL, otherwise this can open a
  // search-result page for |location|.
  virtual void CreateNewTabWithLocation(const std::u16string& location) = 0;

  // Notifies controller that the user started dragging this tabstrip's tabs.
  // |dragging_window| indicates if the whole window is moving, or if tabs are
  // moving within a window.
  virtual void OnStartedDragging(bool dragging_window) = 0;

  // Notifies controller that the user stopped dragging this tabstrip's tabs.
  // This is also called when the tabs that the user is dragging were detached
  // from this tabstrip but the user is still dragging the tabs.
  virtual void OnStoppedDragging() = 0;

  // Notifies controller that the index of the tab with keyboard focus changed
  // to |index|.
  virtual void OnKeyboardFocusedTabChanged(std::optional<int> index) = 0;

  // Returns the title of the given |group|.
  virtual std::u16string GetGroupTitle(
      const tab_groups::TabGroupId& group) const = 0;

  // Returns the string describing the contents of the given |group|.
  virtual std::u16string GetGroupContentString(
      const tab_groups::TabGroupId& group) const = 0;

  // Returns the color ID of the given |group|.
  virtual tab_groups::TabGroupColorId GetGroupColorId(
      const tab_groups::TabGroupId& group) const = 0;

  // Returns the |group| collapsed state. Returns false if the group does not
  // exist or is not collapsed.
  virtual bool IsGroupCollapsed(const tab_groups::TabGroupId& group) const = 0;

  // Sets the title and color ID of the given |group|.
  virtual void SetVisualDataForGroup(
      const tab_groups::TabGroupId& group,
      const tab_groups::TabGroupVisualData& visual_data) = 0;

  // Gets the first tab index in |group|, or nullopt if the group is
  // currently empty. This is always safe to call unlike
  // ListTabsInGroup().
  virtual std::optional<int> GetFirstTabInGroup(
      const tab_groups::TabGroupId& group) const = 0;

  // Returns the range of tabs in the given |group|. This must not be
  // called during intermediate states where the group is not
  // contiguous. For example, if tabs elsewhere in the tab strip are
  // being moved into |group| it may not be contiguous; this method
  // cannot be called.
  virtual gfx::Range ListTabsInGroup(
      const tab_groups::TabGroupId& group) const = 0;

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

  // Returns whether tab strokes can ever be drawn. If true, strokes will only
  // be drawn if necessary.
  virtual bool CanDrawStrokes() const = 0;

  virtual bool IsFrameButtonsRightAligned() const = 0;

  // Returns the color of the browser frame for the given window activation
  // state.
  virtual SkColor GetFrameColor(BrowserFrameActiveState active_state) const = 0;

  // For non-transparent windows, returns the background tab image resource ID
  // if the image has been customized, directly or indirectly, by the theme.
  virtual std::optional<int> GetCustomBackgroundId(
      BrowserFrameActiveState active_state) const = 0;

  // Returns the accessible tab name.
  virtual std::u16string GetAccessibleTabName(const Tab* tab) const = 0;

  // Returns the profile associated with the Tabstrip.
  virtual Profile* GetProfile() const = 0;

  virtual const Browser* GetBrowser() const = 0;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Returns whether the current app instance is locked for OnTask. Only
  // relevant for non-web browser scenarios.
  virtual bool IsLockedForOnTask() = 0;
#endif
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_CONTROLLER_H_
