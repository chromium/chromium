// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desks_storage/core/desk_template_conversion.h"

#include "base/containers/fixed_flat_set.h"
#include "base/guid.h"
#include "base/json/json_reader.h"
#include "base/json/values_util.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/app_constants/constants.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/restore_data.h"
#include "components/app_restore/tab_group_info.h"
#include "components/app_restore/window_info.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/sync/protocol/proto_enum_conversions.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/rect.h"

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/cpp/lacros_startup_state.h"  // nogncheck
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

namespace {

using SyncWindowOpenDisposition =
    sync_pb::WorkspaceDeskSpecifics_WindowOpenDisposition;
using SyncLaunchContainer = sync_pb::WorkspaceDeskSpecifics_LaunchContainer;
using GroupColor = tab_groups::TabGroupColorId;

// JSON value keys.
constexpr char kActiveTabIndex[] = "active_tab_index";
constexpr char kAppId[] = "app_id";
constexpr char kApps[] = "apps";
constexpr char kAppName[] = "app_name";
constexpr char kAppType[] = "app_type";
constexpr char kAppTypeArc[] = "ARC";
constexpr char kAppTypeBrowser[] = "BROWSER";
constexpr char kAppTypeChrome[] = "CHROME_APP";
constexpr char kAppTypeProgressiveWeb[] = "PWA";
constexpr char kAppTypeUnsupported[] = "UNSUPPORTED";
constexpr char kBoundsInRoot[] = "bounds_in_root";
constexpr char kCreatedTime[] = "created_time_usec";
constexpr char kDesk[] = "desk";
constexpr char kDeskType[] = "desk_type";
constexpr char kDeskTypeTemplate[] = "TEMPLATE";
constexpr char kDeskTypeSaveAndRecall[] = "SAVE_AND_RECALL";
constexpr char kDeskTypeUnknown[] = "UNKNOWN";
constexpr char kDisplayId[] = "display_id";
constexpr char kEventFlag[] = "event_flag";
constexpr char kFirstNonPinnedTabIndex[] = "first_non_pinned_tab_index";
constexpr char kIsAppTypeBrowser[] = "is_app";
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
constexpr char kTabRangeFirstIndex[] = "first_index";
constexpr char kTabRangeLastIndex[] = "last_index";
constexpr char kSizeHeight[] = "height";
constexpr char kSizeWidth[] = "width";
constexpr char kSnapPercentage[] = "snap_percent";
constexpr char kTabs[] = "tabs";
constexpr char kTabGroups[] = "tab_groups";
constexpr char kTabUrl[] = "url";
constexpr char kTitle[] = "title";
constexpr char kUpdatedTime[] = "updated_time_usec";
constexpr char kUuid[] = "uuid";
constexpr char kVersion[] = "version";
constexpr char kTabGroupTitleKey[] = "title";
constexpr char kTabGroupColorKey[] = "color";
constexpr char kTabGroupIsCollapsed[] = "is_collapsed";
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
constexpr auto kValidDeskTypes = base::MakeFixedFlatSet<base::StringPiece>(
    {kDeskTypeTemplate, kDeskTypeSaveAndRecall});
constexpr auto kValidLaunchContainers =
    base::MakeFixedFlatSet<base::StringPiece>(
        {kLaunchContainerWindow, kLaunchContainerPanelDeprecated,
         kLaunchContainerTab, kLaunchContainerNone,
         kLaunchContainerUnspecified});
constexpr auto kValidWindowOpenDispositions =
    base::MakeFixedFlatSet<base::StringPiece>(
        {kWindowOpenDispositionUnknown, kWindowOpenDispositionCurrentTab,
         kWindowOpenDispositionSingletonTab,
         kWindowOpenDispositionNewForegroundTab,
         kWindowOpenDispositionNewBackgroundTab, kWindowOpenDispositionNewPopup,
         kWindowOpenDispositionNewWindow, kWindowOpenDispositionSaveToDisk,
         kWindowOpenDispositionOffTheRecord, kWindowOpenDispositionIgnoreAction,
         kWindowOpenDispositionSwitchToTab,
         kWindowOpenDispositionNewPictureInPicture});
constexpr auto kValidWindowStates = base::MakeFixedFlatSet<base::StringPiece>(
    {kWindowStateNormal, kWindowStateMinimized, kWindowStateMaximized,
     kWindowStateFullscreen, kWindowStatePrimarySnapped,
     kWindowStateSecondarySnapped, kZIndex});
constexpr auto kValidTabGroupColors = base::MakeFixedFlatSet<base::StringPiece>(
    {app_restore::kTabGroupColorUnknown, app_restore::kTabGroupColorGrey,
     app_restore::kTabGroupColorBlue, app_restore::kTabGroupColorRed,
     app_restore::kTabGroupColorYellow, app_restore::kTabGroupColorGreen,
     app_restore::kTabGroupColorPink, app_restore::kTabGroupColorPurple,
     app_restore::kTabGroupColorCyan, app_restore::kTabGroupColorOrange});

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

bool GetBool(const base::Value* dict, const char* key, bool* out) {
  const base::Value* value =
      dict->FindKeyOfType(key, base::Value::Type::BOOLEAN);
  if (!value)
    return false;

  *out = value->GetBool();
  return true;
}

bool GetBool(const base::Value& dict, const char* key, bool* out) {
  return GetBool(&dict, key, out);
}

// Get App ID from App proto.
std::string GetJsonAppId(const base::Value& app) {
  std::string app_type;
  if (!GetString(app, kAppType, &app_type))
    return std::string();  // App Type must be specified.

  if (app_type == kAppTypeBrowser) {
    // Return the primary browser's known app ID.
    const bool is_lacros =
#if BUILDFLAG(IS_CHROMEOS_LACROS)
        true;
#else
        // Note that this will launch the browser as lacros if it is enabled,
        // even if it was saved as a non-lacros window (and vice-versa).
        crosapi::lacros_startup_state::IsLacrosEnabled() &&
        crosapi::lacros_startup_state::IsLacrosPrimaryEnabled();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

    // Browser app has a known app ID.
    return std::string(is_lacros ? app_constants::kLacrosAppId
                                 : app_constants::kChromeAppId);
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

// Convert a TabGroupInfo object to a base::Value dictionary.
base::Value ConvertTabGroupInfoToValue(
    const app_restore::TabGroupInfo& group_info) {
  base::Value tab_group_dict(base::Value::Type::DICTIONARY);

  tab_group_dict.SetIntKey(kTabRangeFirstIndex, group_info.tab_range.start());
  tab_group_dict.SetIntKey(kTabRangeLastIndex, group_info.tab_range.end());
  tab_group_dict.SetStringKey(
      kTabGroupTitleKey, base::UTF16ToUTF8(group_info.visual_data.title()));
  tab_group_dict.SetStringKey(
      kTabGroupColorKey,
      app_restore::TabGroupColorToString(group_info.visual_data.color()));
  tab_group_dict.SetBoolKey(kTabGroupIsCollapsed,
                            group_info.visual_data.is_collapsed());

  return tab_group_dict;
}

bool IsValidGroupColor(const std::string& group_color) {
  return base::Contains(kValidTabGroupColors, group_color);
}

GroupColor ConvertGroupColorStringToGroupColor(const std::string& group_color) {
  if (group_color == app_restore::kTabGroupColorGrey) {
    return GroupColor::kGrey;
  } else if (group_color == app_restore::kTabGroupColorBlue) {
    return GroupColor::kBlue;
  } else if (group_color == app_restore::kTabGroupColorRed) {
    return GroupColor::kRed;
  } else if (group_color == app_restore::kTabGroupColorYellow) {
    return GroupColor::kYellow;
  } else if (group_color == app_restore::kTabGroupColorGreen) {
    return GroupColor::kGreen;
  } else if (group_color == app_restore::kTabGroupColorPink) {
    return GroupColor::kPink;
  } else if (group_color == app_restore::kTabGroupColorPurple) {
    return GroupColor::kPurple;
  } else if (group_color == app_restore::kTabGroupColorCyan) {
    return GroupColor::kCyan;
  } else if (group_color == app_restore::kTabGroupColorOrange) {
    return GroupColor::kOrange;
    // There is no UNKNOWN equivalent in GroupColor, simply default
    // to grey.
  } else if (group_color == app_restore::kTabGroupColorUnknown) {
    return GroupColor::kGrey;
  } else {
    NOTREACHED();
    return GroupColor::kGrey;
  }
}

// Constructs a GroupVisualData from value `group_visual_data` IFF all fields
// are present and valid in the value parameter.  Returns true on success, false
// on failure.
bool MakeTabGroupVisualDataFromValue(
    const base::Value& tab_group,
    tab_groups::TabGroupVisualData* out_visual_data) {
  std::string tab_group_title;
  std::string group_color_string;
  bool is_collapsed;
  if (GetString(tab_group, kTabGroupTitleKey, &tab_group_title) &&
      GetBool(tab_group, kTabGroupIsCollapsed, &is_collapsed) &&
      GetString(tab_group, kTabGroupColorKey, &group_color_string) &&
      IsValidGroupColor(group_color_string)) {
    *out_visual_data = tab_groups::TabGroupVisualData(
        base::UTF8ToUTF16(tab_group_title),
        ConvertGroupColorStringToGroupColor(group_color_string), is_collapsed);
    return true;
  }

  return false;
}

// Constructs a gfx::Range from value `group_range` IFF all fields are
// present and valid in the value parameter.  Returns true on success, false on
// failure.
bool MakeTabGroupRangeFromValue(const base::Value& tab_group,
                                gfx::Range* out_range) {
  int32_t range_start;
  int32_t range_end;
  if (GetInt(tab_group, kTabRangeFirstIndex, &range_start) &&
      GetInt(tab_group, kTabRangeLastIndex, &range_end)) {
    *out_range = gfx::Range(range_start, range_end);
    return true;
  }

  return false;
}

// Constructs a TabGroupInfo from `tab_group` IFF all fields are present
// and valid in the value parameter. Returns true on success, false on failure.
absl::optional<app_restore::TabGroupInfo> MakeTabGroupInfoFromDict(
    const base::Value& tab_group) {
  absl::optional<app_restore::TabGroupInfo> tab_group_info = absl::nullopt;

  tab_groups::TabGroupVisualData visual_data;
  gfx::Range range;
  if (MakeTabGroupRangeFromValue(tab_group, &range) &&
      MakeTabGroupVisualDataFromValue(tab_group, &visual_data)) {
    tab_group_info.emplace(range, visual_data);
  }

  return tab_group_info;
}

// Returns true if launch container string value is valid.
bool IsValidLaunchContainer(const std::string& launch_container) {
  return base::Contains(kValidLaunchContainers, launch_container);
}

// Returns a casted apps::LaunchContainer to be set as an app restore data's
// container field.
int32_t StringToLaunchContainer(const std::string& launch_container) {
  if (launch_container == kLaunchContainerWindow) {
    return static_cast<int32_t>(apps::LaunchContainer::kLaunchContainerWindow);
  } else if (launch_container == kLaunchContainerPanelDeprecated) {
    return static_cast<int32_t>(
        apps::LaunchContainer::kLaunchContainerPanelDeprecated);
  } else if (launch_container == kLaunchContainerTab) {
    return static_cast<int32_t>(apps::LaunchContainer::kLaunchContainerTab);
  } else if (launch_container == kLaunchContainerNone) {
    return static_cast<int32_t>(apps::LaunchContainer::kLaunchContainerNone);
  } else if (launch_container == kLaunchContainerUnspecified) {
    return static_cast<int32_t>(apps::LaunchContainer::kLaunchContainerWindow);
    // Dcheck if our container isn't valid.  We should not reach here.
  } else {
    DCHECK(IsValidLaunchContainer(launch_container));
    return static_cast<int32_t>(apps::LaunchContainer::kLaunchContainerWindow);
  }
}

// Returns true if the disposition is a valid value.
bool IsValidWindowOpenDisposition(const std::string& disposition) {
  return base::Contains(kValidWindowOpenDispositions, disposition);
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

// Convert App JSON to `app_restore::AppLaunchInfo`.
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
    // This should never happen. `APP_NOT_SET` corresponds to empty `app_id`.
    // This method will early return when `app_id` is empty.
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

  bool app_type_browser;
  if (GetBool(app, kIsAppTypeBrowser, &app_type_browser))
    app_launch_info->app_type_browser = app_type_browser;

  if (app_type == kAppTypeBrowser) {
    int active_tab_index;
    if (GetInt(app, kActiveTabIndex, &active_tab_index))
      app_launch_info->active_tab_index = active_tab_index;

    int first_non_pinned_tab_index;
    if (GetInt(app, kFirstNonPinnedTabIndex, &first_non_pinned_tab_index))
      app_launch_info->first_non_pinned_tab_index = first_non_pinned_tab_index;

    // Fill in the URL list
    app_launch_info->urls.emplace();
    const base::Value* tabs = app.FindKeyOfType(kTabs, base::Value::Type::LIST);
    if (tabs) {
      for (auto& tab : tabs->GetList()) {
        std::string url;
        if (GetString(tab, kTabUrl, &url)) {
          app_launch_info->urls.value().emplace_back(url);
        }
      }
    }

    // Fill the tab groups
    app_launch_info->tab_group_infos.emplace();
    const base::Value* tab_groups =
        app.FindKeyOfType(kTabGroups, base::Value::Type::LIST);
    if (tab_groups) {
      for (auto& tab : tab_groups->GetList()) {
        absl::optional<app_restore::TabGroupInfo> tab_group =
            MakeTabGroupInfoFromDict(tab);
        if (tab_group.has_value()) {
          app_launch_info->tab_group_infos->push_back(
              std::move(tab_group.value()));
        }
      }
    }
  }
  // For Chrome apps and PWAs, the `app_id` is sufficient for identification.

  return app_launch_info;
}

bool IsValidWindowState(const std::string& window_state) {
  return base::Contains(kValidWindowStates, window_state);
}

// Convert JSON string WindowState `state` to ui::WindowShowState used by
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

// Convert JSON string WindowState `state` to chromeos::WindowStateType used by
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

// Fill `out_window_info` with information from JSON `app`.
void FillWindowInfoFromJson(const base::Value& app,
                            app_restore::WindowInfo* out_window_info) {
  std::string window_state;
  chromeos::WindowStateType cros_window_state =
      chromeos::WindowStateType::kDefault;
  if (GetString(app, kWindowState, &window_state) &&
      IsValidWindowState(window_state)) {
    cros_window_state = ToChromeOsWindowState(window_state);
    out_window_info->window_state_type.emplace(cros_window_state);
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
      IsValidWindowState(pre_minimized_window_state) &&
      cros_window_state == chromeos::WindowStateType::kMinimized) {
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

// Convert a desk template to `app_restore::RestoreData`.
std::unique_ptr<app_restore::RestoreData> ConvertJsonToRestoreData(
    const base::Value* desk) {
  std::unique_ptr<app_restore::RestoreData> restore_data =
      std::make_unique<app_restore::RestoreData>();

  const base::Value* apps = desk->FindListKey(kApps);
  if (apps) {
    for (const auto& app : apps->GetList()) {
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

// Convert ui::WindowStateType `window_state` to std::string used by the
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

// Convert ui::WindowShowState `state` to JSON used by the base::Value
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

std::string LaunchContainerToString(apps::LaunchContainer launch_container) {
  switch (launch_container) {
    case apps::LaunchContainer::kLaunchContainerWindow:
      return kLaunchContainerWindow;
    case apps::LaunchContainer::kLaunchContainerPanelDeprecated:
      return kLaunchContainerPanelDeprecated;
    case apps::LaunchContainer::kLaunchContainerTab:
      return kLaunchContainerTab;
    case apps::LaunchContainer::kLaunchContainerNone:
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
  // This switch should follow the same structure as DeskSyncBridge#FillApp.
  switch (apps_cache->GetAppType(app_id)) {
    case apps::AppType::kWeb:
    case apps::AppType::kSystemWeb:
      return kAppTypeChrome;

    case apps::AppType::kChromeApp:
      if (app_id == app_constants::kChromeAppId) {
        return kAppTypeBrowser;
      } else {
        return kAppTypeChrome;
      }

    case apps::AppType::kStandaloneBrowser:
      if (app_id == app_constants::kLacrosAppId) {
        return kAppTypeBrowser;
      } else {
        return kAppTypeUnsupported;
      }

    case apps::AppType::kArc:
      return kAppTypeArc;

    case apps::AppType::kStandaloneBrowserChromeApp:
      return kAppTypeChrome;

    case apps::AppType::kBuiltIn:
    case apps::AppType::kCrostini:
    case apps::AppType::kPluginVm:
    case apps::AppType::kUnknown:
    case apps::AppType::kMacOs:
    case apps::AppType::kRemote:
    case apps::AppType::kBorealis:
    case apps::AppType::kExtension:
    case apps::AppType::kStandaloneBrowserExtension:
      // Default to unsupported. This app should not be captured.
      return kAppTypeUnsupported;
  }
}

base::Value ConvertWindowToDeskApp(const std::string& app_id,
                                   const int window_id,
                                   const app_restore::AppRestoreData* app,
                                   apps::AppRegistryCache* apps_cache) {
  std::string app_type = GetAppTypeForJson(apps_cache, app_id);

  if (kAppTypeUnsupported == app_type) {
    return base::Value(base::Value::Type::NONE);
  }

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

  chromeos::WindowStateType window_state = chromeos::WindowStateType::kDefault;
  if (app->window_state_type.has_value()) {
    window_state = app->window_state_type.value();
    app_data.SetKey(kWindowState,
                    base::Value(ChromeOsWindowStateToString(window_state)));
  }

  // TODO(crbug.com/1311801): Add support for actual event_flag values.
  app_data.SetKey(kEventFlag, base::Value(0));

  if (app->activation_index.has_value())
    app_data.SetKey(kZIndex, base::Value(app->activation_index.value()));

  app_data.SetKey(kAppType, base::Value(app_type));

  if (app->urls.has_value())
    app_data.SetKey(kTabs, ConvertURLsToBrowserAppTabValues(app->urls.value()));

  if (app->tab_group_infos.has_value()) {
    base::Value tab_groups_value(base::Value::Type::LIST);

    for (const auto& tab_group : app->tab_group_infos.value()) {
      tab_groups_value.Append(ConvertTabGroupInfoToValue(tab_group));
    }

    app_data.SetKey(kTabGroups, std::move(tab_groups_value));
  }

  if (app->active_tab_index.has_value()) {
    app_data.SetKey(kActiveTabIndex,
                    base::Value(app->active_tab_index.value()));
  }

  if (app->first_non_pinned_tab_index.has_value()) {
    app_data.SetKey(kFirstNonPinnedTabIndex,
                    base::Value(app->first_non_pinned_tab_index.value()));
  }

  if (app->app_type_browser.has_value()) {
    app_data.SetKey(kIsAppTypeBrowser,
                    base::Value(app->app_type_browser.value()));
  }

  if (app_type != kAppTypeBrowser)
    app_data.SetKey(kAppId, base::Value(app_id));

  app_data.SetKey(kWindowId, base::Value(window_id));

  if (app->display_id.has_value()) {
    app_data.SetKey(kDisplayId,
                    base::Value(base::NumberToString(app->display_id.value())));
  }

  if (app->pre_minimized_show_state_type.has_value() &&
      window_state == chromeos::WindowStateType::kMinimized) {
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
    apps::LaunchContainer container =
        static_cast<apps::LaunchContainer>(app->container.value());
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
      auto app_data = ConvertWindowToDeskApp(app.first, window.first,
                                             window.second.get(), apps_cache);
      if (app_data.is_none())
        continue;

      desk_data.Append(std::move(app_data));
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
    case ash::DeskTemplateType::kUnknown:
      NOTREACHED();
      return kDeskTypeUnknown;
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

// Converts the TabGroupColorId passed into its string equivalent
// as defined in the k constants above.
std::string ConvertTabGroupColorIdToString(GroupColor color) {
  switch (color) {
    case GroupColor::kGrey:
      return app_restore::kTabGroupColorGrey;
    case GroupColor::kBlue:
      return app_restore::kTabGroupColorBlue;
    case GroupColor::kRed:
      return app_restore::kTabGroupColorRed;
    case GroupColor::kYellow:
      return app_restore::kTabGroupColorYellow;
    case GroupColor::kGreen:
      return app_restore::kTabGroupColorGreen;
    case GroupColor::kPink:
      return app_restore::kTabGroupColorPink;
    case GroupColor::kPurple:
      return app_restore::kTabGroupColorPurple;
    case GroupColor::kCyan:
      return app_restore::kTabGroupColorCyan;
    case GroupColor::kOrange:
      return app_restore::kTabGroupColorOrange;
  }
}

// Converts a time field from sync protobufs to a time object.
base::Time ProtoTimeToTime(int64_t proto_time) {
  return base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(proto_time));
}

// Converts a time object to the format used in sync protobufs
// (Microseconds since the Windows epoch).
int64_t TimeToProtoTime(const base::Time& t) {
  return t.ToDeltaSinceWindowsEpoch().InMicroseconds();
}

std::unique_ptr<ash::DeskTemplate> ParseDeskTemplateFromSource(
    const base::Value& policy_json,
    ash::DeskTemplateSource source) {
  if (!policy_json.is_dict())
    return nullptr;

  int version;
  std::string uuid_str;
  std::string name;
  std::string created_time_usec_str;
  std::string updated_time_usec_str;
  int64_t created_time_usec;
  int64_t updated_time_usec;
  const base::Value* desk = policy_json.FindDictKey(kDesk);
  if (!desk || !GetInt(policy_json, kVersion, &version) ||
      !GetString(policy_json, kUuid, &uuid_str) ||
      !GetString(policy_json, kName, &name) ||
      !GetString(policy_json, kCreatedTime, &created_time_usec_str) ||
      !base::StringToInt64(created_time_usec_str, &created_time_usec) ||
      !GetString(policy_json, kUpdatedTime, &updated_time_usec_str) ||
      !base::StringToInt64(updated_time_usec_str, &updated_time_usec) ||
      name.empty() || created_time_usec_str.empty() ||
      updated_time_usec_str.empty())
    return nullptr;

  base::GUID uuid = base::GUID::ParseCaseInsensitive(uuid_str);
  if (!uuid.is_valid())
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
          std::move(uuid), source, name, created_time,
          GetDeskTypeFromString(desk_type_string));

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

SyncLaunchContainer FromLaunchContainer(apps::LaunchContainer container) {
  switch (container) {
    case apps::LaunchContainer::kLaunchContainerWindow:
      return sync_pb::
          WorkspaceDeskSpecifics_LaunchContainer_LAUNCH_CONTAINER_WINDOW;
    case apps::LaunchContainer::kLaunchContainerPanelDeprecated:
      return sync_pb::
          WorkspaceDeskSpecifics_LaunchContainer_LAUNCH_CONTAINER_PANEL_DEPRECATED;
    case apps::LaunchContainer::kLaunchContainerTab:
      return sync_pb::
          WorkspaceDeskSpecifics_LaunchContainer_LAUNCH_CONTAINER_TAB;
    case apps::LaunchContainer::kLaunchContainerNone:
      return sync_pb::
          WorkspaceDeskSpecifics_LaunchContainer_LAUNCH_CONTAINER_NONE;
  }
}

apps::LaunchContainer ToLaunchContainer(SyncLaunchContainer container) {
  switch (container) {
    case sync_pb::
        WorkspaceDeskSpecifics_LaunchContainer_LAUNCH_CONTAINER_UNSPECIFIED:
      return apps::LaunchContainer::kLaunchContainerWindow;
    case sync_pb::
        WorkspaceDeskSpecifics_LaunchContainer_LAUNCH_CONTAINER_WINDOW:
      return apps::LaunchContainer::kLaunchContainerWindow;
    case sync_pb::
        WorkspaceDeskSpecifics_LaunchContainer_LAUNCH_CONTAINER_PANEL_DEPRECATED:
      return apps::LaunchContainer::kLaunchContainerPanelDeprecated;
    case sync_pb::WorkspaceDeskSpecifics_LaunchContainer_LAUNCH_CONTAINER_TAB:
      return apps::LaunchContainer::kLaunchContainerTab;
    case sync_pb::WorkspaceDeskSpecifics_LaunchContainer_LAUNCH_CONTAINER_NONE:
      return apps::LaunchContainer::kLaunchContainerNone;
  }
}

}  // namespace desk_template_conversion

}  // namespace desks_storage
