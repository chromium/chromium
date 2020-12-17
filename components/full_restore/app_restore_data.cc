// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/full_restore/app_restore_data.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/full_restore/app_launch_info.h"
#include "components/services/app_service/public/cpp/intent_util.h"

namespace full_restore {

namespace {

const char kEventFlagKey[] = "event_flag";
const char kContainerKey[] = "container";
const char kDispositionKey[] = "disposition";
const char kDisplayIdKey[] = "display_id";
const char kUrlKey[] = "url";
const char kIntentKey[] = "intent";
const char kFilePathsKey[] = "file_paths";
const char kActivationIndexKey[] = "index";
const char kDeskIdKey[] = "desk_id";
const char kRestoredBoundsKey[] = "restored_bounds";
const char kcurrentBoundsKey[] = "current_bounds";
const char kWindowStateTypeKey[] = "window_state_type";

// Converts |rect| to base::Value, e.g. { 0, 100, 200, 300 }.
base::Value ConvertRectToValue(const gfx::Rect& rect) {
  base::Value rect_list(base::Value::Type::LIST);
  rect_list.Append(base::Value(rect.x()));
  rect_list.Append(base::Value(rect.y()));
  rect_list.Append(base::Value(rect.width()));
  rect_list.Append(base::Value(rect.height()));
  return rect_list;
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
  restored_bounds = GetBoundsRectFromDict(*data_dict, kRestoredBoundsKey);
  current_bounds = GetBoundsRectFromDict(*data_dict, kcurrentBoundsKey);
  window_state_type = GetIntValueFromDict(*data_dict, kWindowStateTypeKey);

  if (data_dict->HasKey(kIntentKey)) {
    intent = apps_util::ConvertValueToIntent(
        std::move(*data_dict->FindDictKey(kIntentKey)));
  }
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

  if (restored_bounds.has_value())
    data->restored_bounds = restored_bounds.value();

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

  if (restored_bounds.has_value()) {
    launch_info_dict.SetKey(kRestoredBoundsKey,
                            ConvertRectToValue(restored_bounds.value()));
  }

  if (current_bounds.has_value()) {
    launch_info_dict.SetKey(kcurrentBoundsKey,
                            ConvertRectToValue(current_bounds.value()));
  }

  if (window_state_type.has_value())
    launch_info_dict.SetIntKey(kWindowStateTypeKey, window_state_type.value());

  return launch_info_dict;
}

AppRestoreData::AppRestoreData(std::unique_ptr<AppLaunchInfo> app_launch_info) {
  if (!app_launch_info)
    return;

  if (app_launch_info->event_flag.has_value())
    event_flag = app_launch_info->event_flag.value();

  if (app_launch_info->container.has_value())
    container = app_launch_info->container.value();

  if (app_launch_info->disposition.has_value())
    disposition = app_launch_info->disposition.value();

  if (app_launch_info->display_id.has_value())
    display_id = app_launch_info->display_id.value();

  if (app_launch_info->url.has_value())
    url = std::move(app_launch_info->url.value());

  if (app_launch_info->file_paths.has_value())
    file_paths = std::move(app_launch_info->file_paths.value());

  if (app_launch_info->intent.has_value())
    intent = std::move(app_launch_info->intent.value());
}

}  // namespace full_restore
