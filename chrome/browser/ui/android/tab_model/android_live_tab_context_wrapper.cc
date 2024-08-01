// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_model/android_live_tab_context_wrapper.h"

#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "components/sessions/content/content_live_tab.h"
#include "components/sessions/core/tab_restore_types.h"

AndroidLiveTabContextCloseWrapper::AndroidLiveTabContextCloseWrapper(
    TabModel* tab_model,
    std::vector<raw_ptr<TabAndroid, VectorExperimental>>&& closed_tabs,
    std::map<int, tab_groups::TabGroupId>&& tab_id_to_tab_group,
    std::map<tab_groups::TabGroupId, tab_groups::TabGroupVisualData>&&
        tab_group_visual_data,
    std::map<tab_groups::TabGroupId, std::optional<base::Uuid>>&&
        saved_tab_group_ids,
    std::vector<WebContentsStateByteBuffer>&& web_contents_state)
    : AndroidLiveTabContext(tab_model),
      closed_tabs_(closed_tabs),
      tab_id_to_tab_group_(tab_id_to_tab_group),
      tab_group_visual_data_(tab_group_visual_data),
      saved_tab_group_ids_(saved_tab_group_ids),
      web_contents_state_(std::move(web_contents_state)) {}

AndroidLiveTabContextCloseWrapper::~AndroidLiveTabContextCloseWrapper() =
    default;

int AndroidLiveTabContextCloseWrapper::GetTabCount() const {
  return closed_tabs_.size();
}

int AndroidLiveTabContextCloseWrapper::GetSelectedIndex() const {
  return 0;
}

sessions::LiveTab* AndroidLiveTabContextCloseWrapper::GetLiveTabAt(
    int relative_index) const {
  DCHECK_LT(base::checked_cast<size_t>(relative_index),
            web_contents_state_.size());
  scoped_web_contents_ = historical_tab_saver::ScopedWebContents::CreateForTab(
      GetTabAt(relative_index), &web_contents_state_[relative_index]);
  DCHECK(scoped_web_contents_->web_contents());
  return sessions::ContentLiveTab::GetForWebContents(
      scoped_web_contents_->web_contents());
}

std::optional<tab_groups::TabGroupId>
AndroidLiveTabContextCloseWrapper::GetTabGroupForTab(int relative_index) const {
  auto it = tab_id_to_tab_group_.find(GetTabAt(relative_index)->GetAndroidId());
  return it != tab_id_to_tab_group_.end()
             ? it->second
             : std::optional<tab_groups::TabGroupId>();
}

const tab_groups::TabGroupVisualData*
AndroidLiveTabContextCloseWrapper::GetVisualDataForGroup(
    const tab_groups::TabGroupId& group_id) const {
  auto it = tab_group_visual_data_.find(group_id);
  return it == tab_group_visual_data_.end() ? nullptr : &it->second;
}

const std::optional<base::Uuid>
AndroidLiveTabContextCloseWrapper::GetSavedTabGroupIdForGroup(
    const tab_groups::TabGroupId& group_id) const {
  auto it = saved_tab_group_ids_.find(group_id);
  return it == saved_tab_group_ids_.end() ? std::nullopt : it->second;
}

TabAndroid* AndroidLiveTabContextCloseWrapper::GetTabAt(
    int relative_index) const {
  DCHECK_LT(base::checked_cast<size_t>(relative_index), closed_tabs_.size());
  auto* tab_android = closed_tabs_[relative_index].get();
  DCHECK(tab_android);
  return tab_android;
}

AndroidLiveTabContextRestoreWrapper::AndroidLiveTabContextRestoreWrapper(
    TabModel* tab_model)
    : AndroidLiveTabContext(tab_model) {}
AndroidLiveTabContextRestoreWrapper::~AndroidLiveTabContextRestoreWrapper() =
    default;

AndroidLiveTabContextRestoreWrapper::TabGroup::TabGroup() = default;
AndroidLiveTabContextRestoreWrapper::TabGroup::~TabGroup() = default;
AndroidLiveTabContextRestoreWrapper::TabGroup::TabGroup(
    AndroidLiveTabContextRestoreWrapper::TabGroup&&) = default;
AndroidLiveTabContextRestoreWrapper::TabGroup&
AndroidLiveTabContextRestoreWrapper::TabGroup::operator=(
    AndroidLiveTabContextRestoreWrapper::TabGroup&&) = default;

void AndroidLiveTabContextRestoreWrapper::SetVisualDataForGroup(
    const tab_groups::TabGroupId& group,
    const tab_groups::TabGroupVisualData& visual_data) {
  // This is recorded in AddRestoredTab calls instead. This information is
  // available to the caller to use as needed via GetTabGroups().
}

sessions::LiveTab* AndroidLiveTabContextRestoreWrapper::AddRestoredTab(
    const sessions::tab_restore::Tab& tab,
    int tab_index,
    bool select,
    sessions::tab_restore::Type original_session_type) {
  auto* live_tab =
      AndroidLiveTabContext::AddRestoredTab(tab, tab_index, select,
                                            original_session_type);
  if (tab.group) {
    TabAndroid* restored_tab = TabAndroid::FromWebContents(
        static_cast<sessions::ContentLiveTab*>(live_tab)->web_contents());
    DCHECK(restored_tab);
    TabGroup& tab_group = tab_groups_[*tab.group];
    tab_group.visual_data = *tab.group_visual_data;
    tab_group.saved_tab_group_id = tab.saved_group_id;
    tab_group.tab_ids.push_back(restored_tab->GetAndroidId());
  }
  return live_tab;
}

const std::map<tab_groups::TabGroupId,
               AndroidLiveTabContextRestoreWrapper::TabGroup>&
AndroidLiveTabContextRestoreWrapper::GetTabGroups() {
  return tab_groups_;
}
