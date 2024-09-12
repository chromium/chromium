// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/app_restore/app_restore_data.h"

#include <string_view>
#include <utility>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "components/app_restore/app_launch_info.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "ui/base/mojom/window_show_state.mojom.h"

namespace app_restore {

namespace {

constexpr char kEventFlagKey[] = "event_flag";
constexpr char kContainerKey[] = "container";
constexpr char kDispositionKey[] = "disposition";
constexpr char kOverrideUrlKey[] = "override_url";
constexpr char kDisplayIdKey[] = "display_id";
constexpr char kHandlerIdKey[] = "handler_id";
constexpr char kUrlsKey[] = "urls";
constexpr char kActiveTabIndexKey[] = "active_tab_index";
constexpr char kIntentKey[] = "intent";
constexpr char kFilePathsKey[] = "file_paths";
constexpr char kAppTypeBrowserKey[] = "is_app";
constexpr char kAppNameKey[] = "app_name";
constexpr char kActivationIndexKey[] = "index";
constexpr char kFirstNonPinnedTabIndexKey[] = "first_non_pinned_tab_index";
constexpr char kDeskIdKey[] = "desk_id";
constexpr char kDeskUuidKey[] = "desk_guid";
constexpr char kCurrentBoundsKey[] = "current_bounds";
constexpr char kWindowStateTypeKey[] = "window_state_type";
constexpr char kPreMinimizedShowStateTypeKey[] = "pre_min_state";
constexpr char kSnapPercentageKey[] = "snap_percent";
constexpr char kMinimumSizeKey[] = "min_size";
constexpr char kMaximumSizeKey[] = "max_size";
constexpr char kTitleKey[] = "title";
constexpr char kBoundsInRoot[] = "bounds_in_root";
constexpr char kPrimaryColorKey[] = "primary_color";
constexpr char kStatusBarColorKey[] = "status_bar_color";
constexpr char kLacrosProfileIdKey[] = "lacros_profile_id";

// Converts |size| to base::Value::List, e.g. { 100, 300 }.
base::Value::List ConvertSizeToList(const gfx::Size& size) {
  base::Value::List size_list;
  size_list.Append(size.width());
  size_list.Append(size.height());
  return size_list;
}

// Converts |rect| to base::Value, e.g. { 0, 100, 200, 300 }.
base::Value::List ConvertRectToList(const gfx::Rect& rect) {
  base::Value::List rect_list;
  rect_list.Append(rect.x());
  rect_list.Append(rect.y());
  rect_list.Append(rect.width());
  rect_list.Append(rect.height());
  return rect_list;
}

// Converts |uint32_t| to base::Value in string, e.g 123 to "123".
base::Value ConvertUintToValue(uint32_t number) {
  return base::Value(base::NumberToString(number));
}

// Converts `number` to base::Value in string, e.g. 123 to "123".
base::Value ConvertUint64ToValue(uint64_t number) {
  return base::Value(base::NumberToString(number));
}

// Gets bool value from base::Value::Dict, e.g. { "key": true } returns
// true.
std::optional<bool> GetBoolValueFromDict(const base::Value::Dict& dict,
                                         std::string_view key_name) {
  return dict.FindBool(key_name);
}

// Gets int value from base::Value::Dict, e.g. { "key": 100 } returns 100.
std::optional<int32_t> GetIntValueFromDict(const base::Value::Dict& dict,
                                           std::string_view key_name) {
  return dict.FindInt(key_name);
}

// Gets uint32_t value from base::Value::Dict, e.g. { "key": "123" } returns
// 123.
std::optional<uint32_t> GetUIntValueFromDict(const base::Value::Dict& dict,
                                             std::string_view key_name) {
  uint32_t result = 0;
  const std::string* value = dict.FindString(key_name);
  if (!value || !base::StringToUint(*value, &result)) {
    return std::nullopt;
  }
  return result;
}

// Gets uint64_t value from a base::Value::Dict where it is stored as a string,
// e.g. { "key": "123" } returns 123.
std::optional<uint64_t> GetUInt64ValueFromDict(const base::Value::Dict& dict,
                                               std::string_view key_name) {
  uint64_t result = 0;
  const std::string* value = dict.FindString(key_name);
  if (!value || !base::StringToUint64(*value, &result)) {
    return std::nullopt;
  }
  return result;
}

std::optional<std::string> GetStringValueFromDict(const base::Value::Dict& dict,
                                                  std::string_view key_name) {
  const std::string* value = dict.FindString(key_name);
  return value ? std::optional<std::string>(*value) : std::nullopt;
}

std::optional<GURL> GetUrlValueFromDict(const base::Value::Dict& dict,
                                        std::string_view key_name) {
  const std::string* value = dict.FindString(key_name);
  return value ? std::optional<GURL>(*value) : std::nullopt;
}

std::optional<std::u16string> GetU16StringValueFromDict(
    const base::Value::Dict& dict,
    std::string_view key_name) {
  std::u16string result;
  const std::string* value = dict.FindString(key_name);
  if (!value || !base::UTF8ToUTF16(value->c_str(), value->length(), &result))
    return std::nullopt;
  return result;
}

// Gets display id from base::Value::Dict, e.g. { "display_id": "22000000" }
// returns 22000000.
std::optional<int64_t> GetDisplayIdFromDict(const base::Value::Dict& dict) {
  const std::string* display_id_str = dict.FindString(kDisplayIdKey);
  int64_t display_id_value;
  if (display_id_str &&
      base::StringToInt64(*display_id_str, &display_id_value)) {
    return display_id_value;
  }

  return std::nullopt;
}

// Gets urls from the dictionary value.
std::vector<GURL> GetUrlsFromDict(const base::Value::Dict& dict) {
  const base::Value::List* urls_path_value = dict.FindList(kUrlsKey);
  std::vector<GURL> url_paths;
  if (!urls_path_value || urls_path_value->empty()) {
    return url_paths;
  }

  for (const auto& item : *urls_path_value) {
    GURL url(item.GetString());
    if (url.is_valid()) {
      url_paths.push_back(url);
    }
  }

  return url_paths;
}

// Gets std::vector<base::FilePath> from base::Value::Dict, e.g.
// {"file_paths": { "aa.cc", "bb.h", ... }} returns
// std::vector<base::FilePath>{"aa.cc", "bb.h", ...}.
std::vector<base::FilePath> GetFilePathsFromDict(
    const base::Value::Dict& dict) {
  const base::Value::List* file_paths_value = dict.FindList(kFilePathsKey);
  std::vector<base::FilePath> file_paths;
  if (!file_paths_value || file_paths_value->empty())
    return file_paths;

  for (const auto& item : *file_paths_value) {
    if (item.GetString().empty())
      continue;
    file_paths.push_back(base::FilePath(item.GetString()));
  }

  return file_paths;
}

// Gets gfx::Size from base::Value, e.g. { 100, 300 } returns
// gfx::Size(100, 300).
std::optional<gfx::Size> GetSizeFromDict(const base::Value::Dict& dict,
                                         std::string_view key_name) {
  const base::Value::List* size_value = dict.FindList(key_name);
  if (!size_value || size_value->size() != 2) {
    return std::nullopt;
  }

  return gfx::Size((*size_value)[0].GetInt(), (*size_value)[1].GetInt());
}

// Gets gfx::Rect from base::Value, e.g. { 0, 100, 200, 300 } returns
// gfx::Rect(0, 100, 200, 300).
std::optional<gfx::Rect> GetBoundsRectFromDict(const base::Value::Dict& dict,
                                               std::string_view key_name) {
  const base::Value::List* rect_value = dict.FindList(key_name);
  if (!rect_value || rect_value->size() != 4) {
    return std::nullopt;
  }

  return gfx::Rect((*rect_value)[0].GetInt(), (*rect_value)[1].GetInt(),
                   (*rect_value)[2].GetInt(), (*rect_value)[3].GetInt());
}

// Gets WindowStateType from base::Value::Dict, e.g. { "window_state_type":
// 2 } returns WindowStateType::kMinimized.
std::optional<chromeos::WindowStateType> GetWindowStateTypeFromDict(
    const base::Value::Dict& dict) {
  return dict.Find(kWindowStateTypeKey)
             ? std::make_optional(static_cast<chromeos::WindowStateType>(
                   dict.FindInt(kWindowStateTypeKey).value()))
             : std::nullopt;
}

std::optional<ui::mojom::WindowShowState> GetPreMinimizedShowStateTypeFromDict(
    const base::Value::Dict& dict) {
  return dict.Find(kPreMinimizedShowStateTypeKey)
             ? std::make_optional(static_cast<ui::mojom::WindowShowState>(
                   dict.FindInt(kPreMinimizedShowStateTypeKey).value()))
             : std::nullopt;
}

base::Uuid GetGuidValueFromDict(const base::Value::Dict& dict,
                                const std::string& key_name) {
  if (const std::string* value = dict.FindString(key_name)) {
    return base::Uuid::ParseCaseInsensitive(*value);
  }
  return base::Uuid();
}

template <typename T>
void SetValueIntoDict(std::optional<T> value,
                      std::string_view key,
                      base::Value::Dict& dict) {
  if (value) {
    dict.Set(key, *value);
  }
}

}  // namespace

AppRestoreData::AppRestoreData() = default;

AppRestoreData::AppRestoreData(base::Value::Dict&& data) {
  event_flag = GetIntValueFromDict(data, kEventFlagKey);
  container = GetIntValueFromDict(data, kContainerKey);
  disposition = GetIntValueFromDict(data, kDispositionKey);
  override_url = GetUrlValueFromDict(data, kOverrideUrlKey);
  display_id = GetDisplayIdFromDict(data);
  handler_id = GetStringValueFromDict(data, kHandlerIdKey);
  file_paths = GetFilePathsFromDict(data);
  if (const base::Value::Dict* intent_value = data.FindDict(kIntentKey)) {
    intent = apps_util::ConvertDictToIntent(*intent_value);
  }

  browser_extra_info.urls = GetUrlsFromDict(data);
  browser_extra_info.active_tab_index =
      GetIntValueFromDict(data, kActiveTabIndexKey);
  browser_extra_info.first_non_pinned_tab_index =
      GetIntValueFromDict(data, kFirstNonPinnedTabIndexKey);
  browser_extra_info.app_type_browser =
      GetBoolValueFromDict(data, kAppTypeBrowserKey);
  browser_extra_info.app_name = GetStringValueFromDict(data, kAppNameKey);
  browser_extra_info.lacros_profile_id =
      GetUInt64ValueFromDict(data, kLacrosProfileIdKey);

  window_info.activation_index = GetIntValueFromDict(data, kActivationIndexKey);
  window_info.desk_id = GetIntValueFromDict(data, kDeskIdKey);
  window_info.desk_guid = GetGuidValueFromDict(data, kDeskUuidKey);
  window_info.current_bounds = GetBoundsRectFromDict(data, kCurrentBoundsKey);
  window_info.window_state_type = GetWindowStateTypeFromDict(data);
  window_info.pre_minimized_show_state_type =
      GetPreMinimizedShowStateTypeFromDict(data);
  window_info.snap_percentage = GetUIntValueFromDict(data, kSnapPercentageKey);
  window_info.app_title = GetU16StringValueFromDict(data, kTitleKey);

  std::optional<gfx::Size> max_size = GetSizeFromDict(data, kMaximumSizeKey);
  std::optional<gfx::Size> min_size = GetSizeFromDict(data, kMinimumSizeKey);
  std::optional<gfx::Rect> bounds_in_root =
      GetBoundsRectFromDict(data, kBoundsInRoot);
  if (max_size || min_size || bounds_in_root) {
    window_info.arc_extra_info = {.maximum_size = max_size,
                                  .minimum_size = min_size,
                                  .bounds_in_root = bounds_in_root};
  }
  primary_color = GetUIntValueFromDict(data, kPrimaryColorKey);
  status_bar_color = GetUIntValueFromDict(data, kStatusBarColorKey);
}

AppRestoreData::AppRestoreData(std::unique_ptr<AppLaunchInfo> app_launch_info) {
  if (!app_launch_info)
    return;

  event_flag = std::move(app_launch_info->event_flag);
  container = std::move(app_launch_info->container);
  disposition = std::move(app_launch_info->disposition);
  override_url = std::move(app_launch_info->override_url);
  display_id = std::move(app_launch_info->display_id);
  handler_id = std::move(app_launch_info->handler_id);
  file_paths = std::move(app_launch_info->file_paths);
  intent = std::move(app_launch_info->intent);

  browser_extra_info = std::move(app_launch_info->browser_extra_info);
}

AppRestoreData::~AppRestoreData() = default;

std::unique_ptr<AppRestoreData> AppRestoreData::Clone() const {
  auto data = std::make_unique<AppRestoreData>();

  data->event_flag = event_flag;
  data->container = container;
  data->disposition = disposition;
  data->override_url = override_url;
  data->display_id = display_id;
  data->handler_id = handler_id;
  data->file_paths = file_paths;
  if (intent) {
    data->intent = intent->Clone();
  }

  data->browser_extra_info = browser_extra_info;

  data->window_info = window_info;
  data->primary_color = primary_color;
  data->status_bar_color = status_bar_color;

  return data;
}

base::Value AppRestoreData::ConvertToValue() const {
  base::Value::Dict launch_info_dict;

  SetValueIntoDict(event_flag, kEventFlagKey, launch_info_dict);
  SetValueIntoDict(container, kContainerKey, launch_info_dict);
  SetValueIntoDict(disposition, kDispositionKey, launch_info_dict);

  if (override_url.has_value()) {
    launch_info_dict.Set(kOverrideUrlKey, override_url.value().spec());
  }

  if (display_id.has_value()) {
    launch_info_dict.Set(kDisplayIdKey,
                         base::NumberToString(display_id.value()));
  }

  SetValueIntoDict(handler_id, kHandlerIdKey, launch_info_dict);

  if (!file_paths.empty()) {
    base::Value::List file_paths_list;
    for (const base::FilePath& file_path : file_paths) {
      file_paths_list.Append(file_path.value());
    }
    launch_info_dict.Set(kFilePathsKey, std::move(file_paths_list));
  }

  if (intent) {
    launch_info_dict.Set(kIntentKey, apps_util::ConvertIntentToValue(intent));
  }

  if (!browser_extra_info.urls.empty()) {
    base::Value::List urls_list;
    for (const GURL& url : browser_extra_info.urls) {
      urls_list.Append(url.spec());
    }
    launch_info_dict.Set(kUrlsKey, std::move(urls_list));
  }

  SetValueIntoDict(browser_extra_info.active_tab_index, kActiveTabIndexKey,
                   launch_info_dict);
  SetValueIntoDict(browser_extra_info.first_non_pinned_tab_index,
                   kFirstNonPinnedTabIndexKey, launch_info_dict);
  SetValueIntoDict(browser_extra_info.app_name, kAppNameKey, launch_info_dict);
  SetValueIntoDict(browser_extra_info.app_type_browser, kAppTypeBrowserKey,
                   launch_info_dict);

  if (browser_extra_info.lacros_profile_id.has_value()) {
    launch_info_dict.Set(
        kLacrosProfileIdKey,
        ConvertUint64ToValue(browser_extra_info.lacros_profile_id.value()));
  }

  SetValueIntoDict(window_info.activation_index, kActivationIndexKey,
                   launch_info_dict);
  SetValueIntoDict(window_info.desk_id, kDeskIdKey, launch_info_dict);

  if (window_info.desk_guid.is_valid()) {
    launch_info_dict.Set(kDeskUuidKey,
                         window_info.desk_guid.AsLowercaseString());
  }

  if (window_info.current_bounds.has_value()) {
    launch_info_dict.Set(kCurrentBoundsKey,
                         ConvertRectToList(window_info.current_bounds.value()));
  }

  if (window_info.window_state_type.has_value()) {
    launch_info_dict.Set(
        kWindowStateTypeKey,
        static_cast<int>(window_info.window_state_type.value()));
  }

  if (window_info.pre_minimized_show_state_type.has_value()) {
    launch_info_dict.Set(
        kPreMinimizedShowStateTypeKey,
        static_cast<int>(window_info.pre_minimized_show_state_type.value()));
  }

  if (window_info.snap_percentage.has_value()) {
    launch_info_dict.Set(
        kSnapPercentageKey,
        ConvertUintToValue(window_info.snap_percentage.value()));
  }

  SetValueIntoDict(window_info.app_title, kTitleKey, launch_info_dict);

  if (window_info.arc_extra_info) {
    WindowInfo::ArcExtraInfo arc_info = *window_info.arc_extra_info;
    if (arc_info.maximum_size.has_value()) {
      launch_info_dict.Set(kMaximumSizeKey,
                           ConvertSizeToList(arc_info.maximum_size.value()));
    }

    if (arc_info.minimum_size.has_value()) {
      launch_info_dict.Set(kMinimumSizeKey,
                           ConvertSizeToList(arc_info.minimum_size.value()));
    }

    if (arc_info.bounds_in_root.has_value()) {
      launch_info_dict.Set(kBoundsInRoot,
                           ConvertRectToList(arc_info.bounds_in_root.value()));
    }
  }

  if (primary_color.has_value()) {
    launch_info_dict.Set(kPrimaryColorKey,
                         ConvertUintToValue(primary_color.value()));
  }

  if (status_bar_color.has_value()) {
    launch_info_dict.Set(kStatusBarColorKey,
                         ConvertUintToValue(status_bar_color.value()));
  }

  return base::Value(std::move(launch_info_dict));
}

void AppRestoreData::ModifyWindowInfo(const WindowInfo& info) {
  window_info = info;

  if (info.display_id) {
    display_id = info.display_id;
  }
}

void AppRestoreData::ModifyThemeColor(uint32_t window_primary_color,
                                      uint32_t window_status_bar_color) {
  primary_color = window_primary_color;
  status_bar_color = window_status_bar_color;
}

void AppRestoreData::ClearWindowInfo() {
  window_info = WindowInfo();
  primary_color.reset();
  status_bar_color.reset();
}

std::unique_ptr<AppLaunchInfo> AppRestoreData::GetAppLaunchInfo(
    const std::string& app_id,
    int window_id) const {
  auto app_launch_info = std::make_unique<AppLaunchInfo>(app_id, window_id);

  app_launch_info->event_flag = event_flag;
  app_launch_info->container = container;
  app_launch_info->disposition = disposition;
  app_launch_info->override_url = override_url;
  app_launch_info->display_id = display_id;
  app_launch_info->handler_id = handler_id;
  app_launch_info->file_paths = file_paths;
  if (intent)
    app_launch_info->intent = intent->Clone();

  app_launch_info->browser_extra_info = browser_extra_info;
  return app_launch_info;
}

std::unique_ptr<WindowInfo> AppRestoreData::GetWindowInfo() const {
  auto ret_window_info = std::make_unique<WindowInfo>();
  *ret_window_info = window_info;

  // Display id is set as the app launch parameter, so we don't need to return
  // the display id to restore the display id.
  ret_window_info->display_id.reset();
  return ret_window_info;
}

apps::WindowInfoPtr AppRestoreData::GetAppWindowInfo() const {
  auto apps_window_info = std::make_unique<apps::WindowInfo>();

  if (display_id.has_value()) {
    apps_window_info->display_id = display_id.value();
  }

  if (window_info.arc_extra_info.has_value() &&
      window_info.arc_extra_info->bounds_in_root.has_value()) {
    apps_window_info->bounds =
        window_info.arc_extra_info->bounds_in_root.value();
  } else if (window_info.current_bounds.has_value()) {
    apps_window_info->bounds = window_info.current_bounds.value();
  }

  if (window_info.window_state_type.has_value()) {
    apps_window_info->state =
        static_cast<int32_t>(window_info.window_state_type.value());
  }

  return apps_window_info;
}

std::string AppRestoreData::ToString() const {
  return ConvertToValue().DebugString();
}

bool AppRestoreData::operator==(const AppRestoreData& other) const {
  if (!intent && other.intent) {
    return false;
  }
  if (intent && !other.intent) {
    return false;
  }
  if (intent && other.intent && *intent != *other.intent) {
    return false;
  }

  return event_flag == other.event_flag && container == other.container &&
         disposition == other.disposition &&
         override_url == other.override_url && display_id == other.display_id &&
         handler_id == other.handler_id && file_paths == other.file_paths &&
         browser_extra_info == other.browser_extra_info &&
         window_info == other.window_info &&
         primary_color == other.primary_color &&
         status_bar_color == other.status_bar_color;
}

bool AppRestoreData::operator!=(const AppRestoreData& other) const {
  return !(*this == other);
}
}  // namespace app_restore
