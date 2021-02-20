// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/full_restore/app_restore_data.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/full_restore/app_launch_info.h"
#include "components/full_restore/window_info.h"
#include "components/services/app_service/public/cpp/intent_util.h"

namespace full_restore {

namespace {

constexpr char kEventFlagKey[] = "event_flag";
constexpr char kContainerKey[] = "container";
constexpr char kDispositionKey[] = "disposition";
constexpr char kDisplayIdKey[] = "display_id";
constexpr char kUrlKey[] = "url";
constexpr char kIntentKey[] = "intent";
constexpr char kFilePathsKey[] = "file_paths";
constexpr char kActivationIndexKey[] = "index";
constexpr char kDeskIdKey[] = "desk_id";
constexpr char kVisibleOnAllWorkspacesKey[] = "all_desk";
constexpr char kRestoreBoundsKey[] = "restore_bounds";
constexpr char kCurrentBoundsKey[] = "current_bounds";
constexpr char kWindowStateTypeKey[] = "window_state_type";

// Converts |rect| to base::Value, e.g. { 0, 100, 200, 300 }.
base::Value ConvertRectToValue(const gfx::Rect& rect) {
  base::Value rect_list(base::Value::Type::LIST);
  rect_list.Append(base::Value(rect.x()));
  rect_list.Append(base::Value(rect.y()));
  rect_list.Append(base::Value(rect.width()));
  rect_list.Append(base::Value(rect.height()));
  return rect_list;
}

// Gets bool value from base::DictionaryValue, e.g. { "key": true } returns
// true.
base::Optional<bool> GetBoolValueFromDict(const base::DictionaryValue& dict,
                                          const std::string& key_name) {
  return dict.HasKey(key_name) ? dict.FindBoolKey(key_name) : base::nullopt;
}

// Gets int value from base::DictionaryValue, e.g. { "key": 100 } returns 100.
base::Optional<int32_t> GetIntValueFromDict(const base::DictionaryValue& dict,
                                            const std::string& key_name) {
  return dict.HasKey(key_name) ? dict.FindIntKey(key_name) : base::nullopt;
}

// Gets display id from base::DictionaryValue, e.g. { "display_id": "22000000" }
// returns 22000000.
base::Optional<int64_t> GetDisplayIdFromDict(
    const base::DictionaryValue& dict) {
  if (!dict.HasKey(kDisplayIdKey))
    return base::nullopt;

  const std::string* display_id_str = dict.FindStringKey(kDisplayIdKey);
  int64_t display_id_value;
  if (display_id_str &&
      base::StringToInt64(*display_id_str, &display_id_value)) {
    return display_id_value;
  }

  return base::nullopt;
}

// Gets std::vector<base::FilePath> from base::DictionaryValue, e.g.
// {"file_paths": { "aa.cc", "bb.h", ... }} returns
// std::vector<base::FilePath>{"aa.cc", "bb.h", ...}.
base::Optional<std::vector<base::FilePath>> GetFilePathsFromDict(
    const base::DictionaryValue& dict) {
  if (!dict.HasKey(kFilePathsKey))
    return base::nullopt;

  const base::Value* file_paths_value = dict.FindListKey(kFilePathsKey);
  if (!file_paths_value || !file_paths_value->is_list() ||
      file_paths_value->GetList().empty())
    return base::nullopt;

  std::vector<base::FilePath> file_paths;
  for (const auto& item : file_paths_value->GetList()) {
    if (item.GetString().empty())
      continue;
    file_paths.push_back(base::FilePath(item.GetString()));
  }

  return file_paths;
}

// Gets gfx::Rect from base::Value, e.g. { 0, 100, 200, 300 } returns
// gfx::Rect(0, 100, 200, 300).
base::Optional<gfx::Rect> GetBoundsRectFromDict(
    const base::DictionaryValue& dict,
    const std::string& key_name) {
  if (!dict.HasKey(key_name))
    return base::nullopt;

  const base::Value* rect_value = dict.FindListKey(key_name);
  if (!rect_value || !rect_value->is_list() || rect_value->GetList().empty())
    return base::nullopt;

  std::vector<int> rect;
  for (const auto& item : rect_value->GetList())
    rect.push_back(item.GetInt());

  if (rect.size() != 4)
    return base::nullopt;

  return gfx::Rect(rect[0], rect[1], rect[2], rect[3]);
}

// Gets WindowStateType from base::DictionaryValue, e.g. { "window_state_type":
// 2 } returns WindowStateType::kMinimized.
base::Optional<chromeos::WindowStateType> GetWindowStateTypeFromDict(
    const base::DictionaryValue& dict) {
  return dict.HasKey(kWindowStateTypeKey)
             ? base::make_optional(static_cast<chromeos::WindowStateType>(
                   dict.FindIntKey(kWindowStateTypeKey).value()))
             : base::nullopt;
}

}  // namespace

AppRestoreData::AppRestoreData() = default;

AppRestoreData::AppRestoreData(base::Value&& value) {
  base::DictionaryValue* data_dict = nullptr;
  if (!value.is_dict() || !value.GetAsDictionary(&data_dict) || !data_dict) {
    DVLOG(0) << "Fail to parse app restore data. "
             << "Cannot find the app restore data dict.";
    return;
  }

  event_flag = GetIntValueFromDict(*data_dict, kEventFlagKey);
  container = GetIntValueFromDict(*data_dict, kContainerKey);
  disposition = GetIntValueFromDict(*data_dict, kDispositionKey);
  display_id = GetDisplayIdFromDict(*data_dict);
  url = apps_util::GetGurlValueFromDict(*data_dict, kUrlKey);
  file_paths = GetFilePathsFromDict(*data_dict);
  activation_index = GetIntValueFromDict(*data_dict, kActivationIndexKey);
  desk_id = GetIntValueFromDict(*data_dict, kDeskIdKey);
  visible_on_all_workspaces =
      GetBoolValueFromDict(*data_dict, kVisibleOnAllWorkspacesKey);
  restore_bounds = GetBoundsRectFromDict(*data_dict, kRestoreBoundsKey);
  current_bounds = GetBoundsRectFromDict(*data_dict, kCurrentBoundsKey);
  window_state_type = GetWindowStateTypeFromDict(*data_dict);

  if (data_dict->HasKey(kIntentKey)) {
    intent = apps_util::ConvertValueToIntent(
        std::move(*data_dict->FindDictKey(kIntentKey)));
  }
}

AppRestoreData::AppRestoreData(std::unique_ptr<AppLaunchInfo> app_launch_info) {
  if (!app_launch_info)
    return;

  event_flag = std::move(app_launch_info->event_flag);
  container = std::move(app_launch_info->container);
  disposition = std::move(app_launch_info->disposition);
  display_id = std::move(app_launch_info->display_id);
  url = std::move(app_launch_info->url);
  file_paths = std::move(app_launch_info->file_paths);
  intent = std::move(app_launch_info->intent);
}

AppRestoreData::~AppRestoreData() = default;

std::unique_ptr<AppRestoreData> AppRestoreData::Clone() const {
  std::unique_ptr<AppRestoreData> data = std::make_unique<AppRestoreData>();

  if (event_flag.has_value())
    data->event_flag = event_flag.value();

  if (container.has_value())
    data->container = container.value();

  if (disposition.has_value())
    data->disposition = disposition.value();

  if (display_id.has_value())
    data->display_id = display_id.value();

  if (url.has_value())
    data->url = url.value();

  if (intent.has_value() && intent.value())
    data->intent = intent.value()->Clone();

  if (file_paths.has_value())
    data->file_paths = file_paths.value();

  if (activation_index.has_value())
    data->activation_index = activation_index.value();

  if (desk_id.has_value())
    data->desk_id = desk_id.value();

  if (visible_on_all_workspaces.has_value())
    data->visible_on_all_workspaces = visible_on_all_workspaces.value();

  if (restore_bounds.has_value())
    data->restore_bounds = restore_bounds.value();

  if (current_bounds.has_value())
    data->current_bounds = current_bounds.value();

  if (window_state_type.has_value())
    data->window_state_type = window_state_type.value();

  return data;
}

base::Value AppRestoreData::ConvertToValue() const {
  base::Value launch_info_dict(base::Value::Type::DICTIONARY);

  if (event_flag.has_value())
    launch_info_dict.SetIntKey(kEventFlagKey, event_flag.value());

  if (container.has_value())
    launch_info_dict.SetIntKey(kContainerKey, container.value());

  if (disposition.has_value())
    launch_info_dict.SetIntKey(kDispositionKey, disposition.value());

  if (display_id.has_value()) {
    launch_info_dict.SetStringKey(kDisplayIdKey,
                                  base::NumberToString(display_id.value()));
  }

  if (url.has_value())
    launch_info_dict.SetStringKey(kUrlKey, url.value().spec());

  if (intent.has_value() && intent.value()) {
    launch_info_dict.SetKey(kIntentKey,
                            apps_util::ConvertIntentToValue(intent.value()));
  }

  if (file_paths.has_value() && !file_paths.value().empty()) {
    base::Value file_paths_list(base::Value::Type::LIST);
    for (auto& file_path : file_paths.value())
      file_paths_list.Append(base::Value(file_path.value()));
    launch_info_dict.SetKey(kFilePathsKey, std::move(file_paths_list));
  }

  if (activation_index.has_value())
    launch_info_dict.SetIntKey(kActivationIndexKey, activation_index.value());

  if (desk_id.has_value())
    launch_info_dict.SetIntKey(kDeskIdKey, desk_id.value());

  if (visible_on_all_workspaces.has_value()) {
    launch_info_dict.SetBoolKey(kVisibleOnAllWorkspacesKey,
                                visible_on_all_workspaces.value());
  }

  if (restore_bounds.has_value()) {
    launch_info_dict.SetKey(kRestoreBoundsKey,
                            ConvertRectToValue(restore_bounds.value()));
  }

  if (current_bounds.has_value()) {
    launch_info_dict.SetKey(kCurrentBoundsKey,
                            ConvertRectToValue(current_bounds.value()));
  }

  if (window_state_type.has_value()) {
    launch_info_dict.SetIntKey(kWindowStateTypeKey,
                               static_cast<int>(window_state_type.value()));
  }

  return launch_info_dict;
}

void AppRestoreData::ModifyWindowInfo(const WindowInfo& window_info) {
  if (window_info.activation_index.has_value())
    activation_index = window_info.activation_index.value();

  if (window_info.desk_id.has_value())
    desk_id = window_info.desk_id.value();

  if (window_info.visible_on_all_workspaces.has_value())
    visible_on_all_workspaces = window_info.visible_on_all_workspaces.value();

  if (window_info.restore_bounds.has_value())
    restore_bounds = std::move(window_info.restore_bounds.value());

  if (window_info.current_bounds.has_value())
    current_bounds = window_info.current_bounds.value();

  if (window_info.window_state_type.has_value())
    window_state_type = window_info.window_state_type.value();

  if (window_info.display_id.has_value())
    display_id = window_info.display_id.value();
}

std::unique_ptr<WindowInfo> AppRestoreData::GetWindowInfo() const {
  auto window_info = std::make_unique<WindowInfo>();

  if (activation_index.has_value())
    window_info->activation_index = activation_index;

  if (desk_id.has_value())
    window_info->desk_id = desk_id.value();

  if (visible_on_all_workspaces.has_value())
    window_info->visible_on_all_workspaces = visible_on_all_workspaces.value();

  if (restore_bounds.has_value())
    window_info->restore_bounds = restore_bounds.value();

  if (current_bounds.has_value())
    window_info->current_bounds = current_bounds.value();

  if (window_state_type.has_value())
    window_info->window_state_type = window_state_type.value();

  // Display id is set as the app launch parameter, so we don't need to return
  // the display id to restore the display id.
  return window_info;
}

apps::mojom::WindowInfoPtr AppRestoreData::GetAppWindowInfo() const {
  apps::mojom::WindowInfoPtr window_info = apps::mojom::WindowInfo::New();

  if (display_id.has_value())
    window_info->display_id = display_id.value();

  if (current_bounds.has_value()) {
    window_info->bounds = apps::mojom::Rect::New();
    window_info->bounds->x = current_bounds.value().x();
    window_info->bounds->y = current_bounds.value().y();
    window_info->bounds->width = current_bounds.value().width();
    window_info->bounds->height = current_bounds.value().height();
  }

  if (window_state_type.has_value())
    window_info->state = static_cast<int32_t>(window_state_type.value());

  return window_info;
}

}  // namespace full_restore
