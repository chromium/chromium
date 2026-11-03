// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_model/android_live_tab_context_wrapper.h"

#include <utility>

#include "base/check_op.h"
#include "base/containers/map_util.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/glic/glic_tab_restore_helper.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "components/sessions/content/content_live_tab.h"
#include "components/sessions/core/tab_restore_types.h"
#include "components/tab_groups/tab_group_id.h"

namespace {
std::optional<int> GetOldTabIdFromExtraData(
    const std::map<std::string, std::string>& extra_data) {
  const std::string* value = base::FindOrNull(extra_data, kUndoOldTabIdKey);
  if (value) {
    int old_tab_id;
    if (base::StringToInt(*value, &old_tab_id)) {
      return old_tab_id;
    }
  }
  return std::nullopt;
}
}  // namespace

AndroidLiveTabContextCloseWrapper::AndroidLiveTabContextCloseWrapper(
    TabModel* tab_model,
    std::vector<raw_ptr<TabAndroid>> closed_tabs,
    base::flat_map<int, tab_groups::TabGroupId> tab_id_to_tab_group,
    base::flat_map<tab_groups::TabGroupId, tab_groups::TabGroupVisualData>
        tab_group_visual_data,
    base::flat_map<tab_groups::TabGroupId, std::optional<base::Uuid>>
        saved_tab_group_ids,
    std::vector<WebContentsStateByteBuffer> web_contents_state)
    : AndroidLiveTabContext(tab_model),
      closed_tabs_(std::move(closed_tabs)),
      tab_id_to_tab_group_(std::move(tab_id_to_tab_group)),
      tab_group_visual_data_(std::move(tab_group_visual_data)),
      saved_tab_group_ids_(std::move(saved_tab_group_ids)),
      web_contents_state_(std::move(web_contents_state)) {
  CHECK_EQ(closed_tabs_.size(), web_contents_state_.size());
}

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
  CHECK_LT(base::checked_cast<size_t>(relative_index),
           web_contents_state_.size(), base::NotFatalUntil::M162);
  scoped_web_contents_ = historical_tab_saver::ScopedWebContents::CreateForTab(
      GetTabAt(relative_index), &web_contents_state_[relative_index]);
  CHECK(scoped_web_contents_->web_contents(), base::NotFatalUntil::M162);
  return sessions::ContentLiveTab::GetOrCreateForWebContents(
      scoped_web_contents_->web_contents());
}

