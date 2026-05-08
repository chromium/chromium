// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_group_data.h"

#include <algorithm>
#include <optional>
#include <string>
#include <unordered_set>

#include "base/check_op.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/tab_group_features.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/tabs/hovercard/hover_card_anchor_target.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"

namespace {
tabs::TabGroupTabData GetTabData(tabs::TabInterface* tab_interface) {
  TabUIHelper* const tab_ui_helper = TabUIHelper::From(tab_interface);
  tabs::TabGroupTabData tab_group_tab_data;
  tab_group_tab_data.last_committed_url = tab_ui_helper->GetLastCommittedURL();
  tab_group_tab_data.visible_url = tab_ui_helper->GetVisibleURL();
  tab_group_tab_data.should_display_url = tab_ui_helper->ShouldDisplayURL();
  tab_group_tab_data.is_crashed = tab_ui_helper->IsCrashed();
  tab_group_tab_data.title = tab_ui_helper->GetTitle();
  return tab_group_tab_data;
}
}  // namespace

namespace tabs {

TabGroupTabData::TabGroupTabData() = default;
TabGroupTabData::TabGroupTabData(const TabGroupTabData& other) = default;
TabGroupTabData& TabGroupTabData::operator=(const TabGroupTabData& other) =
    default;
TabGroupTabData::TabGroupTabData(TabGroupTabData&& other) = default;
TabGroupTabData& TabGroupTabData::operator=(TabGroupTabData&& other) = default;
TabGroupTabData::~TabGroupTabData() = default;
bool TabGroupTabData::operator==(const TabGroupTabData& other) const = default;

TabGroupData::TabGroupData() = default;
TabGroupData::~TabGroupData() = default;
TabGroupData::TabGroupData(const TabGroupData&) = default;
TabGroupData& TabGroupData::operator=(const TabGroupData&) = default;
TabGroupData::TabGroupData(TabGroupData&&) = default;
TabGroupData& TabGroupData::operator=(TabGroupData&&) = default;

class TabGroupDataObserver::TabDataObserver {
 public:
  TabDataObserver(TabInterface* tab_interface,
                  base::RepeatingClosure on_data_change_callback)
      : tab_interface_(tab_interface),
        on_data_change_callback_(on_data_change_callback) {
    tab_ui_change_subscription_ =
        TabUIHelper::From(tab_interface)
            ->AddTabUIChangeCallback(base::BindRepeating(
                &TabDataObserver::OnTabDataUpdated, base::Unretained(this)));
    tab_group_tab_data_ = GetTabData(tab_interface_);
  }

  ~TabDataObserver() = default;

  const TabGroupTabData& tab_group_tab_data() const {
    return tab_group_tab_data_;
  }

 private:
  void OnTabDataUpdated() {
    TabGroupTabData updated_tab_group_data = GetTabData(tab_interface_);

    if (tab_group_tab_data_ != updated_tab_group_data) {
      tab_group_tab_data_ = updated_tab_group_data;
      on_data_change_callback_.Run();
    }
  }

