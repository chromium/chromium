// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desks_storage/core/desk_template_conversion.h"

#include "ash/public/cpp/desk_template.h"
#include "base/guid.h"
#include "base/json/json_reader.h"
#include "base/json/values_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/app_constants/constants.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/restore_data.h"
#include "components/app_restore/window_info.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/sync/protocol/proto_enum_conversions.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/rect.h"

namespace {

using SyncWindowOpenDisposition =
    sync_pb::WorkspaceDeskSpecifics_WindowOpenDisposition;
using SyncLaunchContainer = sync_pb::WorkspaceDeskSpecifics_LaunchContainer;

// JSON value keys.
constexpr char kActiveTabIndex[] = "active_tab_index";
constexpr char kAppId[] = "app_id";
constexpr char kApps[] = "apps";
constexpr char kAppName[] = "app_name";
constexpr char kAppType[] = "app_type";
constexpr char kAppTypeBrowser[] = "BROWSER";
constexpr char kAppTypeChrome[] = "CHROME_APP";
constexpr char kAppTypeProgressiveWeb[] = "PWA";
constexpr char kAppTypeArc[] = "ARC";
constexpr char kBoundsInRoot[] = "bounds_in_root";
constexpr char kCreatedTime[] = "created_time_usec";
constexpr char kDesk[] = "desk";
constexpr char kDeskType[] = "desk_type";
constexpr char kDeskTypeTemplate[] = "TEMPLATE";
constexpr char kDeskTypeSaveAndRecall[] = "SAVE_AND_RECALL";
constexpr char kDisplayId[] = "display_id";
constexpr char kEventFlag[] = "event_flag";
constexpr char kLaunchContainer[] = "launch_container";
constexpr char kLaunchContainerWindow[] = "LAUNCH_CONTAINER_WINDOW";
constexpr char kLaunchContainerUnspecified[] = "LAUNCH_CONTAINER_UNSPECIFIED";
constexpr char kLaunchContainerPanelDeprecated[] = "LAUNCH_CONTAINER_PANEL";
constexpr char kLaunchContainerTab[] = "LAUNCH_CONTAINER_TAB";
constexpr char kLaunchContainerNone[] = "LAUNCH_CONTAINER_NONE";
constexpr char kMaximumSize[] = "maximum_size";
constexpr char kMinimumSize[] = "minimum_size";
constexpr char kName[] = "name";
constexpr char kPreMinimizedWindowState[] = "pre_minimized_window_state";
constexpr char kSizeHeight[] = "height";
constexpr char kSizeWidth[] = "width";
constexpr char kSnapPercentage[] = "snap_percent";
constexpr char kTabs[] = "tabs";
constexpr char kTabUrl[] = "url";
constexpr char kTitle[] = "title";
constexpr char kUpdatedTime[] = "updated_time_usec";
constexpr char kUuid[] = "uuid";
constexpr char kVersion[] = "version";
constexpr char kWindowId[] = "window_id";
constexpr char kWindowBound[] = "window_bound";
constexpr char kWindowBoundHeight[] = "height";
constexpr char kWindowBoundLeft[] = "left";
constexpr char kWindowBoundTop[] = "top";
constexpr char kWindowBoundWidth[] = "width";
constexpr char kWindowOpenDisposition[] = "window_open_disposition";
constexpr char kWindowOpenDispositionUnknown[] = "UNKOWN";
constexpr char kWindowOpenDispositionCurrentTab[] = "CURRENT_TAB";
constexpr char kWindowOpenDispositionSingletonTab[] = "SINGLETON_TAB";
constexpr char kWindowOpenDispositionNewForegroundTab[] = "NEW_FOREGROUND_TAB";
constexpr char kWindowOpenDispositionNewBackgroundTab[] = "NEW_BACKGROUND_TAB";
constexpr char kWindowOpenDispositionNewPopup[] = "NEW_POPUP";
constexpr char kWindowOpenDispositionNewWindow[] = "NEW_WINDOW";
constexpr char kWindowOpenDispositionSaveToDisk[] = "SAVE_TO_DISK";
constexpr char kWindowOpenDispositionOffTheRecord[] = "OFF_THE_RECORD";
constexpr char kWindowOpenDispositionIgnoreAction[] = "IGNORE_ACTION";
constexpr char kWindowOpenDispositionSwitchToTab[] = "SWITCH_TO_TAB";
constexpr char kWindowOpenDispositionNewPictureInPicture[] =
    "NEW_PICTURE_IN_PICTURE";
constexpr char kWindowState[] = "window_state";
constexpr char kWindowStateNormal[] = "NORMAL";
constexpr char kWindowStateMinimized[] = "MINIMIZED";
constexpr char kWindowStateMaximized[] = "MAXIMIZED";
constexpr char kWindowStateFullscreen[] = "FULLSCREEN";
constexpr char kWindowStatePrimarySnapped[] = "PRIMARY_SNAPPED";
constexpr char kWindowStateSecondarySnapped[] = "SECONDARY_SNAPPED";
constexpr char kZIndex[] = "z_index";

// Valid value sets.
const std::set<std::string> kValidDeskTypes = {kDeskTypeTemplate,
                                               kDeskTypeSaveAndRecall};

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
  } else if (app_type == kAppTypeChrome || app_type == kAppTypeProgressiveWeb ||
             app_type == kAppTypeArc) {
    // Read the provided app ID
    std::string app_id;
    if (GetString(app, kAppId, &app_id)) {
      return app_id;
    }
  }

  // Unsupported type
  return std::string();
}

