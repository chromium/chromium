// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/app_restore/app_restore_data.h"

#include <utility>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/window_info.h"
#include "components/services/app_service/public/cpp/intent_util.h"

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

// Gets bool value from base::Value::Dict, e.g. { "key": true } returns
// true.
absl::optional<bool> GetBoolValueFromDict(const base::Value::Dict& dict,
                                          base::StringPiece key_name) {
  return dict.FindBool(key_name);
}

// Gets int value from base::Value::Dict, e.g. { "key": 100 } returns 100.
absl::optional<int32_t> GetIntValueFromDict(const base::Value::Dict& dict,
                                            base::StringPiece key_name) {
  return dict.FindInt(key_name);
}

// Gets uint32_t value from base::Value::Dict, e.g. { "key": "123" } returns
// 123.
absl::optional<uint32_t> GetUIntValueFromDict(const base::Value::Dict& dict,
                                              base::StringPiece key_name) {
  uint32_t result = 0;
  const std::string* value = dict.FindString(key_name);
  if (!value || !base::StringToUint(*value, &result)) {
    return absl::nullopt;
  }
  return result;
}

absl::optional<std::string> GetStringValueFromDict(
    const base::Value::Dict& dict,
    base::StringPiece key_name) {
  const std::string* value = dict.FindString(key_name);
  return value ? absl::optional<std::string>(*value) : absl::nullopt;
}

absl::optional<GURL> GetUrlValueFromDict(const base::Value::Dict& dict,
                                         base::StringPiece key_name) {
  const std::string* value = dict.FindString(key_name);
  return value ? absl::optional<GURL>(*value) : absl::nullopt;
}

absl::optional<std::u16string> GetU16StringValueFromDict(
    const base::Value::Dict& dict,
    base::StringPiece key_name) {
  std::u16string result;
  const std::string* value = dict.FindString(key_name);
  if (!value || !base::UTF8ToUTF16(value->c_str(), value->length(), &result))
    return absl::nullopt;
  return result;
}

// Gets display id from base::Value::Dict, e.g. { "display_id": "22000000" }
// returns 22000000.
absl::optional<int64_t> GetDisplayIdFromDict(const base::Value::Dict& dict) {
  const std::string* display_id_str = dict.FindString(kDisplayIdKey);
  int64_t display_id_value;
  if (display_id_str &&
      base::StringToInt64(*display_id_str, &display_id_value)) {
    return display_id_value;
  }

  return absl::nullopt;
}

// Gets urls from the dictionary value.
absl::optional<std::vector<GURL>> GetUrlsFromDict(
    const base::Value::Dict& dict) {
  const base::Value::List* urls_path_value = dict.FindList(kUrlsKey);
  if (!urls_path_value || urls_path_value->empty()) {
    return absl::nullopt;
  }

  std::vector<GURL> url_paths;
  for (const auto& item : *urls_path_value) {
    if (item.GetString().empty())
      continue;
    GURL url(item.GetString());
    if (!url.is_valid())
      continue;
    url_paths.push_back(url);
  }

  return url_paths;
}

// Gets std::vector<base::FilePath> from base::Value::Dict, e.g.
// {"file_paths": { "aa.cc", "bb.h", ... }} returns
// std::vector<base::FilePath>{"aa.cc", "bb.h", ...}.
absl::optional<std::vector<base::FilePath>> GetFilePathsFromDict(
    const base::Value::Dict& dict) {
  const base::Value::List* file_paths_value = dict.FindList(kFilePathsKey);
  if (!file_paths_value || file_paths_value->empty())
    return absl::nullopt;

  std::vector<base::FilePath> file_paths;
  for (const auto& item : *file_paths_value) {
    if (item.GetString().empty())
      continue;
    file_paths.push_back(base::FilePath(item.GetString()));
  }

  return file_paths;
}

// Gets gfx::Size from base::Value, e.g. { 100, 300 } returns
// gfx::Size(100, 300).
absl::optional<gfx::Size> GetSizeFromDict(const base::Value::Dict& dict,
                                          base::StringPiece key_name) {
  const base::Value::List* size_value = dict.FindList(key_name);
  if (!size_value || size_value->size() != 2) {
    return absl::nullopt;
  }

  std::vector<int> size;
  for (const auto& item : *size_value)
    size.push_back(item.GetInt());

  return gfx::Size(size[0], size[1]);
}

// Gets gfx::Rect from base::Value, e.g. { 0, 100, 200, 300 } returns
// gfx::Rect(0, 100, 200, 300).
absl::optional<gfx::Rect> GetBoundsRectFromDict(const base::Value::Dict& dict,
                                                base::StringPiece key_name) {
  const base::Value::List* rect_value = dict.FindList(key_name);
  if (!rect_value || rect_value->empty())
    return absl::nullopt;

  std::vector<int> rect;
  for (const auto& item : *rect_value)
    rect.push_back(item.GetInt());

  if (rect.size() != 4)
    return absl::nullopt;

  return gfx::Rect(rect[0], rect[1], rect[2], rect[3]);
}

