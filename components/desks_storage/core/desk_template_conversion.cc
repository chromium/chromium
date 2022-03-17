// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desks_storage/core/desk_template_conversion.h"

#include "ash/public/cpp/desk_template.h"
#include "base/guid.h"
#include "base/json/json_reader.h"
#include "base/json/values_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/app_constants/constants.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/restore_data.h"
#include "components/app_restore/window_info.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/rect.h"

namespace {

// JSON value keys.
constexpr char kActiveTabIndex[] = "active_tab_index";
constexpr char kAppId[] = "app_id";
constexpr char kApps[] = "apps";
constexpr char kAppName[] = "app_name";
constexpr char kAppType[] = "app_type";
constexpr char kAppTypeBrowser[] = "BROWSER";
constexpr char kAppTypeChrome[] = "CHROME_APP";
constexpr char kAppTypeProgressiveWeb[] = "PWA";
constexpr char kCreatedTime[] = "created_time_usec";
constexpr char kDesk[] = "desk";
constexpr char kDisplayId[] = "display_id";
constexpr char kName[] = "name";
constexpr char kPreMinimizedWindowState[] = "pre_minimized_window_state";
constexpr char kTabs[] = "tabs";
constexpr char kTabUrl[] = "url";
constexpr char kUpdatedTime[] = "updated_time_usec";
constexpr char kUuid[] = "uuid";
constexpr char kVersion[] = "version";
constexpr char kWindowId[] = "window_id";
constexpr char kWindowBound[] = "window_bound";
constexpr char kWindowBoundHeight[] = "height";
constexpr char kWindowBoundLeft[] = "left";
constexpr char kWindowBoundTop[] = "top";
constexpr char kWindowBoundWidth[] = "width";
constexpr char kWindowState[] = "window_state";
constexpr char kWindowStateNormal[] = "NORMAL";
constexpr char kWindowStateMinimized[] = "MINIMIZED";
constexpr char kWindowStateMaximized[] = "MAXIMIZED";
constexpr char kWindowStateFullscreen[] = "FULLSCREEN";
constexpr char kWindowStatePrimarySnapped[] = "PRIMARY_SNAPPED";
constexpr char kWindowStateSecondarySnapped[] = "SECONDARY_SNAPPED";
constexpr char kZIndex[] = "z_index";

// Version number.
constexpr int kVersionNum = 1;

// Conversion to desk methods.

bool GetString(const base::Value* dict, const char* key, std::string* out) {
  const base::Value* value =
      dict->FindKeyOfType(key, base::Value::Type::STRING);
  if (!value)
    return false;

  *out = value->GetString();
  return true;
}

bool GetString(const base::Value& dict, const char* key, std::string* out) {
  return GetString(&dict, key, out);
}

bool GetInt(const base::Value* dict, const char* key, int* out) {
  const base::Value* value =
      dict->FindKeyOfType(key, base::Value::Type::INTEGER);
  if (!value)
    return false;

  *out = value->GetInt();
  return true;
}

bool GetInt(const base::Value& dict, const char* key, int* out) {
  return GetInt(&dict, key, out);
}

// Get App ID from App proto.
std::string GetJsonAppId(const base::Value& app) {
  std::string app_type;
  if (!GetString(app, kAppType, &app_type))
    return std::string();  // App Type must be specified.

  if (app_type == kAppTypeBrowser) {
    // Browser app has a known app ID.
    return std::string(app_constants::kChromeAppId);
  } else if (app_type == kAppTypeChrome || app_type == kAppTypeProgressiveWeb) {
    // Read the provided app ID
    std::string app_id;
    if (GetString(app, kAppId, &app_id)) {
      return app_id;
    }
  }

  // Unsupported type
  return std::string();
}

// Convert App JSON to |app_restore::AppLaunchInfo|.
std::unique_ptr<app_restore::AppLaunchInfo> ConvertJsonToAppLaunchInfo(
    const base::Value& app) {
  int32_t window_id;
  if (!GetInt(app, kWindowId, &window_id))
    return nullptr;

  const std::string app_id = GetJsonAppId(app);

  if (app_id.empty())
    return nullptr;

  std::unique_ptr<app_restore::AppLaunchInfo> app_launch_info =
      std::make_unique<app_restore::AppLaunchInfo>(app_id, window_id);

  std::string display_id_string;
  int64_t display_id;
  if (GetString(app, kDisplayId, &display_id_string) &&
      base::StringToInt64(display_id_string, &display_id)) {
    app_launch_info->display_id = display_id;
  }

  std::string app_type;
  if (!GetString(app, kAppType, &app_type)) {
    // This should never happen. |APP_NOT_SET| corresponds to empty |app_id|.
    // This method will early return when |app_id| is empty.
    NOTREACHED();
    return nullptr;
  }

  std::string app_name;
  if (GetString(app, kAppName, &app_name))
    app_launch_info->app_name = app_name;

  if (app_type == kAppTypeBrowser) {
    int active_tab_index;
    if (GetInt(app, kActiveTabIndex, &active_tab_index))
      app_launch_info->active_tab_index = active_tab_index;

    // Fill in the URL list
    app_launch_info->urls.emplace();
    const base::Value* tabs = app.FindKeyOfType(kTabs, base::Value::Type::LIST);
    if (tabs) {
      for (auto& tab : tabs->GetListDeprecated()) {
        std::string url;
        if (GetString(tab, kTabUrl, &url)) {
          app_launch_info->urls.value().emplace_back(url);
        }
      }
    }
  }
  // For Chrome apps and PWAs, the |app_id| is sufficient for identification.

  return app_launch_info;
}

bool IsValidWindowState(const std::string& window_state) {
  return window_state == kWindowStateNormal ||
         window_state == kWindowStateMinimized ||
         window_state == kWindowStateMaximized ||
         window_state == kWindowStateFullscreen ||
         window_state == kWindowStatePrimarySnapped ||
         window_state == kWindowStateSecondarySnapped;
}

// Convert JSON string WindowState |state| to ui::WindowShowState used by
// the app_restore::WindowInfo struct.
ui::WindowShowState ToUiWindowState(const std::string& window_state) {
  if (window_state == kWindowStateNormal)
    return ui::WindowShowState::SHOW_STATE_NORMAL;
  else if (window_state == kWindowStateMinimized)
    return ui::WindowShowState::SHOW_STATE_MINIMIZED;
  else if (window_state == kWindowStateMaximized)
    return ui::WindowShowState::SHOW_STATE_MAXIMIZED;
  else if (window_state == kWindowStateFullscreen)
    return ui::WindowShowState::SHOW_STATE_FULLSCREEN;
  else if (window_state == kWindowStatePrimarySnapped)
    return ui::WindowShowState::SHOW_STATE_NORMAL;
  else if (window_state == kWindowStateSecondarySnapped)
    return ui::WindowShowState::SHOW_STATE_NORMAL;
  // We should never reach here unless we have been passed an invalid window
  // state
  DCHECK(IsValidWindowState(window_state));
  return ui::WindowShowState::SHOW_STATE_NORMAL;
}

// Convert JSON string WindowState |state| to chromeos::WindowStateType used by
// the app_restore::WindowInfo struct.
chromeos::WindowStateType ToChromeOsWindowState(
    const std::string& window_state) {
  if (window_state == kWindowStateNormal)
    return chromeos::WindowStateType::kNormal;
  else if (window_state == kWindowStateMinimized)
    return chromeos::WindowStateType::kMinimized;
  else if (window_state == kWindowStateMaximized)
    return chromeos::WindowStateType::kMaximized;
  else if (window_state == kWindowStateFullscreen)
    return chromeos::WindowStateType::kFullscreen;
  else if (window_state == kWindowStatePrimarySnapped)
    return chromeos::WindowStateType::kPrimarySnapped;
  else if (window_state == kWindowStateSecondarySnapped)
    return chromeos::WindowStateType::kSecondarySnapped;

  // We should never reach here unless we have been passed an invalid window
  // state.
  DCHECK(IsValidWindowState(window_state));
  return chromeos::WindowStateType::kNormal;
}

// Fill |out_window_info| with information from JSON |app|.
void FillWindowInfoFromJson(const base::Value& app,
                            app_restore::WindowInfo* out_window_info) {
  std::string window_state;
  if (GetString(app, kWindowState, &window_state) &&
      IsValidWindowState(window_state)) {
    out_window_info->window_state_type.emplace(
        ToChromeOsWindowState(window_state));
  }

  const base::Value* window_bound = app.FindDictKey(kWindowBound);
  int top;
  int left;
  int width;
  int height;
  if (window_bound && GetInt(window_bound, kWindowBoundTop, &top) &&
      GetInt(window_bound, kWindowBoundLeft, &left) &&
      GetInt(window_bound, kWindowBoundWidth, &width) &&
      GetInt(window_bound, kWindowBoundHeight, &height)) {
    out_window_info->current_bounds.emplace(left, top, width, height);
  }

  int z_index;
  if (GetInt(app, kZIndex, &z_index))
    out_window_info->activation_index.emplace(z_index);

  std::string display_id_string;
  int64_t display_id;
  if (GetString(app, kDisplayId, &display_id_string) &&
      base::StringToInt64(display_id_string, &display_id)) {
    out_window_info->display_id = display_id;
  }

  std::string pre_minimized_window_state;
  if (GetString(app, kPreMinimizedWindowState, &pre_minimized_window_state) &&
      IsValidWindowState(pre_minimized_window_state)) {
    out_window_info->pre_minimized_show_state_type.emplace(
        ToUiWindowState(pre_minimized_window_state));
  }
}

// Convert a desk template to |app_restore::RestoreData|.
std::unique_ptr<app_restore::RestoreData> ConvertJsonToRestoreData(
    const base::Value* desk) {
  std::unique_ptr<app_restore::RestoreData> restore_data =
      std::make_unique<app_restore::RestoreData>();

  const base::Value* apps = desk->FindListKey(kApps);
  if (apps) {
    for (const auto& app : apps->GetListDeprecated()) {
      std::unique_ptr<app_restore::AppLaunchInfo> app_launch_info =
          ConvertJsonToAppLaunchInfo(app);
      if (!app_launch_info)
        continue;  // Skip unsupported app.

      int window_id;
      if (!GetInt(app, kWindowId, &window_id))
        return nullptr;

      const std::string app_id = app_launch_info->app_id;
      restore_data->AddAppLaunchInfo(std::move(app_launch_info));

      app_restore::WindowInfo app_window_info;
      FillWindowInfoFromJson(app, &app_window_info);

      restore_data->ModifyWindowInfo(app_id, window_id, app_window_info);
    }
  }

  return restore_data;
}

// Conversion to value methods.

base::Value ConvertWindowBoundToValue(const gfx::Rect& rect) {
  base::Value rectangle_value(base::Value::Type::DICTIONARY);

  rectangle_value.SetKey(kWindowBoundTop, base::Value(rect.y()));
  rectangle_value.SetKey(kWindowBoundLeft, base::Value(rect.x()));
  rectangle_value.SetKey(kWindowBoundHeight, base::Value(rect.height()));
  rectangle_value.SetKey(kWindowBoundWidth, base::Value(rect.width()));

  return rectangle_value;
}

// Convert ui::WindowStateType |window_state| to std::string used by the
// base::Value representation.
std::string ChromeOsWindowStateToString(
    const chromeos::WindowStateType& window_state) {
  switch (window_state) {
    case chromeos::WindowStateType::kNormal:
      return kWindowStateNormal;
    case chromeos::WindowStateType::kMinimized:
      return kWindowStateMinimized;
    case chromeos::WindowStateType::kMaximized:
      return kWindowStateMaximized;
    case chromeos::WindowStateType::kFullscreen:
      return kWindowStateFullscreen;
    case chromeos::WindowStateType::kPrimarySnapped:
      return kWindowStatePrimarySnapped;
    case chromeos::WindowStateType::kSecondarySnapped:
      return kWindowStateSecondarySnapped;
    default:
      // Available states in JSON representation is a subset of all window
      // states enumerated by WindowStateType. Default to normal if not
      // supported.
      return kWindowStateNormal;
  }
}

// Convert ui::WindowShowState |state| to JSON used by the base::Value
// representation.
std::string UiWindowStateToString(const ui::WindowShowState& window_state) {
  switch (window_state) {
    case ui::WindowShowState::SHOW_STATE_NORMAL:
      return kWindowStateNormal;
    case ui::WindowShowState::SHOW_STATE_MINIMIZED:
      return kWindowStateMinimized;
    case ui::WindowShowState::SHOW_STATE_MAXIMIZED:
      return kWindowStateMaximized;
    case ui::WindowShowState::SHOW_STATE_FULLSCREEN:
      return kWindowStateFullscreen;
    default:
      // available states in JSON representation is a subset
      // of all window states enumerated by WindowShowState.
      // Default to normal if not supported.
      return kWindowStateNormal;
  }
}

base::Value ConvertURLsToBrowserAppTabValues(const std::vector<GURL>& urls) {
  base::Value tab_list = base::Value(base::Value::Type::LIST);

  for (const auto& url : urls) {
    base::Value browser_tab = base::Value(base::Value::Type::DICTIONARY);
    browser_tab.SetKey(kTabUrl, base::Value(url.spec()));
    tab_list.Append(std::move(browser_tab));
  }

  return tab_list;
}

std::string GetAppTypeForJson(apps::AppRegistryCache* apps_cache,
                              const std::string& app_id) {
  const apps::mojom::AppType app_type = app_id == app_constants::kChromeAppId
                                            ? apps::mojom::AppType::kWeb
                                            : apps_cache->GetAppType(app_id);

  switch (app_type) {
    case apps::mojom::AppType::kWeb:
      return app_id == app_constants::kChromeAppId ? kAppTypeBrowser
                                                   : kAppTypeProgressiveWeb;
    case apps::mojom::AppType::kChromeApp:
      return kAppTypeChrome;
    default:
      // Default to browser if unsupported, this shouldn't be captured and
      // there is no error type in the proto definition.
      return kAppTypeBrowser;
  }
}

base::Value ConvertWindowToDeskApp(const std::string& app_id,
                                   const int window_id,
                                   const app_restore::AppRestoreData* app,
                                   apps::AppRegistryCache* apps_cache) {
  base::Value app_data = base::Value(base::Value::Type::DICTIONARY);

  if (app->current_bounds.has_value()) {
    app_data.SetKey(kWindowBound,
                    ConvertWindowBoundToValue(app->current_bounds.value()));
  }

  if (app->window_state_type.has_value()) {
    app_data.SetKey(kWindowState, base::Value(ChromeOsWindowStateToString(
                                      app->window_state_type.value())));
  }

  if (app->activation_index.has_value())
    app_data.SetKey(kZIndex, base::Value(app->activation_index.value()));

  std::string app_type = GetAppTypeForJson(apps_cache, app_id);

  app_data.SetKey(kAppType, base::Value(app_type));

  if (app->urls.has_value())
    app_data.SetKey(kTabs, ConvertURLsToBrowserAppTabValues(app->urls.value()));

  if (app->active_tab_index.has_value()) {
    app_data.SetKey(kActiveTabIndex,
                    base::Value(app->active_tab_index.value()));
  }

  if (app_type != kAppTypeBrowser)
    app_data.SetKey(kAppId, base::Value(app_id));

  app_data.SetKey(kWindowId, base::Value(window_id));

  if (app->display_id.has_value()) {
    app_data.SetKey(kDisplayId,
                    base::Value(base::NumberToString(app->display_id.value())));
  }

  if (app->pre_minimized_show_state_type.has_value()) {
    app_data.SetKey(kPreMinimizedWindowState,
                    base::Value(UiWindowStateToString(
                        app->pre_minimized_show_state_type.value())));
  }

  if (app->app_name.has_value())
    app_data.SetKey(kAppName, base::Value(app->app_name.value()));

  return app_data;
}

base::Value ConvertRestoreDataToValue(
    const app_restore::RestoreData* restore_data,
    apps::AppRegistryCache* apps_cache) {
  base::Value desk_data = base::Value(base::Value::Type::LIST);

  for (const auto& app : restore_data->app_id_to_launch_list()) {
    for (const auto& window : app.second) {
      desk_data.Append(ConvertWindowToDeskApp(app.first, window.first,
                                              window.second.get(), apps_cache));
    }
  }

  base::Value apps = base::Value(base::Value::Type::DICTIONARY);
  apps.SetKey(kApps, std::move(desk_data));
  return apps;
}

}  // namespace

