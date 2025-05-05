// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_LIST_INTERFACE_H_
#define CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_LIST_INTERFACE_H_

#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_interface.h"
#include "url/gurl.h"

// TODO(crbug.com/415323446): Move this file somewhere that is shared with
// desktop and give it an appropriate namespace.

// Interface for supporting a basic set of tab operations on Android and
// Desktop.
class TabListInterface {
 public:
  TabListInterface() = default;
  virtual ~TabListInterface() = default;

  TabListInterface(const TabListInterface& other) = delete;
  void operator=(const TabListInterface& other) = delete;

  // Opens a new tab to the given `url`, inserting it at `index` in the tab
  // strip. `index` may be ignored by the implementation if necessary.
  virtual void OpenTab(const GURL& url, int index) = 0;

  // Attempts to discard the renderer for the tab at the given `index` from
  // memory. An out-of- bounds `index` is ignored.
  //
  // For details refer to:
  // docs/website/site/chromium-os/chromiumos-design-docs/tab-discarding-and-reloading/index.md
  virtual void DiscardTab(int index) = 0;

  // Duplicates the tab at the given `index` to the next adjacent index. An
  // out-of-bounds `index` is ignored.
  virtual void DuplicateTab(int index) = 0;

  // Returns the `TabInterface` for the tab at a given `index`. May be `nullptr`
  // if the index is out-of-bounds.
  virtual tabs::TabInterface* GetTab(int index) = 0;

  // Highlights / selects the tabs at the given `indices`. Any out-of-bounds
  // index values are ignored.
  virtual void HighlightTabs(std::set<int> indices) = 0;

  // Moves the tab at `from_index` to `to_index`. The nearest valid index will
  // be used.
  virtual void MoveTab(int from_index, int to_index) = 0;

  // Closes the tab at `index`. An out-of-bounds `index` is ignored.
  virtual void CloseTab(int index) = 0;

  // Returns an in-order list of all tabs in the tab strip.
  virtual std::vector<tabs::TabInterface*> GetAllTabs() = 0;

  // Pins the tab at `index`. Pinning a pinned tab has no effect. This may
  // result in moving the tab if necessary.
  virtual void PinTab(int index) = 0;

  // Unpins the tab at `index`. Unpinning an unpinned tab has no effect. This
  // may result in moving the tab if necessary.
  virtual void UnpinTab(int index) = 0;

  // Creates a new tab group with the tabs at the given `indices`. Tabs will be
  // moved as necessary to make the group contiguous. Pinned tabs will no longer
  // be pinned, tabs that were in other groups will be removed from those
  // groups. Will return nullopt if all indices are invalid or groups are not
  // supported.
  virtual std::optional<tab_groups::TabGroupId> CreateGroup(
      std::set<int> indices) = 0;

  // Moves the tab group to `index`. The nearest valid index will be used.
  virtual void MoveGroupTo(tab_groups::TabGroupId group_id, int index) = 0;

  // TODO(crbug.com/415323446): Figure out a memory management model that works
  // for both Android and Desktop for the following methods.

  // Detaches the tab at a given `index` allowing the caller to reparent it to a
  // different tab strip. May return `nullptr` if index is out-of-bounds.
  // virtual std::unique_ptr<TabInterface> DetachTabAt(int index) = 0;

  // Inserts the given `tab` at the given `index`. The nearest valid index will
  // be used.
  // virtual void InsertTabAt(std::unique_ptr<TabInterface>, int index) = 0;

  // Detaches the tab group with the given `group_id` to be attached to a
  // different window.
  // virtual std::unique_ptr<TabGroup> DetachTabGroup(TabGroupId group_id) = 0;

  // Inserts a previously deteached `tab_group` to `index`. The nearest valid
  // index will be used.
  // virtual std::unique_ptr<TabGroup> DetachTabGroup(TabGroupId group_id) = 0;
};

#endif  // CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_LIST_INTERFACE_H_