// Returns true if launch container string value is valid.
bool IsValidLaunchContainer(const std::string& launch_container) {
  return launch_container == kLaunchContainerWindow ||
         launch_container == kLaunchContainerPanelDeprecated ||
         launch_container == kLaunchContainerTab ||
         launch_container == kLaunchContainerNone ||
         launch_container == kLaunchContainerUnspecified;
}

// Returns a casted apps::mojom::LaunchContainer to be set as an app restore
// data's container field.
int32_t StringToLaunchContainer(const std::string& launch_container) {
  if (launch_container == kLaunchContainerWindow) {
    return static_cast<int32_t>(
        apps::mojom::LaunchContainer::kLaunchContainerWindow);
  } else if (launch_container == kLaunchContainerPanelDeprecated) {
    return static_cast<int32_t>(
        apps::mojom::LaunchContainer::kLaunchContainerPanelDeprecated);
  } else if (launch_container == kLaunchContainerTab) {
    return static_cast<int32_t>(
        apps::mojom::LaunchContainer::kLaunchContainerTab);
  } else if (launch_container == kLaunchContainerNone) {
    return static_cast<int32_t>(
        apps::mojom::LaunchContainer::kLaunchContainerNone);
  } else if (launch_container == kLaunchContainerUnspecified) {
    return static_cast<int32_t>(
        apps::mojom::LaunchContainer::kLaunchContainerWindow);
    // Dcheck if our container isn't valid.  We should not reach here.
  } else {
    DCHECK(IsValidLaunchContainer(launch_container));
    return static_cast<int32_t>(
        apps::mojom::LaunchContainer::kLaunchContainerWindow);
  }
}

// Returns true if the disposition is a valid value.
bool IsValidWindowOpenDisposition(const std::string& disposition) {
  return disposition == kWindowOpenDispositionUnknown ||
         disposition == kWindowOpenDispositionCurrentTab ||
         disposition == kWindowOpenDispositionSingletonTab ||
         disposition == kWindowOpenDispositionNewForegroundTab ||
         disposition == kWindowOpenDispositionNewBackgroundTab ||
         disposition == kWindowOpenDispositionNewPopup ||
         disposition == kWindowOpenDispositionNewWindow ||
         disposition == kWindowOpenDispositionSaveToDisk ||
         disposition == kWindowOpenDispositionOffTheRecord ||
         disposition == kWindowOpenDispositionIgnoreAction ||
         disposition == kWindowOpenDispositionSwitchToTab ||
         disposition == kWindowOpenDispositionNewPictureInPicture;
}

