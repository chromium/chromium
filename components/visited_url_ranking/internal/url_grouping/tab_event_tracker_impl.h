// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_INTERNAL_URL_GROUPING_TAB_EVENT_TRACKER_IMPL_H_
#define COMPONENTS_VISITED_URL_RANKING_INTERNAL_URL_GROUPING_TAB_EVENT_TRACKER_IMPL_H_

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "components/visited_url_ranking/public/url_grouping/tab_event_tracker.h"
#include "ui/base/page_transition_types.h"

namespace visited_url_ranking {

// TabEventTracker implementation that sends events to storage and triggers
// suggestion computation.
class TabEventTrackerImpl : public TabEventTracker {
 public:
  static const char kAndroidNativeNewTabPageURL[];

  using OnNewEventCallback = base::RepeatingClosure;
  explicit TabEventTrackerImpl(OnNewEventCallback trigger_event_callback,
                               OnNewEventCallback invalidate_cache_callback);
  ~TabEventTrackerImpl() override;

  TabEventTrackerImpl(const TabEventTrackerImpl&) = delete;
  TabEventTrackerImpl& operator=(const TabEventTrackerImpl&) = delete;

  // TabEventTracker impl:
  void DidAddTab(int tab_id, int tab_launch_type) override;
  void DidSelectTab(int tab_id,
                    const GURL& url,
                    TabSelectionType tab_selection_type,
                    int last_tab_id) override;
  void WillCloseTab(int tab_id) override;
  void TabClosureUndone(int tab_id) override;
  void TabClosureCommitted(int tab_id) override;
  void DidMoveTab(int tab_id, int new_index, int current_index) override;
  void OnDidFinishNavigation(int tab_id,
                             ui::PageTransition page_transition) override;
  void DidEnterTabSwitcher() override;

  int GetSelectedCount(int tab_id) const;

 private:
  struct TabSelection {
    TabSelection();
    TabSelection(int tab_id,
                 TabSelectionType tab_selection_type,
                 base::Time time);
    ~TabSelection();

    int tab_id{-1};
    TabSelectionType tab_selection_type{TabSelectionType::kUnknown};
    base::Time time;
    // Whether this selection has been committed to the the TabEventTracker to
    // store.
    bool committed{false};
  };

  std::map<int, std::vector<TabSelection>> tab_id_selection_map_;
  std::set<int> closing_tabs_;
  std::optional<TabSelection> current_selection_;
  OnNewEventCallback trigger_event_callback_;
  OnNewEventCallback invalidate_cache_callback_;
  const bool tab_switcher_trigger_only_;
  const bool trigger_on_navigation_;
};

}  // namespace visited_url_ranking

#endif  // COMPONENTS_VISITED_URL_RANKING_INTERNAL_URL_GROUPING_TAB_EVENT_TRACKER_IMPL_H_
