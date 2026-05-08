// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_GROUP_DATA_H_
#define CHROME_BROWSER_UI_TABS_TAB_GROUP_DATA_H_

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/tabs/tab_group_attention_indicator.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_group.h"
#include "url/gurl.h"

namespace tabs {
class TabInterface;

// Holds the data for a single tab in a tab group that is used by the
// tab group hover card.
struct TabGroupTabData {
  TabGroupTabData();
  TabGroupTabData(const TabGroupTabData& other);
  TabGroupTabData& operator=(const TabGroupTabData& other);
  TabGroupTabData(TabGroupTabData&& other);
  TabGroupTabData& operator=(TabGroupTabData&& other);
  ~TabGroupTabData();
  bool operator==(const TabGroupTabData& other) const;

  GURL last_committed_url;
  GURL visible_url;
  bool should_display_url = true;
  bool is_crashed = false;
  std::u16string title;
};

// Holds the data for a tab group that is used by the tab group hover card.
struct TabGroupData {
  TabGroupData();
  ~TabGroupData();
  TabGroupData(const TabGroupData&);
  TabGroupData& operator=(const TabGroupData&);
  TabGroupData(TabGroupData&&);
  TabGroupData& operator=(TabGroupData&&);

  bool needs_attention = false;
  bool is_sharing_group = false;
  std::vector<TabGroupTabData> tab_data;
  tab_groups::TabGroupVisualData visual_data;
  int num_tabs_in_group = 0;

  // Maximum number of tab data observed for a tab group.
  static constexpr size_t kMaxTabs = 5;
};

// Caches various data about a tab group and notifies subscribers when any of
// the data property updates.
class TabGroupDataObserver : public TabGroupAttentionIndicator::Observer {
 public:
  explicit TabGroupDataObserver(TabGroup* group);
  ~TabGroupDataObserver() override;

  // Notifies subscribers when the data needed for group hover cards has
  // updated.
  base::CallbackListSubscription RegisterTabGroupDataChangedCallback(
      base::RepeatingClosure callback);

  const TabGroupData& tab_group_data() const { return tab_group_data_; }

  // TabGroupAttentionIndicator::Observer:
  void OnAttentionStateChanged() override;

 private:
  class TabDataObserver;

  void OnTabDataChanged();
  bool RefreshTabData();
  bool IsTabGroupShared();

  void OnGroupChanged();
  void OnVisualDataChanged();

  raw_ptr<TabGroup> tab_group_;
  std::unordered_map<TabInterface*, std::unique_ptr<TabDataObserver>>
      tab_data_observers_;
  base::RepeatingClosureList tab_group_data_changed_callback_list_;
  TabGroupData tab_group_data_;
  base::ScopedObservation<TabGroupAttentionIndicator,
                          TabGroupAttentionIndicator::Observer>
      attention_indicator_observation_{this};
  base::CallbackListSubscription group_changed_subscription_;
  base::CallbackListSubscription visual_data_subscription_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_TAB_GROUP_DATA_H_
