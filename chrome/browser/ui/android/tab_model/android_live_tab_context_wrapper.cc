// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_model/android_live_tab_context_wrapper.h"

#include "base/numerics/safe_conversions.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "components/sessions/content/content_live_tab.h"

AndroidLiveTabContextCloseWrapper::AndroidLiveTabContextCloseWrapper(
    TabModel* tab_model,
    std::vector<TabAndroid*>&& closed_tabs,
    std::map<int, tab_groups::TabGroupId>&& tab_id_to_tab_group,
    std::map<tab_groups::TabGroupId, tab_groups::TabGroupVisualData>&&
        tab_group_visual_data,
    std::vector<historical_tab_saver::WebContentsStateByteBuffer>&&
        web_contents_state)
    : AndroidLiveTabContext(tab_model),
      closed_tabs_(closed_tabs),
      tab_id_to_tab_group_(tab_id_to_tab_group),
      tab_group_visual_data_(tab_group_visual_data),
      web_contents_state_(web_contents_state) {}

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

absl::optional<tab_groups::TabGroupId>
AndroidLiveTabContextCloseWrapper::GetTabGroupForTab(int relative_index) const {
  auto it = tab_id_to_tab_group_.find(GetTabAt(relative_index)->GetAndroidId());
  return it != tab_id_to_tab_group_.end()
             ? it->second
             : absl::optional<tab_groups::TabGroupId>();
}

const tab_groups::TabGroupVisualData*
AndroidLiveTabContextCloseWrapper::GetVisualDataForGroup(
    const tab_groups::TabGroupId& group_id) const {
  auto it = tab_group_visual_data_.find(group_id);
  return it == tab_group_visual_data_.end() ? nullptr : &it->second;
}

TabAndroid* AndroidLiveTabContextCloseWrapper::GetTabAt(
    int relative_index) const {
  DCHECK_LT(base::checked_cast<size_t>(relative_index), closed_tabs_.size());
  auto* tab_android = closed_tabs_[relative_index];
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
    const std::vector<sessions::SerializedNavigationEntry>& navigations,
    int tab_index,
    int selected_navigation,
    const std::string& extension_app_id,
    absl::optional<tab_groups::TabGroupId> group,
    const tab_groups::TabGroupVisualData& group_visual_data,
    bool select,
    bool pin,
    const sessions::PlatformSpecificTabData* storage_namespace,
    const sessions::SerializedUserAgentOverride& user_agent_override,
    const std::map<std::string, std::string>& extra_data,
    const SessionID* tab_id) {
  auto* live_tab = AndroidLiveTabContext::AddRestoredTab(
      navigations, tab_index, selected_navigation, extension_app_id, group,
      group_visual_data, select, pin, storage_namespace, user_agent_override,
      extra_data, tab_id);
  if (group) {
    TabAndroid* tab = TabAndroid::FromWebContents(
        static_cast<sessions::ContentLiveTab*>(live_tab)->web_contents());
    DCHECK(tab);
    tab_groups_[*group].visual_data = group_visual_data;
    tab_groups_[*group].tab_ids.push_back(tab->GetAndroidId());
  }
  return live_tab;
}

const std::map<tab_groups::TabGroupId,
               AndroidLiveTabContextRestoreWrapper::TabGroup>&
AndroidLiveTabContextRestoreWrapper::GetTabGroups() {
  return tab_groups_;
}
