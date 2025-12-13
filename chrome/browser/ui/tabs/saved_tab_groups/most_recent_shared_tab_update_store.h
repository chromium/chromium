// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_MOST_RECENT_SHARED_TAB_UPDATE_STORE_H_
#define CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_MOST_RECENT_SHARED_TAB_UPDATE_STORE_H_

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/saved_tab_groups/public/types.h"

class BrowserView;

namespace ui {
class TrackedElement;
}  // namespace ui

namespace tab_groups {

using TabIdentifiers = std::pair<LocalTabGroupID, std::optional<LocalTabID>>;

// Data store used to temporarily store contextual data with
// asynchronous messaging systems (i.e. User Education, Toasts).
//
// The most recent local update to a Shared Tab Group is stored here
// and may trigger showing a promo. The promo can then retrieve the data
// from the store in order to know where to anchor.
class MostRecentSharedTabUpdateStore {
 public:
  explicit MostRecentSharedTabUpdateStore(
      BrowserWindowInterface* browser_window);
  MostRecentSharedTabUpdateStore(const MostRecentSharedTabUpdateStore&) =
      delete;
  MostRecentSharedTabUpdateStore& operator=(
      const MostRecentSharedTabUpdateStore& other) = delete;
  ~MostRecentSharedTabUpdateStore();

  // Returns whether |tab_identifiers| has been set.
  bool HasUpdate() { return last_updated_tab_.has_value(); }

  // Gets the identifiers of the most recent locally-updated shared tab.
  std::optional<TabIdentifiers> GetLastUpdatedTab() {
    return last_updated_tab_;
  }

  // Sets the identifiers for a recent local update to a tab. Passing
  // nullopt for tab_id indicates that the update was removing a tab
  // from this tab group.
  void SetLastUpdatedTab(LocalTabGroupID group_id,
                         std::optional<LocalTabID> tab_id);

  // Returns the anchor for shared tab activity as a TrackedElement.
  ui::TrackedElement* GetIPHAnchor(BrowserView* browser_view);

 private:
  // Attempt to trigger the IPH after a relevant change.
  void MaybeShowPromo(const base::Feature& feature);

  const raw_ptr<BrowserWindowInterface> browser_window_;

  // The most recent local update to a tab within this browser window.
  // LocalTabID will be null in the case where the user removed the tab.
  std::optional<TabIdentifiers> last_updated_tab_ = std::nullopt;
};

}  // namespace tab_groups

#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_MOST_RECENT_SHARED_TAB_UPDATE_STORE_H_
