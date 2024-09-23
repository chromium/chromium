// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/app_restore/restore_data.h"

#include <utility>

#include "base/logging.h"
#include "base/not_fatal_until.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/app_constants/constants.h"
#include "components/app_restore/app_launch_info.h"

namespace app_restore {

namespace {

// Used to generate unique restore window IDs for desk template launches. These
// IDs will all be negative in order to avoid clashes with full restore (which
// are all positive). The first generated ID will be one lower than the starting
// point and then proceed down. The starting point is a special case value that
// a valid RWID should not use.
int32_t g_desk_template_window_restore_id = -1;

// Key used to record `removing_desk_guid_` when `RestoreData` is converted to
// JSON.
constexpr char kRemovingDeskGuidKey[] = "removing_desk_guid";

}  // namespace

RestoreData::RestoreData() = default;

RestoreData::RestoreData(base::Value restore_data_value) {
  base::Value::Dict* dict = restore_data_value.GetIfDict();
  if (!dict) {
    DVLOG(0) << "Fail to parse full restore data. "
             << "Cannot find the full restore data dict.";
    return;
  }

  if (auto* removing_desk_guid_string =
          dict->FindString(kRemovingDeskGuidKey)) {
    removing_desk_guid_ =
        base::Uuid::ParseLowercase(*removing_desk_guid_string);
  }

  for (auto iter : *dict) {
    // `key` can be an app ID or `kRemovingDeskGuidKey`.
    const std::string& key = iter.first;
    base::Value::Dict* value = iter.second.GetIfDict();

    // Skip the removing desk GUID because we already covered this before the
    // loop.
    if (key == kRemovingDeskGuidKey) {
      continue;
    }

    if (!value) {
      DVLOG(0) << "Fail to parse full restore data. "
               << "Cannot find the app restore data dict.";
      continue;
    }

    for (auto data_iter : *value) {
      const std::string& window_id_string = data_iter.first;

      int window_id = 0;
      if (!base::StringToInt(window_id_string, &window_id)) {
        DVLOG(0) << "Fail to parse full restore data. "
                 << "Cannot find the valid id.";
        continue;
      }

      base::Value::Dict* app_restore_data_dict = data_iter.second.GetIfDict();
      if (!app_restore_data_dict) {
        DVLOG(0) << "Fail to parse app restore data. "
                 << "Cannot find the app restore data dict.";
        continue;
      }

      // If the data is for an app that was on a removing desk, then we can skip
      // adding the data.
      auto app_restore_data =
          std::make_unique<AppRestoreData>(std::move(*app_restore_data_dict));
      if (removing_desk_guid_.is_valid() &&
          app_restore_data->window_info.desk_guid == removing_desk_guid_) {
        continue;
      }

      app_id_to_launch_list_[key][window_id] = std::move(app_restore_data);
    }
  }
}

RestoreData::~RestoreData() = default;

std::unique_ptr<RestoreData> RestoreData::Clone() const {
  std::unique_ptr<RestoreData> restore_data = std::make_unique<RestoreData>();
  for (const auto& it : app_id_to_launch_list_) {
    for (const auto& data_it : it.second) {
      restore_data->app_id_to_launch_list_[it.first][data_it.first] =
          data_it.second->Clone();
    }
  }
  if (removing_desk_guid_.is_valid()) {
    restore_data->removing_desk_guid_ = removing_desk_guid_;
  }
  return restore_data;
}

base::Value RestoreData::ConvertToValue() const {
  base::Value::Dict restore_data_dict;
  for (const auto& [app_id, launch_list] : app_id_to_launch_list_) {
    if (launch_list.empty()) {
      continue;
    }

    base::Value::Dict info_dict;
    for (const auto& [window_id, app_restore_data] : launch_list) {
      info_dict.Set(base::NumberToString(window_id),
                    app_restore_data->ConvertToValue());
    }

    restore_data_dict.Set(app_id, std::move(info_dict));
  }

  if (removing_desk_guid_.is_valid()) {
    restore_data_dict.Set(kRemovingDeskGuidKey,
                          removing_desk_guid_.AsLowercaseString());
  }

  return base::Value(std::move(restore_data_dict));
}

bool RestoreData::HasAppTypeBrowser() const {
  auto it = app_id_to_launch_list_.find(app_constants::kChromeAppId);
  if (it == app_id_to_launch_list_.end())
    return false;

  return base::ranges::any_of(
      it->second,
      [](const std::pair<const int, std::unique_ptr<AppRestoreData>>& data) {
        return data.second->browser_extra_info.app_type_browser.value_or(false);
      });
}

bool RestoreData::HasBrowser() const {
  auto it = app_id_to_launch_list_.find(app_constants::kChromeAppId);
  if (it == app_id_to_launch_list_.end())
    return false;

  return base::ranges::any_of(
      it->second,
      [](const std::pair<const int, std::unique_ptr<AppRestoreData>>& data) {
        return !data.second->browser_extra_info.app_type_browser.value_or(
            false);
      });
}

bool RestoreData::HasAppRestoreData(const std::string& app_id,
                                    int32_t window_id) {
  return GetAppRestoreData(app_id, window_id) != nullptr;
}

void RestoreData::AddAppLaunchInfo(
    std::unique_ptr<AppLaunchInfo> app_launch_info) {
  if (!app_launch_info || !app_launch_info->window_id.has_value())
    return;

  const std::string app_id = app_launch_info->app_id;
  const int32_t window_id = app_launch_info->window_id.value();
  app_id_to_launch_list_[app_id][window_id] =
      std::make_unique<AppRestoreData>(std::move(app_launch_info));
}

void RestoreData::ModifyWindowId(const std::string& app_id,
                                 int32_t old_window_id,
                                 int32_t new_window_id) {
  auto it = app_id_to_launch_list_.find(app_id);
  if (it == app_id_to_launch_list_.end())
    return;

  auto data_it = it->second.find(old_window_id);
  if (data_it == it->second.end())
    return;

  it->second[new_window_id] = std::move(data_it->second);
  it->second.erase(data_it);
}

void RestoreData::ModifyWindowInfo(const std::string& app_id,
                                   int32_t window_id,
                                   const WindowInfo& window_info) {
  auto* app_restore_data = GetAppRestoreDataMutable(app_id, window_id);
  if (app_restore_data)
    app_restore_data->ModifyWindowInfo(window_info);
}

void RestoreData::ModifyThemeColor(const std::string& app_id,
                                   int32_t window_id,
                                   uint32_t primary_color,
                                   uint32_t status_bar_color) {
  auto* app_restore_data = GetAppRestoreDataMutable(app_id, window_id);
  if (app_restore_data)
    app_restore_data->ModifyThemeColor(primary_color, status_bar_color);
}

void RestoreData::SetNextRestoreWindowIdForChromeApp(
    const std::string& app_id) {
  auto it = app_id_to_launch_list_.find(app_id);
  if (it == app_id_to_launch_list_.end())
    return;

  chrome_app_id_to_current_window_id_[app_id] = it->second.begin()->first;

  if (it->second.size() == 1)
    return;

  // When a chrome app has multiple windows, all windows will be sent to the
  // background.
  for (auto& [window_id, app_restore_data] : it->second) {
    app_restore_data->window_info.activation_index = INT32_MAX;
  }
}

void RestoreData::RemoveAppRestoreData(const std::string& app_id,
                                       int window_id) {
  if (app_id_to_launch_list_.find(app_id) == app_id_to_launch_list_.end())
    return;

  app_id_to_launch_list_[app_id].erase(window_id);
  if (app_id_to_launch_list_[app_id].empty())
    app_id_to_launch_list_.erase(app_id);
}

void RestoreData::SendWindowToBackground(const std::string& app_id,
                                         int window_id) {
  if (auto* app_restore_data = GetAppRestoreDataMutable(app_id, window_id)) {
    app_restore_data->window_info.activation_index = INT32_MAX;
  }
}

void RestoreData::RemoveApp(const std::string& app_id) {
  app_id_to_launch_list_.erase(app_id);
  chrome_app_id_to_current_window_id_.erase(app_id);
}

std::unique_ptr<AppLaunchInfo> RestoreData::GetAppLaunchInfo(
    const std::string& app_id,
    int window_id) {
  auto* app_restore_data = GetAppRestoreData(app_id, window_id);
  return app_restore_data
             ? app_restore_data->GetAppLaunchInfo(app_id, window_id)
             : nullptr;
}

std::unique_ptr<WindowInfo> RestoreData::GetWindowInfo(
    const std::string& app_id,
    int window_id) {
  auto* app_restore_data = GetAppRestoreData(app_id, window_id);
  return app_restore_data ? app_restore_data->GetWindowInfo() : nullptr;
}

int32_t RestoreData::FetchRestoreWindowId(const std::string& app_id) {
  auto it = app_id_to_launch_list_.find(app_id);
  if (it == app_id_to_launch_list_.end())
    return 0;

  if (chrome_app_id_to_current_window_id_.find(app_id) ==
      chrome_app_id_to_current_window_id_.end()) {
    return 0;
  }

  int window_id = chrome_app_id_to_current_window_id_[app_id];

  // Move to the next window_id.
  auto data_it = it->second.find(window_id);
  CHECK(data_it != it->second.end(), base::NotFatalUntil::M130);
  ++data_it;
  if (data_it == it->second.end())
    chrome_app_id_to_current_window_id_.erase(app_id);
  else
    chrome_app_id_to_current_window_id_[app_id] = data_it->first;

  return window_id;
}

const AppRestoreData* RestoreData::GetAppRestoreData(const std::string& app_id,
                                                     int window_id) const {
  auto it = app_id_to_launch_list_.find(app_id);
  if (it == app_id_to_launch_list_.end())
    return nullptr;

  auto data_it = it->second.find(window_id);
  if (data_it == it->second.end())
    return nullptr;

  return data_it->second.get();
}

void RestoreData::SetDeskUuid(const base::Uuid& desk_uuid) {
  for (auto& [app_id, launch_list] : app_id_to_launch_list_) {
    for (auto& [window_id, app_restore_data] : launch_list) {
      app_restore_data->window_info.desk_guid = desk_uuid;
    }
  }
}

base::flat_map<int32_t, int32_t>
RestoreData::MakeWindowIdsUniqueForDeskTemplate() {
  if (has_unique_window_ids_for_desk_template_) {
    return {};
  }

  base::flat_map<int32_t, int32_t> mapping;
  for (auto& [app_id, launch_list] : app_id_to_launch_list_) {
    // We don't want to do in-place updates of the launch list since it
    // complicates traversal. We'll therefore build a new LaunchList and pilfer
    // the old one for AppRestoreData.
    LaunchList new_launch_list;
    for (auto& [window_id, app_restore_data] : launch_list) {
      int32_t new_rwid = --g_desk_template_window_restore_id;
      new_launch_list[new_rwid] = std::move(app_restore_data);
      mapping[new_rwid] = window_id;
    }
    launch_list = std::move(new_launch_list);
  }

  has_unique_window_ids_for_desk_template_ = true;
  return mapping;
}

void RestoreData::UpdateBrowserAppIdToLacros() {
  auto app_launch_list_iter =
      app_id_to_launch_list_.find(app_constants::kChromeAppId);
  if (app_launch_list_iter == app_id_to_launch_list_.end()) {
    return;
  }
  app_id_to_launch_list_[app_constants::kLacrosAppId] =
      std::move(app_launch_list_iter->second);
  RemoveApp(app_constants::kChromeAppId);
}

std::string RestoreData::ToString() const {
  return ConvertToValue().DebugString();
}

AppRestoreData* RestoreData::GetAppRestoreDataMutable(const std::string& app_id,
                                                      int window_id) {
  return const_cast<AppRestoreData*>(GetAppRestoreData(app_id, window_id));
}

}  // namespace app_restore