// Gets WindowStateType from base::Value::Dict, e.g. { "window_state_type":
// 2 } returns WindowStateType::kMinimized.
absl::optional<chromeos::WindowStateType> GetWindowStateTypeFromDict(
    const base::Value::Dict& dict) {
  return dict.Find(kWindowStateTypeKey)
             ? absl::make_optional(static_cast<chromeos::WindowStateType>(
                   dict.FindInt(kWindowStateTypeKey).value()))
             : absl::nullopt;
}

absl::optional<ui::WindowShowState> GetPreMinimizedShowStateTypeFromDict(
    const base::Value::Dict& dict) {
  return dict.Find(kPreMinimizedShowStateTypeKey)
             ? absl::make_optional(static_cast<ui::WindowShowState>(
                   dict.FindInt(kPreMinimizedShowStateTypeKey).value()))
             : absl::nullopt;
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
  urls = GetUrlsFromDict(data);
  active_tab_index = GetIntValueFromDict(data, kActiveTabIndexKey);
  file_paths = GetFilePathsFromDict(data);
  app_type_browser = GetBoolValueFromDict(data, kAppTypeBrowserKey);
  app_name = GetStringValueFromDict(data, kAppNameKey);
  activation_index = GetIntValueFromDict(data, kActivationIndexKey);
  first_non_pinned_tab_index =
      GetIntValueFromDict(data, kFirstNonPinnedTabIndexKey);
  desk_id = GetIntValueFromDict(data, kDeskIdKey);
  current_bounds = GetBoundsRectFromDict(data, kCurrentBoundsKey);
  window_state_type = GetWindowStateTypeFromDict(data);
  pre_minimized_show_state_type = GetPreMinimizedShowStateTypeFromDict(data);
  snap_percentage = GetUIntValueFromDict(data, kSnapPercentageKey);
  maximum_size = GetSizeFromDict(data, kMaximumSizeKey);
  minimum_size = GetSizeFromDict(data, kMinimumSizeKey);
  title = GetU16StringValueFromDict(data, kTitleKey);
  bounds_in_root = GetBoundsRectFromDict(data, kBoundsInRoot);
  primary_color = GetUIntValueFromDict(data, kPrimaryColorKey);
  status_bar_color = GetUIntValueFromDict(data, kStatusBarColorKey);

  const base::Value::Dict* intent_value = data.FindDict(kIntentKey);
  if (intent_value) {
    intent = apps_util::ConvertDictToIntent(*intent_value);
  }
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
  urls = std::move(app_launch_info->urls);
  active_tab_index = std::move(app_launch_info->active_tab_index);
  first_non_pinned_tab_index =
      std::move(app_launch_info->first_non_pinned_tab_index);
  file_paths = std::move(app_launch_info->file_paths);
  intent = std::move(app_launch_info->intent);
  app_type_browser = std::move(app_launch_info->app_type_browser);
  app_name = std::move(app_launch_info->app_name);
  tab_group_infos = std::move(app_launch_info->tab_group_infos);
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

  if (override_url.has_value())
    data->override_url = override_url.value();

  if (display_id.has_value())
    data->display_id = display_id.value();

  if (handler_id.has_value())
    data->handler_id = handler_id.value();

  if (urls.has_value())
    data->urls = urls.value();

  if (active_tab_index.has_value())
    data->active_tab_index = active_tab_index.value();

  if (first_non_pinned_tab_index.has_value())
    data->first_non_pinned_tab_index = first_non_pinned_tab_index.value();

  if (intent)
    data->intent = intent->Clone();

  if (file_paths.has_value())
    data->file_paths = file_paths.value();

  if (app_type_browser.has_value())
    data->app_type_browser = app_type_browser.value();

  if (app_name.has_value())
    data->app_name = app_name.value();

  if (title.has_value())
    data->title = title.value();

  if (activation_index.has_value())
    data->activation_index = activation_index.value();

  if (desk_id.has_value())
    data->desk_id = desk_id.value();

  if (current_bounds.has_value())
    data->current_bounds = current_bounds.value();

  if (window_state_type.has_value())
    data->window_state_type = window_state_type.value();

  if (pre_minimized_show_state_type.has_value())
    data->pre_minimized_show_state_type = pre_minimized_show_state_type.value();

  if (snap_percentage.has_value())
    data->snap_percentage = snap_percentage.value();

  if (maximum_size.has_value())
    data->maximum_size = maximum_size.value();

  if (minimum_size.has_value())
    data->minimum_size = minimum_size.value();

  if (bounds_in_root.has_value())
    data->bounds_in_root = bounds_in_root.value();

  if (primary_color.has_value())
    data->primary_color = primary_color.value();

  if (status_bar_color.has_value())
    data->status_bar_color = status_bar_color.value();

  if (tab_group_infos.has_value())
    data->tab_group_infos = tab_group_infos.value();

  return data;
}