// Returns a casted WindowOpenDisposition to be set in the app restore data.
int32_t StringToWindowOpenDisposition(const std::string& disposition) {
  if (disposition == kWindowOpenDispositionUnknown) {
    return static_cast<int32_t>(WindowOpenDisposition::UNKNOWN);
  } else if (disposition == kWindowOpenDispositionCurrentTab) {
    return static_cast<int32_t>(WindowOpenDisposition::CURRENT_TAB);
  } else if (disposition == kWindowOpenDispositionSingletonTab) {
    return static_cast<int32_t>(WindowOpenDisposition::SINGLETON_TAB);
  } else if (disposition == kWindowOpenDispositionNewForegroundTab) {
    return static_cast<int32_t>(WindowOpenDisposition::NEW_FOREGROUND_TAB);
  } else if (disposition == kWindowOpenDispositionNewBackgroundTab) {
    return static_cast<int32_t>(WindowOpenDisposition::NEW_BACKGROUND_TAB);
  } else if (disposition == kWindowOpenDispositionNewPopup) {
    return static_cast<int32_t>(WindowOpenDisposition::NEW_POPUP);
  } else if (disposition == kWindowOpenDispositionNewWindow) {
    return static_cast<int32_t>(WindowOpenDisposition::NEW_WINDOW);
  } else if (disposition == kWindowOpenDispositionSaveToDisk) {
    return static_cast<int32_t>(WindowOpenDisposition::SAVE_TO_DISK);
  } else if (disposition == kWindowOpenDispositionOffTheRecord) {
    return static_cast<int32_t>(WindowOpenDisposition::OFF_THE_RECORD);
  } else if (disposition == kWindowOpenDispositionIgnoreAction) {
    return static_cast<int32_t>(WindowOpenDisposition::IGNORE_ACTION);
  } else if (disposition == kWindowOpenDispositionNewPictureInPicture) {
    return static_cast<int32_t>(WindowOpenDisposition::NEW_PICTURE_IN_PICTURE);

    // Dcheck that the disposition is valid, we should never get here unless
    // the disposition is invalid.
  } else {
    DCHECK(IsValidWindowOpenDisposition(disposition));
    return static_cast<int32_t>(WindowOpenDisposition::UNKNOWN);
  }
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

  std::string launch_container;
  if (GetString(app, kLaunchContainer, &launch_container) &&
      IsValidLaunchContainer(launch_container)) {
    app_launch_info->container = StringToLaunchContainer(launch_container);
  }

  std::string disposition;
  if (GetString(app, kWindowOpenDisposition, &disposition) &&
      IsValidWindowOpenDisposition(disposition)) {
    app_launch_info->disposition = StringToWindowOpenDisposition(disposition);
  }

  std::string app_name;
  if (GetString(app, kAppName, &app_name))
    app_launch_info->app_name = app_name;

  // TODO(crbug.com/1311801): Add support for actual event_flag values.
  app_launch_info->event_flag = 0;

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

void FillArcExtraWindowInfoFromJson(
    const base::Value& app,
    app_restore::WindowInfo::ArcExtraInfo* out_window_info) {
  const base::Value* bounds_in_root = app.FindDictKey(kBoundsInRoot);
  int top;
  int left;
  int bounds_width;
  int bounds_height;
  if (bounds_in_root && GetInt(bounds_in_root, kWindowBoundTop, &top) &&
      GetInt(bounds_in_root, kWindowBoundLeft, &left) &&
      GetInt(bounds_in_root, kWindowBoundWidth, &bounds_width) &&
      GetInt(bounds_in_root, kWindowBoundHeight, &bounds_height))
    out_window_info->bounds_in_root.emplace(left, top, bounds_width,
                                            bounds_height);

  const base::Value* maximum_size = app.FindDictKey(kMaximumSize);
  int max_width;
  int max_height;
  if (maximum_size && GetInt(maximum_size, kSizeWidth, &max_width) &&
      GetInt(maximum_size, kSizeHeight, &max_height))
    out_window_info->maximum_size.emplace(max_width, max_height);

  const base::Value* minimum_size = app.FindDictKey(kMinimumSize);
  int min_width;
  int min_height;
  if (minimum_size && GetInt(minimum_size, kSizeWidth, &min_width) &&
      GetInt(minimum_size, kSizeHeight, &min_height)) {
    out_window_info->minimum_size.emplace(min_width, min_height);
  }
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

  std::string app_type;
  if (GetString(app, kAppType, &app_type) && app_type == kAppTypeArc) {
    FillArcExtraWindowInfoFromJson(app,
                                   &out_window_info->arc_extra_info.emplace());
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

  int snap_percentage;
  if (GetInt(app, kSnapPercentage, &snap_percentage))
    out_window_info->snap_percentage.emplace(snap_percentage);

  std::string title;
  if (GetString(app, kTitle, &title))
    out_window_info->app_title.emplace(base::UTF8ToUTF16(title));
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

base::Value ConvertSizeToValue(const gfx::Size& size) {
  base::Value size_value(base::Value::Type::DICTIONARY);

  size_value.SetKey(kSizeWidth, base::Value(size.width()));
  size_value.SetKey(kSizeHeight, base::Value(size.height()));

  return size_value;
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

// Returns a string WindowOpenDisposition when given a value of the
// WindowOpenDisposition passed into this function.  Assumes the caller
// casts the disposition from a int32_t.
std::string WindowOpenDispositionToString(WindowOpenDisposition disposition) {
  switch (disposition) {
    case WindowOpenDisposition::UNKNOWN:
      return kWindowOpenDispositionUnknown;
    case WindowOpenDisposition::CURRENT_TAB:
      return kWindowOpenDispositionCurrentTab;
    case WindowOpenDisposition::SINGLETON_TAB:
      return kWindowOpenDispositionSingletonTab;
    case WindowOpenDisposition::NEW_FOREGROUND_TAB:
      return kWindowOpenDispositionNewForegroundTab;
    case WindowOpenDisposition::NEW_BACKGROUND_TAB:
      return kWindowOpenDispositionNewBackgroundTab;
    case WindowOpenDisposition::NEW_POPUP:
      return kWindowOpenDispositionNewPopup;
    case WindowOpenDisposition::NEW_WINDOW:
      return kWindowOpenDispositionNewWindow;
    case WindowOpenDisposition::SAVE_TO_DISK:
      return kWindowOpenDispositionSaveToDisk;
    case WindowOpenDisposition::SWITCH_TO_TAB:
      return kWindowOpenDispositionSwitchToTab;
    case WindowOpenDisposition::OFF_THE_RECORD:
      return kWindowOpenDispositionOffTheRecord;
    case WindowOpenDisposition::IGNORE_ACTION:
      return kWindowOpenDispositionIgnoreAction;
    case WindowOpenDisposition::NEW_PICTURE_IN_PICTURE:
      return kWindowOpenDispositionNewPictureInPicture;
  }
}

std::string LaunchContainerToString(
    apps::mojom::LaunchContainer launch_container) {
  switch (launch_container) {
    case apps::mojom::LaunchContainer::kLaunchContainerWindow:
      return kLaunchContainerWindow;
    case apps::mojom::LaunchContainer::kLaunchContainerPanelDeprecated:
      return kLaunchContainerPanelDeprecated;
    case apps::mojom::LaunchContainer::kLaunchContainerTab:
      return kLaunchContainerTab;
    case apps::mojom::LaunchContainer::kLaunchContainerNone:
      return kLaunchContainerNone;
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
  const auto app_type = app_id == app_constants::kChromeAppId
                            ? apps::AppType::kWeb
                            : apps_cache->GetAppType(app_id);

  switch (app_type) {
    case apps::AppType::kWeb:
      return app_id == app_constants::kChromeAppId ? kAppTypeBrowser
                                                   : kAppTypeProgressiveWeb;
    case apps::AppType::kChromeApp:
      return kAppTypeChrome;
    case apps::AppType::kArc:
      return kAppTypeArc;
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

  if (app->bounds_in_root.has_value()) {
    app_data.SetKey(kBoundsInRoot,
                    ConvertWindowBoundToValue(app->bounds_in_root.value()));
  }

  if (app->minimum_size.has_value()) {
    app_data.SetKey(kMinimumSize,
                    ConvertSizeToValue(app->minimum_size.value()));
  }

  if (app->maximum_size.has_value()) {
    app_data.SetKey(kMaximumSize,
                    ConvertSizeToValue(app->maximum_size.value()));
  }

  if (app->title.has_value()) {
    app_data.SetKey(kTitle, base::Value(base::UTF16ToUTF8(app->title.value())));
  }

  if (app->window_state_type.has_value()) {
    app_data.SetKey(kWindowState, base::Value(ChromeOsWindowStateToString(
                                      app->window_state_type.value())));
  }

  // TODO(crbug.com/1311801): Add support for actual event_flag values.
  app_data.SetKey(kEventFlag, base::Value(0));

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

  if (app->snap_percentage.has_value()) {
    app_data.SetKey(
        kSnapPercentage,
        base::Value(static_cast<int>(app->snap_percentage.value())));
  }

  if (app->app_name.has_value())
    app_data.SetKey(kAppName, base::Value(app->app_name.value()));

  if (app->disposition.has_value()) {
    WindowOpenDisposition disposition =
        static_cast<WindowOpenDisposition>(app->disposition.value());
    app_data.SetKey(kWindowOpenDisposition,
                    base::Value(WindowOpenDispositionToString(disposition)));
  }

  if (app->container.has_value()) {
    apps::mojom::LaunchContainer container =
        static_cast<apps::mojom::LaunchContainer>(app->container.value());
    app_data.SetKey(kLaunchContainer,
                    base::Value(LaunchContainerToString(container)));
  }

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

std::string SerializeDeskTypeAsString(ash::DeskTemplateType desk_type) {
  switch (desk_type) {
    case ash::DeskTemplateType::kTemplate:
      return kDeskTypeTemplate;
    case ash::DeskTemplateType::kSaveAndRecall:
      return kDeskTypeSaveAndRecall;
  }
}

bool IsValidDeskTemplateType(const std::string& desk_template_type) {
  return base::Contains(kValidDeskTypes, desk_template_type);
}

ash::DeskTemplateType GetDeskTypeFromString(const std::string& desk_type) {
  DCHECK(IsValidDeskTemplateType(desk_type));
  return desk_type == kDeskTypeTemplate ? ash::DeskTemplateType::kTemplate
                                        : ash::DeskTemplateType::kSaveAndRecall;
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
    const base::Value& policy_json) {
  if (!policy_json.is_dict())
    return nullptr;

  int version;
  std::string uuid;
  std::string name;
  std::string created_time_usec_str;
  std::string updated_time_usec_str;
  int64_t created_time_usec;
  int64_t updated_time_usec;
  const base::Value* desk = policy_json.FindDictKey(kDesk);
  if (!desk || !GetInt(policy_json, kVersion, &version) ||
      !GetString(policy_json, kUuid, &uuid) ||
      !GetString(policy_json, kName, &name) ||
      !GetString(policy_json, kCreatedTime, &created_time_usec_str) ||
      !base::StringToInt64(created_time_usec_str, &created_time_usec) ||
      !GetString(policy_json, kUpdatedTime, &updated_time_usec_str) ||
      !base::StringToInt64(updated_time_usec_str, &updated_time_usec) ||
      uuid.empty() || !base::GUID::ParseCaseInsensitive(uuid).is_valid() ||
      name.empty() || created_time_usec_str.empty() ||
      updated_time_usec_str.empty())
    return nullptr;

  // Set default value for the desk type to template.
  std::string desk_type_string;
  if (!GetString(policy_json, kDeskType, &desk_type_string)) {
    desk_type_string = kDeskTypeTemplate;
  } else if (!IsValidDeskTemplateType(desk_type_string)) {
    return nullptr;
  }

  const base::Time created_time = ProtoTimeToTime(created_time_usec);
  const base::Time updated_time = ProtoTimeToTime(updated_time_usec);

  std::unique_ptr<ash::DeskTemplate> desk_template =
      std::make_unique<ash::DeskTemplate>(
          uuid, ash::DeskTemplateSource::kPolicy, name, created_time);

  desk_template->set_updated_time(updated_time);
  desk_template->set_desk_restore_data(ConvertJsonToRestoreData(desk));
  desk_template->set_type(GetDeskTypeFromString(desk_type_string));

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
  desk_dict.SetKey(kDeskType,
                   base::Value(SerializeDeskTypeAsString(desk->type())));

  desk_dict.SetKey(
      kDesk, ConvertRestoreDataToValue(desk->desk_restore_data(), app_cache));

  return desk_dict;
}

WindowOpenDisposition ToBaseWindowOpenDisposition(
    SyncWindowOpenDisposition disposition) {
  switch (disposition) {
    case sync_pb::WorkspaceDeskSpecifics_WindowOpenDisposition_UNKNOWN:
      return WindowOpenDisposition::UNKNOWN;
    case sync_pb::WorkspaceDeskSpecifics_WindowOpenDisposition_CURRENT_TAB:
      return WindowOpenDisposition::CURRENT_TAB;
    case sync_pb::WorkspaceDeskSpecifics_WindowOpenDisposition_SINGLETON_TAB:
      return WindowOpenDisposition::SINGLETON_TAB;
    case sync_pb::
        WorkspaceDeskSpecifics_WindowOpenDisposition_NEW_FOREGROUND_TAB:
      return WindowOpenDisposition::NEW_FOREGROUND_TAB;
    case sync_pb::
        WorkspaceDeskSpecifics_WindowOpenDisposition_NEW_BACKGROUND_TAB:
      return WindowOpenDisposition::NEW_BACKGROUND_TAB;
    case sync_pb::WorkspaceDeskSpecifics_WindowOpenDisposition_NEW_POPUP:
      return WindowOpenDisposition::NEW_POPUP;
    case sync_pb::WorkspaceDeskSpecifics_WindowOpenDisposition_NEW_WINDOW:
      return WindowOpenDisposition::NEW_WINDOW;
    case sync_pb::WorkspaceDeskSpecifics_WindowOpenDisposition_SAVE_TO_DISK:
      return WindowOpenDisposition::SAVE_TO_DISK;
    case sync_pb::WorkspaceDeskSpecifics_WindowOpenDisposition_OFF_THE_RECORD:
      return WindowOpenDisposition::OFF_THE_RECORD;
    case sync_pb::WorkspaceDeskSpecifics_WindowOpenDisposition_IGNORE_ACTION:
      return WindowOpenDisposition::IGNORE_ACTION;
    case sync_pb::WorkspaceDeskSpecifics_WindowOpenDisposition_SWITCH_TO_TAB:
      return WindowOpenDisposition::SWITCH_TO_TAB;
    case sync_pb::
        WorkspaceDeskSpecifics_WindowOpenDisposition_NEW_PICTURE_IN_PICTURE:
      return WindowOpenDisposition::NEW_PICTURE_IN_PICTURE;
  }
}

SyncWindowOpenDisposition FromBaseWindowOpenDisposition(
    WindowOpenDisposition disposition) {
  switch (disposition) {
    case WindowOpenDisposition::UNKNOWN:
      return sync_pb::WorkspaceDeskSpecifics_WindowOpenDisposition_UNKNOWN;
    case WindowOpenDisposition::CURRENT_TAB:
      return sync_pb::WorkspaceDeskSpecifics_WindowOpenDisposition_CURRENT_TAB;
    case WindowOpenDisposition::SINGLETON_TAB:
      return sync_pb::
          WorkspaceDeskSpecifics_WindowOpenDisposition_SINGLETON_TAB;
    case WindowOpenDisposition::NEW_FOREGROUND_TAB:
      return sync_pb::
          WorkspaceDeskSpecifics_WindowOpenDisposition_NEW_FOREGROUND_TAB;
    case WindowOpenDisposition::NEW_BACKGROUND_TAB:
      return sync_pb::
          WorkspaceDeskSpecifics_WindowOpenDisposition_NEW_BACKGROUND_TAB;
    case WindowOpenDisposition::NEW_POPUP:
      return sync_pb::WorkspaceDeskSpecifics_WindowOpenDisposition_NEW_POPUP;
    case WindowOpenDisposition::NEW_WINDOW:
      return sync_pb::WorkspaceDeskSpecifics_WindowOpenDisposition_NEW_WINDOW;
    case WindowOpenDisposition::SAVE_TO_DISK:
      return sync_pb::WorkspaceDeskSpecifics_WindowOpenDisposition_SAVE_TO_DISK;
    case WindowOpenDisposition::OFF_THE_RECORD:
      return sync_pb::
          WorkspaceDeskSpecifics_WindowOpenDisposition_OFF_THE_RECORD;
    case WindowOpenDisposition::IGNORE_ACTION:
      return sync_pb::
          WorkspaceDeskSpecifics_WindowOpenDisposition_IGNORE_ACTION;
    case WindowOpenDisposition::SWITCH_TO_TAB:
      return sync_pb::
          WorkspaceDeskSpecifics_WindowOpenDisposition_SWITCH_TO_TAB;
    case WindowOpenDisposition::NEW_PICTURE_IN_PICTURE:
      return sync_pb::
          WorkspaceDeskSpecifics_WindowOpenDisposition_NEW_PICTURE_IN_PICTURE;
  }
}

SyncLaunchContainer FromMojomLaunchContainer(
    apps::mojom::LaunchContainer container) {
  switch (container) {
    case apps::mojom::LaunchContainer::kLaunchContainerWindow:
      return sync_pb::
          WorkspaceDeskSpecifics_LaunchContainer_LAUNCH_CONTAINER_WINDOW;
    case apps::mojom::LaunchContainer::kLaunchContainerPanelDeprecated:
      return sync_pb::
          WorkspaceDeskSpecifics_LaunchContainer_LAUNCH_CONTAINER_PANEL_DEPRECATED;
    case apps::mojom::LaunchContainer::kLaunchContainerTab:
      return sync_pb::
          WorkspaceDeskSpecifics_LaunchContainer_LAUNCH_CONTAINER_TAB;
    case apps::mojom::LaunchContainer::kLaunchContainerNone:
      return sync_pb::
          WorkspaceDeskSpecifics_LaunchContainer_LAUNCH_CONTAINER_NONE;
  }
}

apps::mojom::LaunchContainer ToMojomLaunchContainer(
    SyncLaunchContainer container) {
  switch (container) {
    case sync_pb::
        WorkspaceDeskSpecifics_LaunchContainer_LAUNCH_CONTAINER_UNSPECIFIED:
      return apps::mojom::LaunchContainer::kLaunchContainerWindow;
    case sync_pb::
        WorkspaceDeskSpecifics_LaunchContainer_LAUNCH_CONTAINER_WINDOW:
      return apps::mojom::LaunchContainer::kLaunchContainerWindow;
    case sync_pb::
        WorkspaceDeskSpecifics_LaunchContainer_LAUNCH_CONTAINER_PANEL_DEPRECATED:
      return apps::mojom::LaunchContainer::kLaunchContainerPanelDeprecated;
    case sync_pb::WorkspaceDeskSpecifics_LaunchContainer_LAUNCH_CONTAINER_TAB:
      return apps::mojom::LaunchContainer::kLaunchContainerTab;
    case sync_pb::WorkspaceDeskSpecifics_LaunchContainer_LAUNCH_CONTAINER_NONE:
      return apps::mojom::LaunchContainer::kLaunchContainerNone;
  }
}

}  // namespace desk_template_conversion

}  // namespace desks_storage