std::optional<tab_groups::TabGroupId>
AndroidLiveTabContextCloseWrapper::GetTabGroupForTab(int relative_index) const {
  auto it = tab_id_to_tab_group_.find(GetTabAt(relative_index)->GetAndroidId());
  if (it != tab_id_to_tab_group_.end()) {
    return it->second;
  }
  return std::nullopt;
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

const std::optional<tab_groups::TabGroupId>
AndroidLiveTabContextCloseWrapper::GetGroupIdForSavedGroup(
    const base::Uuid& saved) const {
  for (auto const& pair : saved_tab_group_ids_) {
    if (pair.second && pair.second.value() == saved) {
      return pair.first;
    }
  }
  return std::nullopt;
}

bool AndroidLiveTabContextCloseWrapper::IsTabPinned(int relative_index) const {
  if (!base::FeatureList::IsEnabled(
          chrome::android::kUndoableTabClosureRework)) {
    // Legacy behavior: `AndroidLiveTabContextCloseWrapper` previously did not
    // override this method, so `TabRestoreServiceHelper` called
    // `AndroidLiveTabContext::IsTabPinned(relative_index)`, which looks up
    // `tab_model_->GetTabAt(relative_index)` in the live `TabModel` instead of
    // `closed_tabs_[relative_index]`.
    return AndroidLiveTabContext::IsTabPinned(relative_index);
  }
  return GetTabAt(relative_index)->IsPinned();
}

std::map<std::string, std::string>
AndroidLiveTabContextCloseWrapper::GetExtraDataForTab(
    int relative_index) const {
  if (!base::FeatureList::IsEnabled(
          chrome::android::kUndoableTabClosureRework)) {
    // Legacy behavior: `AndroidLiveTabContextCloseWrapper` previously did not
    // override this method, so `TabRestoreServiceHelper` called
    // `AndroidLiveTabContext::GetExtraDataForTab(relative_index)`, which looks
    // up `tab_model_->GetTabAt(relative_index)` in the live `TabModel` instead
    // of `closed_tabs_[relative_index]`.
    return AndroidLiveTabContext::GetExtraDataForTab(relative_index);
  }
  std::map<std::string, std::string> extra_data;
  TabAndroid* tab_android = GetTabAt(relative_index);
  glic::PopulateGlicExtraData(tab_android, &extra_data);
  extra_data[kUndoOldTabIdKey] =
      base::NumberToString(tab_android->GetAndroidId());
  return extra_data;
}

TabAndroid* AndroidLiveTabContextCloseWrapper::GetTabAt(
    int relative_index) const {
  CHECK_LT(base::checked_cast<size_t>(relative_index), closed_tabs_.size(),
           base::NotFatalUntil::M162);
  TabAndroid* tab_android = closed_tabs_[relative_index];
  CHECK(tab_android, base::NotFatalUntil::M162);
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
    bool is_restoring_group_or_window,
    sessions::tab_restore::Type original_session_type) {
  auto* live_tab = AndroidLiveTabContext::AddRestoredTab(
      tab, tab_index, select, is_restoring_group_or_window,
      original_session_type);
  if (tab.group) {
    TabAndroid* restored_tab = TabAndroid::FromWebContents(
        &static_cast<sessions::ContentLiveTab*>(live_tab)->GetWebContents());
    CHECK(restored_tab, base::NotFatalUntil::M162);
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

AndroidLiveTabContextUndoRestoreWrapper::
    AndroidLiveTabContextUndoRestoreWrapper(TabModel* tab_model,
                                            std::vector<ClosedTab> closed_tabs)
    : AndroidLiveTabContext(tab_model),
      closed_tabs_(std::move(closed_tabs)),
      restored_tab_ids_(closed_tabs_.size(), kNotRestored) {
  std::vector<std::pair<int, size_t>> map_entries;
  map_entries.reserve(closed_tabs_.size());
  for (size_t i = 0; i < closed_tabs_.size(); ++i) {
    map_entries.emplace_back(closed_tabs_[i].old_tab_id, i);
  }
  placement_index_by_old_tab_id_ =
      base::flat_map<int, size_t>(std::move(map_entries));
}

AndroidLiveTabContextUndoRestoreWrapper::
    ~AndroidLiveTabContextUndoRestoreWrapper() = default;

// static
std::optional<size_t>
AndroidLiveTabContextUndoRestoreWrapper::ResolvePlacementIndex(
    const std::map<std::string, std::string>& extra_data,
    const base::flat_map<int, size_t>& placement_index_by_old_tab_id,
    const std::vector<int>& restored_tab_ids,
    sessions::tab_restore::Type original_session_type) {
  std::optional<int> old_tab_id = GetOldTabIdFromExtraData(extra_data);
  if (old_tab_id.has_value()) {
    const size_t* placement_index =
        base::FindOrNull(placement_index_by_old_tab_id, old_tab_id.value());
    if (placement_index && restored_tab_ids[*placement_index] == kNotRestored) {
      return *placement_index;
    }
  }
  if (!old_tab_id.has_value() &&
      original_session_type == sessions::tab_restore::Type::TAB &&
      restored_tab_ids.size() == 1 && restored_tab_ids[0] == kNotRestored) {
    return 0;
  }
  return std::nullopt;
}

sessions::LiveTab* AndroidLiveTabContextUndoRestoreWrapper::AddRestoredTab(
    const sessions::tab_restore::Tab& tab,
    int tab_index,
    bool select,
    bool is_restoring_group_or_window,
    sessions::tab_restore::Type original_session_type) {
  Profile* profile = tab_model()->GetProfile();

  std::unique_ptr<content::WebContents> new_web_contents =
      CreateRestoredWebContents(profile, tab);
  content::WebContents* web_contents_ptr = new_web_contents.get();

  std::optional<size_t> placement_index =
      ResolvePlacementIndex(tab.extra_data, placement_index_by_old_tab_id_,
                            restored_tab_ids_, original_session_type);

  // `tab_index` (TabRestoreService passes the tab count) and
  // `tab.tabstrip_index` (the position within the saved entry, shifted by
  // save-time filtering and history deletion) are ignored. The closed tab's
  // model index is used instead.
  int insertion_index;
  bool restore_pinned;
  if (placement_index.has_value()) {
    const ClosedTab& closed_tab = closed_tabs_[placement_index.value()];
    insertion_index = closed_tab.model_index;
    restore_pinned = closed_tab.pinned;
  } else {
    // Unmatched tabs are appended unpinned and not recorded.
    insertion_index = tab_model()->GetTabCount();
    restore_pinned = false;
  }

  if (!tab_model()->CreateTab(
          nullptr, std::move(new_web_contents), insertion_index,
          TabModel::TabLaunchType::FROM_UNDO_CLOSURE, restore_pinned)) {
    return nullptr;
  }

  sessions::LiveTab* live_tab =
      sessions::ContentLiveTab::GetOrCreateForWebContents(web_contents_ptr);

  if (placement_index.has_value()) {
    TabAndroid* restored_tab = TabAndroid::FromWebContents(web_contents_ptr);
    if (restored_tab) {
      // Record the new tab id to map it back to its placement index.
      restored_tab_ids_[placement_index.value()] = restored_tab->GetAndroidId();
    }
  }

  return live_tab;
}

const std::vector<int>&
AndroidLiveTabContextUndoRestoreWrapper::GetRestoredIdsByPlacementIndex()
    const {
  return restored_tab_ids_;
}