  TabGroupTabData tab_group_tab_data_;
  raw_ptr<TabInterface> tab_interface_;
  base::CallbackListSubscription tab_ui_change_subscription_;
  base::RepeatingClosure on_data_change_callback_;
};

TabGroupDataObserver::TabGroupDataObserver(TabGroup* group)
    : tab_group_(group) {
  tab_group_data_.visual_data = *tab_group_->visual_data();
  tab_group_data_.num_tabs_in_group = tab_group_->tab_count();
  RefreshTabData();
  attention_indicator_observation_.Observe(
      tab_group_->GetTabGroupFeatures()->attention_indicator());

  group_changed_subscription_ =
      tab_group_->RegisterOnGroupChanged(base::BindRepeating(
          &TabGroupDataObserver::OnGroupChanged, base::Unretained(this)));
  visual_data_subscription_ =
      tab_group_->RegisterOnVisualDataChanged(base::BindRepeating(
          &TabGroupDataObserver::OnVisualDataChanged, base::Unretained(this)));
}

TabGroupDataObserver::~TabGroupDataObserver() = default;

base::CallbackListSubscription
TabGroupDataObserver::RegisterTabGroupDataChangedCallback(
    base::RepeatingClosure callback) {
  return tab_group_data_changed_callback_list_.Add(std::move(callback));
}

void TabGroupDataObserver::OnVisualDataChanged() {
  tab_group_data_.visual_data = *tab_group_->visual_data();
  tab_group_data_changed_callback_list_.Notify();
}

void TabGroupDataObserver::OnAttentionStateChanged() {
  tab_group_data_.needs_attention = tab_group_->GetTabGroupFeatures()
                                        ->attention_indicator()
                                        ->GetHasAttention();
  tab_group_data_.is_sharing_group = IsTabGroupShared();
  tab_group_data_changed_callback_list_.Notify();
}

void TabGroupDataObserver::OnGroupChanged() {
  const bool num_tab_change =
      tab_group_data_.num_tabs_in_group != tab_group_->tab_count();
  if (num_tab_change) {
    tab_group_data_.num_tabs_in_group = tab_group_->tab_count();
  }

  const bool tab_data_change = RefreshTabData();
  if (num_tab_change || tab_data_change) {
    tab_group_data_changed_callback_list_.Notify();
  }
}

void TabGroupDataObserver::OnTabDataChanged() {
  if (RefreshTabData()) {
    tab_group_data_changed_callback_list_.Notify();
  }
}

bool TabGroupDataObserver::RefreshTabData() {
  std::vector<tabs::TabInterface*> tabs;
  tabs::TabInterface* const first_tab_in_group = tab_group_->GetFirstTab();
  if (first_tab_in_group) {
    TabStripModel* const tab_strip_model =
        first_tab_in_group->GetBrowserWindowInterface()->GetTabStripModel();
    tabs =
        tab_strip_model->GetTabsAtIndices(tab_group_->ListTabs().ToIntVector());
  }

  std::vector<TabGroupTabData> tab_data;
  std::unordered_set<tabs::TabInterface*> tabs_to_observe;
  const size_t min_tab_count =
      std::min(static_cast<size_t>(tab_group_->tab_count()), tabs.size());
  const size_t num_tabs_to_show =
      std::min(min_tab_count, TabGroupData::kMaxTabs);
  CHECK_LE(num_tabs_to_show, tabs.size());
  for (size_t i = 0; i < num_tabs_to_show; i++) {
    tabs::TabInterface* const tab = tabs[i];
    if (!tab_data_observers_.contains(tab)) {
      tab_data_observers_[tab] = std::make_unique<TabDataObserver>(
          tab, base::BindRepeating(&TabGroupDataObserver::OnTabDataChanged,
                                   base::Unretained(this)));
    }
    tab_data.push_back(tab_data_observers_[tab]->tab_group_tab_data());
    tabs_to_observe.insert(tab);
  }

  // Remove observers for tabs that are no longer in the group.
  std::erase_if(tab_data_observers_, [&](const auto& pair) {
    return !tabs_to_observe.contains(pair.first);
  });

  const bool changed = tab_group_data_.tab_data != tab_data;
  if (changed) {
    tab_group_data_.tab_data = std::move(tab_data);
  }
  return changed;
}

bool TabGroupDataObserver::IsTabGroupShared() {
  tabs::TabInterface* const first_tab_in_group = tab_group_->GetFirstTab();
  if (!first_tab_in_group) {
    return false;
  }

  Profile* const profile =
      first_tab_in_group->GetBrowserWindowInterface()->GetProfile();
  tab_groups::TabGroupSyncService* const tab_group_sync_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile);

  if (!tab_group_sync_service) {
    return false;
  }

  std::optional<tab_groups::SavedTabGroup> saved_group =
      tab_group_sync_service->GetGroup(tab_group_->id());
  return saved_group.has_value() && saved_group.value().is_shared_tab_group();
}

}  // namespace tabs
