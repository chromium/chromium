// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_PUBLIC_URL_GROUPING_TAB_EVENT_TRACKER_H_
#define COMPONENTS_VISITED_URL_RANKING_PUBLIC_URL_GROUPING_TAB_EVENT_TRACKER_H_

#include "components/visited_url_ranking/public/url_visit.h"
#include "ui/base/page_transition_types.h"

namespace visited_url_ranking {

class TabEventTracker {
 public:
  TabEventTracker() = default;
  virtual ~TabEventTracker() = default;

  TabEventTracker(const TabEventTracker&) = delete;
  TabEventTracker& operator=(const TabEventTracker&) = delete;

  // The action taken by the user in response to the suggestion.
  // GENERATED_JAVA_ENUM_PACKAGE: (
  // org.chromium.components.visited_url_ranking.url_grouping)
  // The reason for a tab to be selected. This value is platform specific. Extra
  // enums to be added for each platform.
  enum class TabSelectionType {
    // Selection of adjacent tab when the active tab is closed.
    kFromCloseActiveTab,
    // Selection of adjacent tab when the active tab is closed upon app exit.
    kFromAppExit,
    // Selection of newly created tab.
    kFromNewTab,
    // User-originated switch, or selection of last tab on startup.
    kFromUser,
    // Switch to existing tab from Omnibox tab suggestion.
    kFromOmnibox,
    // Selection of a previously closed tab when closure is undone.
    kFromUndoClosure,
    // Unknown reason.
    kUnknown,
  };

  // Called when a new tab is added.
  virtual void DidAddTab(int tab_id, int tab_launch_type) = 0;

  // Called when a tab is selected. Should be called at initialization time with
  // the active tab at startup.
  virtual void DidSelectTab(int tab_id,
                            const GURL& url,
                            TabSelectionType tab_selection_type,
                            int last_tab_id) = 0;

  // Called when a tab will be closed.
  virtual void WillCloseTab(int tab_id) = 0;

  // Called when a tab closure is undone.
  virtual void TabClosureUndone(int tab_id) = 0;

  // Called when a tab closure is committed.
  virtual void TabClosureCommitted(int tab_id) = 0;

  // Called when a tab is moved in the window.
  virtual void DidMoveTab(int tab_id, int new_index, int current_index) = 0;

  // Called when user-initiated page navigation finishes on any candidate tab.
  virtual void OnDidFinishNavigation(int tab_id,
                                     ui::PageTransition page_transition) = 0;

  // Called when users enter tab switcher.
  virtual void DidEnterTabSwitcher() = 0;
};

}  // namespace visited_url_ranking

#endif  // COMPONENTS_VISITED_URL_RANKING_PUBLIC_URL_GROUPING_TAB_EVENT_TRACKER_H_
