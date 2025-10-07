// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_LIST_INTERFACE_H_
#define CHROME_BROWSER_UI_TABS_TAB_LIST_INTERFACE_H_

#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "base/scoped_observation_traits.h"
#include "build/android_buildflags.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_interface.h"
#include "url/gurl.h"

// BrowserWindowInterface is available on desktop Android, but not other Android
// builds.
#if !BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_DESKTOP_ANDROID)
class BrowserWindowInterface;
#endif

class SessionID;
class TabListInterfaceObserver;

// Interface for supporting a basic set of tab operations on Android and
// Desktop.
class TabListInterface {
 public:
  TabListInterface() = default;
  virtual ~TabListInterface() = default;

  TabListInterface(const TabListInterface& other) = delete;
  void operator=(const TabListInterface& other) = delete;

#if !BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_DESKTOP_ANDROID)
  // Returns the TabListInterface associated with the given `browser`.
  static TabListInterface* From(BrowserWindowInterface* browser);
#endif

  // Adds / removes observers from this tab list.
  virtual void AddTabListInterfaceObserver(
      TabListInterfaceObserver* observer) = 0;
  virtual void RemoveTabListInterfaceObserver(
      TabListInterfaceObserver* observer) = 0;

  // Returns the count of tabs within the tab list.
  virtual int GetTabCount() const = 0;

  // Returns the index of the currently-active tab. Note that this is different
  // from the selected tab (of which there may be multiple).
  virtual int GetActiveIndex() const = 0;

  // Returns the `TabInterface` for the currently-active tab.
  virtual tabs::TabInterface* GetActiveTab() = 0;

  // Opens a new tab to the given `url`, inserting it at `index` in the tab
  // strip. `index` may be ignored by the implementation if necessary.
  virtual tabs::TabInterface* OpenTab(const GURL& url, int index) = 0;

  // Attempts to discard the renderer for the `tab` from memory.
  //
  // For details refer to:
  // docs/website/site/chromium-os/chromiumos-design-docs/tab-discarding-and-reloading/index.md
  virtual void DiscardTab(tabs::TabHandle tab) = 0;

  // Duplicates the `tab` to the next adjacent index. Returns the newly-
  // created tab.
  virtual tabs::TabInterface* DuplicateTab(tabs::TabHandle tab) = 0;

  // Returns the `TabInterface` for the tab at a given `index`. May be `nullptr`
  // if the index is out-of-bounds.
  virtual tabs::TabInterface* GetTab(int index) = 0;

  // Returns the index of the given `tab`, if it exists in the tab strip.
  // Otherwise, returns -1.
  virtual int GetIndexOfTab(tabs::TabHandle tab) = 0;

  // Highlights a set of tabs, adding them to the multi-selection set and
  // activating one of them. This is an additive operation; it does not clear
  // other currently selected tabs. The `tab_to_activate` becomes the active
  // tab. The `tab_to_activate` must be present in `tabs`.
  virtual void HighlightTabs(tabs::TabHandle tab_to_activate,
                             const std::set<tabs::TabHandle>& tabs) = 0;

  // Moves the `tab` to `index`. The nearest valid index will be used.
  virtual void MoveTab(tabs::TabHandle tab, int index) = 0;

  // Closes the `tab`.
  virtual void CloseTab(tabs::TabHandle tab) = 0;

  // Returns an in-order list of all tabs in the tab strip.
  virtual std::vector<tabs::TabInterface*> GetAllTabs() = 0;

  // Pins the `tab`. Pinning a pinned tab has no effect. This may result in
  // moving the tab if necessary.
  virtual void PinTab(tabs::TabHandle tab) = 0;

  // Unpins the `tab`. Unpinning an unpinned tab has no effect. This may result
  // in moving the tab if necessary.
  virtual void UnpinTab(tabs::TabHandle tab) = 0;

  // Adds `tabs` to the `group_id` if provided or creates a new tab group.
  // Returns the tab group ID of the created or added to group. Tabs will be
  // moved as necessary to make the group contiguous. Pinned tabs will no longer
  // be pinned, and tabs that were in other groups will be removed from those
  // groups. Will no-op and return nullopt if the provided `group_id` is not an
  // existing tab group.
  virtual std::optional<tab_groups::TabGroupId> AddTabsToGroup(
      std::optional<tab_groups::TabGroupId> group_id,
      const std::set<tabs::TabHandle>& tabs) = 0;

  // Ungroups all `tabs`. Tabs will be moved to an index adjacent to the group
  // they were in.
  virtual void Ungroup(const std::set<tabs::TabHandle>& tabs) = 0;

  // Moves the tab group to `index`. The nearest valid index will be used.
  virtual void MoveGroupTo(tab_groups::TabGroupId group_id, int index) = 0;

  // Moves `tab` from this TabListInterface to the TabListInterface associated
  // with `destination_window_id`. The tab will be inserted at `index` in the
  // destination tab list. This will no-op if the tab is not present in this
  // TabListInterface or the destination window does not exist. `index` may be
  // adjusted as necessary to ensure the tab is in a valid position.
  virtual void MoveTabToWindow(tabs::TabHandle tab,
                               SessionID destination_window_id,
                               int destination_index) = 0;

  // Moves the tab group with `group_id` from this TabListInterface to the
  // TabListInterface associated with `destination_window_id`. The tab group
  // will be inserted with the first tab at `index` in the destination tab list.
  // This will no-op if the tab group is not present in this TabListInterface or
  // the destination window does not exist. `index` may be adjusted as necessary
  // to ensure the tab group is in a valid position.
  virtual void MoveTabGroupToWindow(tab_groups::TabGroupId group_id,
                                    SessionID destination_window_id,
                                    int destination_index) = 0;
};

namespace base {

template <>
struct ScopedObservationTraits<TabListInterface, TabListInterfaceObserver> {
  static void AddObserver(TabListInterface* tab_list,
                          TabListInterfaceObserver* observer) {
    tab_list->AddTabListInterfaceObserver(observer);
  }
  static void RemoveObserver(TabListInterface* tab_list,
                             TabListInterfaceObserver* observer) {
    tab_list->RemoveTabListInterfaceObserver(observer);
  }
};

}  // namespace base

#endif  // CHROME_BROWSER_UI_TABS_TAB_LIST_INTERFACE_H_
