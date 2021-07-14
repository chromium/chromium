// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/full_restore/restore_data.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/full_restore/app_launch_info.h"
#include "components/full_restore/window_info.h"
#include "extensions/common/constants.h"

namespace full_restore {

RestoreData::RestoreData() = default;

RestoreData::RestoreData(std::unique_ptr<base::Value> restore_data_value) {
  base::DictionaryValue* restore_data_dict = nullptr;
  if (!restore_data_value || !restore_data_value->is_dict() ||
      !restore_data_value->GetAsDictionary(&restore_data_dict) ||
      !restore_data_dict) {
    DVLOG(0) << "Fail to parse full restore data. "
             << "Cannot find the full restore data dict.";
    return;
  }

  for (base::DictionaryValue::Iterator iter(*restore_data_dict);
       !iter.IsAtEnd(); iter.Advance()) {
    const std::string& app_id = iter.key();
    base::Value* value = restore_data_dict->FindDictKey(app_id);
    base::DictionaryValue* data_dict = nullptr;
    if (!value || !value->is_dict() || !value->GetAsDictionary(&data_dict) ||
        !data_dict) {
      DVLOG(0) << "Fail to parse full restore data. "
               << "Cannot find the app restore data dict.";
      continue;
    }

    for (base::DictionaryValue::Iterator data_iter(*data_dict);
         !data_iter.IsAtEnd(); data_iter.Advance()) {
      int window_id = 0;
      if (!base::StringToInt(data_iter.key(), &window_id)) {
        DVLOG(0) << "Fail to parse full restore data. "
                 << "Cannot find the valid id.";
        continue;
      }
      app_id_to_launch_list_[app_id][window_id] =
          std::make_unique<AppRestoreData>(
              std::move(*data_dict->FindDictKey(data_iter.key())));
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
  return restore_data;
}

base::Value RestoreData::ConvertToValue() const {
  base::Value restore_data_dict(base::Value::Type::DICTIONARY);
  for (const auto& it : app_id_to_launch_list_) {
    if (it.second.empty())
      continue;

    base::Value info_dict(base::Value::Type::DICTIONARY);
    for (const auto& data : it.second) {
      info_dict.SetKey(base::NumberToString(data.first),
                       data.second->ConvertToValue());
    }

    restore_data_dict.SetKey(it.first, std::move(info_dict));
  }
  return restore_data_dict;
}

bool RestoreData::HasAppTypeBrowser() {
  auto it = app_id_to_launch_list_.find(extension_misc::kChromeAppId);
  if (it == app_id_to_launch_list_.end())
    return false;

  for (const auto& data : it->second) {
    if (data.second->app_type_browser.has_value() &&
        data.second->app_type_browser.value()) {
      return true;
    }
  }
  return false;
}

bool RestoreData::HasBrowser() {
  auto it = app_id_to_launch_list_.find(extension_misc::kChromeAppId);
  if (it == app_id_to_launch_list_.end())
    return false;

  for (const auto& data : it->second) {
    if (!data.second->app_type_browser.has_value() ||
        !data.second->app_type_browser.value()) {
      return true;
    }
  }
  return false;
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
  for (auto& data_it : it->second)
    data_it.second->activation_index = INT32_MIN;
}

void RestoreData::RemoveAppRestoreData(const std::string& app_id,
                                       int window_id) {
  if (app_id_to_launch_list_.find(app_id) == app_id_to_launch_list_.end())
    return;

  app_id_to_launch_list_[app_id].erase(window_id);
  if (app_id_to_launch_list_[app_id].empty())
    app_id_to_launch_list_.erase(app_id);
}

void RestoreData::RemoveWindowInfo(const std::string& app_id, int window_id) {
  auto* app_restore_data = GetAppRestoreDataMutable(app_id, window_id);
  if (app_restore_data)
    app_restore_data->ClearWindowInfo();
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
  DCHECK(data_it != it->second.end());
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

AppRestoreData* RestoreData::GetAppRestoreDataMutable(const std::string& app_id,
                                                      int window_id) {
  return const_cast<AppRestoreData*>(GetAppRestoreData(app_id, window_id));
}

}  // namespace full_restore