base::Value AppRestoreData::ConvertToValue() const {
  base::Value::Dict launch_info_dict;

  if (event_flag.has_value())
    launch_info_dict.Set(kEventFlagKey, event_flag.value());

  if (container.has_value())
    launch_info_dict.Set(kContainerKey, container.value());

  if (disposition.has_value())
    launch_info_dict.Set(kDispositionKey, disposition.value());

  if (override_url.has_value())
    launch_info_dict.Set(kOverrideUrlKey, override_url.value().spec());

  if (display_id.has_value()) {
    launch_info_dict.Set(kDisplayIdKey,
                         base::NumberToString(display_id.value()));
  }

  if (handler_id.has_value())
    launch_info_dict.Set(kHandlerIdKey, handler_id.value());

  if (urls.has_value() && !urls.value().empty()) {
    base::Value::List urls_list;
    for (auto& url : urls.value())
      urls_list.Append(url.spec());
    launch_info_dict.Set(kUrlsKey, std::move(urls_list));
  }

  if (active_tab_index.has_value())
    launch_info_dict.Set(kActiveTabIndexKey, active_tab_index.value());

  if (first_non_pinned_tab_index.has_value()) {
    launch_info_dict.Set(kFirstNonPinnedTabIndexKey,
                         first_non_pinned_tab_index.value());
  }

  if (intent) {
    launch_info_dict.Set(kIntentKey, apps_util::ConvertIntentToValue(intent));
  }

  if (file_paths.has_value() && !file_paths.value().empty()) {
    base::Value::List file_paths_list;
    for (auto& file_path : file_paths.value())
      file_paths_list.Append(file_path.value());
    launch_info_dict.Set(kFilePathsKey, std::move(file_paths_list));
  }

  if (app_type_browser.has_value())
    launch_info_dict.Set(kAppTypeBrowserKey, app_type_browser.value());

  if (app_name.has_value())
    launch_info_dict.Set(kAppNameKey, app_name.value());

  if (title.has_value())
    launch_info_dict.Set(kTitleKey, base::UTF16ToUTF8(title.value()));

  if (activation_index.has_value())
    launch_info_dict.Set(kActivationIndexKey, activation_index.value());

  if (desk_id.has_value())
    launch_info_dict.Set(kDeskIdKey, desk_id.value());

  if (current_bounds.has_value()) {
    launch_info_dict.Set(kCurrentBoundsKey,
                         ConvertRectToList(current_bounds.value()));
  }

  if (window_state_type.has_value()) {
    launch_info_dict.Set(kWindowStateTypeKey,
                         static_cast<int>(window_state_type.value()));
  }

  if (pre_minimized_show_state_type.has_value()) {
    launch_info_dict.Set(
        kPreMinimizedShowStateTypeKey,
        static_cast<int>(pre_minimized_show_state_type.value()));
  }

  if (snap_percentage.has_value()) {
    launch_info_dict.Set(kSnapPercentageKey,
                         ConvertUintToValue(snap_percentage.value()));
  }

  if (maximum_size.has_value()) {
    launch_info_dict.Set(kMaximumSizeKey,
                         ConvertSizeToList(maximum_size.value()));
  }

  if (minimum_size.has_value()) {
    launch_info_dict.Set(kMinimumSizeKey,
                         ConvertSizeToList(minimum_size.value()));
  }

  if (bounds_in_root.has_value()) {
    launch_info_dict.Set(kBoundsInRoot,
                         ConvertRectToList(bounds_in_root.value()));
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

void AppRestoreData::ModifyWindowInfo(const WindowInfo& window_info) {
  if (window_info.activation_index.has_value())
    activation_index = window_info.activation_index.value();

  if (window_info.desk_id.has_value())
    desk_id = window_info.desk_id.value();

  if (window_info.current_bounds.has_value())
    current_bounds = window_info.current_bounds.value();

  if (window_info.window_state_type.has_value())
    window_state_type = window_info.window_state_type.value();

  if (window_info.pre_minimized_show_state_type.has_value()) {
    pre_minimized_show_state_type =
        window_info.pre_minimized_show_state_type.value();
  }

  if (window_info.snap_percentage.has_value())
    snap_percentage = window_info.snap_percentage.value();

  if (window_info.display_id.has_value())
    display_id = window_info.display_id.value();

  if (window_info.app_title.has_value())
    title = window_info.app_title;

  if (window_info.arc_extra_info.has_value()) {
    minimum_size = window_info.arc_extra_info->minimum_size;
    maximum_size = window_info.arc_extra_info->maximum_size;
    bounds_in_root = window_info.arc_extra_info->bounds_in_root;
  }
}

void AppRestoreData::ModifyThemeColor(uint32_t window_primary_color,
                                      uint32_t window_status_bar_color) {
  primary_color = window_primary_color;
  status_bar_color = window_status_bar_color;
}

void AppRestoreData::ClearWindowInfo() {
  activation_index.reset();
  desk_id.reset();
  current_bounds.reset();
  window_state_type.reset();
  pre_minimized_show_state_type.reset();
  snap_percentage.reset();
  minimum_size.reset();
  maximum_size.reset();
  title.reset();
  bounds_in_root.reset();
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
  app_launch_info->display_id = display_id;
  app_launch_info->active_tab_index = active_tab_index;
  app_launch_info->handler_id = handler_id;
  app_launch_info->urls = urls;
  app_launch_info->first_non_pinned_tab_index = first_non_pinned_tab_index;
  app_launch_info->file_paths = file_paths;
  if (intent)
    app_launch_info->intent = intent->Clone();
  app_launch_info->app_type_browser = app_type_browser;
  app_launch_info->app_name = app_name;
  app_launch_info->tab_group_infos = tab_group_infos;
  return app_launch_info;
}

std::unique_ptr<WindowInfo> AppRestoreData::GetWindowInfo() const {
  auto window_info = std::make_unique<WindowInfo>();

  if (activation_index.has_value())
    window_info->activation_index = activation_index;

  if (desk_id.has_value())
    window_info->desk_id = desk_id.value();

  if (current_bounds.has_value())
    window_info->current_bounds = current_bounds.value();

  if (window_state_type.has_value())
    window_info->window_state_type = window_state_type.value();

  if (pre_minimized_show_state_type.has_value()) {
    window_info->pre_minimized_show_state_type =
        pre_minimized_show_state_type.value();
  }

  if (snap_percentage.has_value())
    window_info->snap_percentage = snap_percentage;

  if (title.has_value())
    window_info->app_title = title;

  if (maximum_size.has_value() || minimum_size.has_value() ||
      title.has_value() || bounds_in_root.has_value()) {
    window_info->arc_extra_info = WindowInfo::ArcExtraInfo();
    window_info->arc_extra_info->maximum_size = maximum_size;
    window_info->arc_extra_info->minimum_size = minimum_size;
    window_info->arc_extra_info->bounds_in_root = bounds_in_root;
  }

  // Display id is set as the app launch parameter, so we don't need to return
  // the display id to restore the display id.
  return window_info;
}

apps::WindowInfoPtr AppRestoreData::GetAppWindowInfo() const {
  apps::WindowInfoPtr window_info = std::make_unique<apps::WindowInfo>();

  if (display_id.has_value())
    window_info->display_id = display_id.value();

  if (bounds_in_root.has_value()) {
    window_info->bounds = gfx::Rect{
        bounds_in_root.value().x(), bounds_in_root.value().y(),
        bounds_in_root.value().width(), bounds_in_root.value().height()};
  } else if (current_bounds.has_value()) {
    window_info->bounds = gfx::Rect{
        current_bounds.value().x(), current_bounds.value().y(),
        current_bounds.value().width(), current_bounds.value().height()};
  }

  if (window_state_type.has_value())
    window_info->state = static_cast<int32_t>(window_state_type.value());

  return window_info;
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
         handler_id == other.handler_id && urls == other.urls &&
         active_tab_index == other.active_tab_index &&
         first_non_pinned_tab_index == other.first_non_pinned_tab_index &&
         file_paths == other.file_paths &&
         app_type_browser == other.app_type_browser &&
         app_name == other.app_name && title == other.title &&
         activation_index == other.activation_index &&
         desk_id == other.desk_id && current_bounds == other.current_bounds &&
         window_state_type == other.window_state_type &&
         pre_minimized_show_state_type == other.pre_minimized_show_state_type &&
         snap_percentage == other.snap_percentage &&
         tab_group_infos == other.tab_group_infos &&
         minimum_size == other.minimum_size &&
         maximum_size == other.maximum_size &&
         bounds_in_root == other.bounds_in_root &&
         primary_color == other.primary_color &&
         status_bar_color == other.status_bar_color;
}

bool AppRestoreData::operator!=(const AppRestoreData& other) const {
  return !(*this == other);
}
}  // namespace app_restore