namespace desks_storage {

namespace desk_template_conversion {

// Converts a time field from sync protobufs to a time object.
base::Time ProtoTimeToTime(int64_t proto_time) {
  return base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(proto_time));
}

// Converts a time object to the format used in sync protobufs
// (Microseconds since the Windows epoch).
int64_t TimeToProtoTime(const base::Time& t) {
  return t.ToDeltaSinceWindowsEpoch().InMicroseconds();
}

std::unique_ptr<ash::DeskTemplate> ParseDeskTemplateFromPolicy(
    const base::Value& policyJson) {
  if (!policyJson.is_dict())
    return nullptr;

  int version;
  std::string uuid;
  std::string name;
  std::string created_time_usec_str;
  std::string updated_time_usec_str;
  int64_t created_time_usec;
  int64_t updated_time_usec;
  const base::Value* desk = policyJson.FindDictKey(kDesk);
  if (!desk || !GetInt(policyJson, kVersion, &version) ||
      !GetString(policyJson, kUuid, &uuid) ||
      !GetString(policyJson, kName, &name) ||
      !GetString(policyJson, kCreatedTime, &created_time_usec_str) ||
      !base::StringToInt64(created_time_usec_str, &created_time_usec) ||
      !GetString(policyJson, kUpdatedTime, &updated_time_usec_str) ||
      !base::StringToInt64(updated_time_usec_str, &updated_time_usec) ||
      uuid.empty() || !base::GUID::ParseCaseInsensitive(uuid).is_valid() ||
      name.empty() || created_time_usec_str.empty() ||
      updated_time_usec_str.empty()) {
    return nullptr;
  }

  const base::Time created_time = ProtoTimeToTime(created_time_usec);
  const base::Time updated_time = ProtoTimeToTime(updated_time_usec);

  std::unique_ptr<ash::DeskTemplate> desk_template =
      std::make_unique<ash::DeskTemplate>(
          uuid, ash::DeskTemplateSource::kPolicy, name, created_time);

  desk_template->set_updated_time(updated_time);
  desk_template->set_desk_restore_data(ConvertJsonToRestoreData(desk));

  return desk_template;
}

base::Value SerializeDeskTemplateAsPolicy(const ash::DeskTemplate* desk,
                                          apps::AppRegistryCache* app_cache) {
  base::Value desk_dict(base::Value::Type::DICTIONARY);
  desk_dict.SetKey(kVersion, base::Value(kVersionNum));
  desk_dict.SetKey(kUuid, base::Value(desk->uuid().AsLowercaseString()));
  desk_dict.SetKey(kName, base::Value(desk->template_name()));
  desk_dict.SetKey(kCreatedTime, base::TimeToValue(desk->created_time()));
  desk_dict.SetKey(kUpdatedTime, base::TimeToValue(desk->GetLastUpdatedTime()));

  desk_dict.SetKey(
      kDesk, ConvertRestoreDataToValue(desk->desk_restore_data(), app_cache));

  return desk_dict;
}

}  // namespace desk_template_conversion

}  // namespace desks_storage
