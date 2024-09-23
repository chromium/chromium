// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desks_storage/core/desk_template_conversion.h"

#include <optional>
#include <string_view>

#include "base/containers/fixed_flat_set.h"
#include "base/json/json_reader.h"
#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "chromeos/ui/base/window_state_type.h"
#include "components/app_constants/constants.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/restore_data.h"
#include "components/app_restore/window_info.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/sync/protocol/proto_enum_conversions.h"
#include "components/sync_device_info/device_info_proto_enum_util.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_info.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/gfx/geometry/rect.h"

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/cpp/lacros_startup_state.h"  // nogncheck
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

namespace {

using SyncWindowOpenDisposition =
    sync_pb::WorkspaceDeskSpecifics_WindowOpenDisposition;
using SyncLaunchContainer = sync_pb::WorkspaceDeskSpecifics_LaunchContainer;
using GroupColor = tab_groups::TabGroupColorId;
using BrowserAppTab =
    sync_pb::WorkspaceDeskSpecifics_BrowserAppWindow_BrowserAppTab;
using BrowserAppWindow = sync_pb::WorkspaceDeskSpecifics_BrowserAppWindow;
using ArcApp = sync_pb::WorkspaceDeskSpecifics_ArcApp;
using ArcAppWindowSize = sync_pb::WorkspaceDeskSpecifics_ArcApp_WindowSize;
using ash::DeskTemplate;
using ash::DeskTemplateSource;
using ash::DeskTemplateType;
using SyncDeskType = sync_pb::WorkspaceDeskSpecifics_DeskType;
using WindowState = sync_pb::WorkspaceDeskSpecifics_WindowState;
using WindowBound = sync_pb::WorkspaceDeskSpecifics_WindowBound;
using LaunchContainer = sync_pb::WorkspaceDeskSpecifics_LaunchContainer;
// Use name prefixed with Sync here to avoid name collision with original class
// which isn't defined in a namespace.
using SyncWindowOpenDisposition =
    sync_pb::WorkspaceDeskSpecifics_WindowOpenDisposition;
using ProgressiveWebApp = sync_pb::WorkspaceDeskSpecifics_ProgressiveWebApp;
using ChromeApp = sync_pb::WorkspaceDeskSpecifics_ChromeApp;
using WorkspaceDeskSpecifics_App = sync_pb::WorkspaceDeskSpecifics_App;
using SyncTabGroup = sync_pb::WorkspaceDeskSpecifics_BrowserAppWindow_TabGroup;
using SyncTabGroupColor = sync_pb::WorkspaceDeskSpecifics_TabGroupColor;
using TabGroupColor = tab_groups::TabGroupColorId;

// JSON value keys.
constexpr char kActiveTabIndex[] = "active_tab_index";
constexpr char kAppId[] = "app_id";
constexpr char kApps[] = "apps";
constexpr char kAppName[] = "app_name";
constexpr char kAppType[] = "app_type";
constexpr char kAppTypeArc[] = "ARC";
constexpr char kAppTypeArcAdminFormat[] = "arc";
constexpr char kAppTypeBrowser[] = "BROWSER";
constexpr char kAppTypeBrowserAdminFormat[] = "browser";
constexpr char kAppTypeChrome[] = "CHROME_APP";
constexpr char kAppTypeChromeAdminFormat[] = "chrome_app";
constexpr char kAppTypeProgressiveWebAdminFormat[] = "progressive_web_app";
constexpr char kAppTypeIsolatedWebAppAdminFormat[] = "isolated_web_app";
constexpr char kAppTypeUnknown[] = "UKNOWN";
constexpr char kAppTypeUnsupported[] = "UNSUPPORTED";
constexpr char kAutoLaunchOnStartup[] = "auto_launch_on_startup";
constexpr char kBoundsInRoot[] = "bounds_in_root";
constexpr char kCreatedTime[] = "created_time_usec";
constexpr char kDesk[] = "desk";
constexpr char kDeskType[] = "desk_type";
constexpr char kDeskTypeTemplate[] = "TEMPLATE";
constexpr char kDeskTypeSaveAndRecall[] = "SAVE_AND_RECALL";
constexpr char kDeskTypeFloatingWorkspace[] = "FLOATING_WORKSPACE";
constexpr char kDeskTypeUnknown[] = "UNKNOWN";
constexpr char kDisplayId[] = "display_id";
constexpr char kEventFlag[] = "event_flag";
constexpr char kFirstNonPinnedTabIndex[] = "first_non_pinned_tab_index";
constexpr char kIsAppTypeBrowser[] = "is_app";
constexpr char kLacrosProfileId[] = "lacros_profile_id";
constexpr char kLaunchContainer[] = "launch_container";
constexpr char kLaunchContainerWindow[] = "LAUNCH_CONTAINER_WINDOW";
constexpr char kLaunchContainerUnspecified[] = "LAUNCH_CONTAINER_UNSPECIFIED";
constexpr char kLaunchContainerPanelDeprecated[] = "LAUNCH_CONTAINER_PANEL";
constexpr char kLaunchContainerTab[] = "LAUNCH_CONTAINER_TAB";
constexpr char kLaunchContainerNone[] = "LAUNCH_CONTAINER_NONE";
constexpr char kMaximumSize[] = "maximum_size";
constexpr char kMinimumSize[] = "minimum_size";
constexpr char kName[] = "name";
constexpr char kOverrideUrl[] = "override_url";
constexpr char kPolicy[] = "policy";
constexpr char kPreMinimizedWindowState[] = "pre_minimized_window_state";
constexpr char kTabRangeFirstIndex[] = "first_index";
constexpr char kTabRangeLastIndex[] = "last_index";
constexpr char kSizeHeight[] = "height";
constexpr char kSizeWidth[] = "width";
constexpr char kSnapPercentage[] = "snap_percent";
constexpr char kTabs[] = "tabs";
constexpr char kTabsAdminFormat[] = "browser_tabs";
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
constexpr char kWindowStateFloated[] = "FLOATED";
constexpr char kZIndex[] = "z_index";

// Valid value sets.
constexpr auto kValidDeskTypes = base::MakeFixedFlatSet<std::string_view>(
    {kDeskTypeTemplate, kDeskTypeSaveAndRecall, kDeskTypeFloatingWorkspace});
constexpr auto kValidLaunchContainers =
    base::MakeFixedFlatSet<std::string_view>(
        {kLaunchContainerWindow, kLaunchContainerPanelDeprecated,
         kLaunchContainerTab, kLaunchContainerNone,
         kLaunchContainerUnspecified});
constexpr auto kValidWindowOpenDispositions =
    base::MakeFixedFlatSet<std::string_view>(
        {kWindowOpenDispositionUnknown, kWindowOpenDispositionCurrentTab,
         kWindowOpenDispositionSingletonTab,
         kWindowOpenDispositionNewForegroundTab,
         kWindowOpenDispositionNewBackgroundTab, kWindowOpenDispositionNewPopup,
         kWindowOpenDispositionNewWindow, kWindowOpenDispositionSaveToDisk,
         kWindowOpenDispositionOffTheRecord, kWindowOpenDispositionIgnoreAction,
         kWindowOpenDispositionSwitchToTab,
         kWindowOpenDispositionNewPictureInPicture});
constexpr auto kValidWindowStates = base::MakeFixedFlatSet<std::string_view>(
    {kWindowStateNormal, kWindowStateMinimized, kWindowStateMaximized,
     kWindowStateFullscreen, kWindowStatePrimarySnapped,
     kWindowStateSecondarySnapped, kWindowStateFloated, kZIndex});
constexpr auto kValidTabGroupColors = base::MakeFixedFlatSet<std::string_view>(
    {tab_groups::kTabGroupColorUnknown, tab_groups::kTabGroupColorGrey,
     tab_groups::kTabGroupColorBlue, tab_groups::kTabGroupColorRed,
     tab_groups::kTabGroupColorYellow, tab_groups::kTabGroupColorGreen,
     tab_groups::kTabGroupColorPink, tab_groups::kTabGroupColorPurple,
     tab_groups::kTabGroupColorCyan, tab_groups::kTabGroupColorOrange});

// Version number.
constexpr int kVersionNum = 1;

// Conversion to desk methods.
bool GetString(const base::Value::Dict& dict,
               const char* key,
               std::string* out) {
  const std::string* value = dict.FindString(key);
  if (!value)
    return false;

  *out = *value;
  return true;
}

bool GetInt(const base::Value::Dict& dict, const char* key, int* out) {
  std::optional<int> value = dict.FindInt(key);
  if (!value)
    return false;

  *out = *value;
  return true;
}

bool GetBool(const base::Value::Dict& dict, const char* key, bool* out) {
  std::optional<bool> value = dict.FindBool(key);
  if (!value)
    return false;

  *out = *value;
  return true;
}

// Get App ID from App proto.
std::string GetJsonAppId(const base::Value::Dict& app) {
  std::string app_type;
  if (GetString(app, kAppType, &app_type) && app_type == kAppTypeBrowser) {
    // Return the primary browser's known app ID.
    const bool is_lacros =
#if BUILDFLAG(IS_CHROMEOS_LACROS)
        true;
#else
        // Note that this will launch the browser as lacros if it is enabled,
        // even if it was saved as a non-lacros window (and vice-versa).
        crosapi::lacros_startup_state::IsLacrosEnabled();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

    // Browser app has a known app ID.
    return std::string(is_lacros ? app_constants::kLacrosAppId
                                 : app_constants::kChromeAppId);
  }

  // Fall back on a stored app_id (which may or may not be present).
  std::string app_id;
  GetString(app, kAppId, &app_id);

  return app_id;
}

// Convert a TabGroupInfo object to a base::Value::Dict.
base::Value::Dict ConvertTabGroupInfoToDict(
    const tab_groups::TabGroupInfo& group_info) {
  base::Value::Dict tab_group_dict;

  tab_group_dict.Set(kTabRangeFirstIndex,
                     static_cast<int>(group_info.tab_range.start()));
  tab_group_dict.Set(kTabRangeLastIndex,
                     static_cast<int>(group_info.tab_range.end()));
  tab_group_dict.Set(kTabGroupTitleKey,
                     base::UTF16ToUTF8(group_info.visual_data.title()));
  tab_group_dict.Set(kTabGroupColorKey, tab_groups::TabGroupColorToString(
                                            group_info.visual_data.color()));
  tab_group_dict.Set(kTabGroupIsCollapsed,
                     group_info.visual_data.is_collapsed());

  return tab_group_dict;
}

bool IsValidGroupColor(const std::string& group_color) {
  return base::Contains(kValidTabGroupColors, group_color);
}

GroupColor ConvertGroupColorStringToGroupColor(const std::string& group_color) {
  if (group_color == tab_groups::kTabGroupColorGrey) {
    return GroupColor::kGrey;
  } else if (group_color == tab_groups::kTabGroupColorBlue) {
    return GroupColor::kBlue;
  } else if (group_color == tab_groups::kTabGroupColorRed) {
    return GroupColor::kRed;
  } else if (group_color == tab_groups::kTabGroupColorYellow) {
    return GroupColor::kYellow;
  } else if (group_color == tab_groups::kTabGroupColorGreen) {
    return GroupColor::kGreen;
  } else if (group_color == tab_groups::kTabGroupColorPink) {
    return GroupColor::kPink;
  } else if (group_color == tab_groups::kTabGroupColorPurple) {
    return GroupColor::kPurple;
  } else if (group_color == tab_groups::kTabGroupColorCyan) {
    return GroupColor::kCyan;
  } else if (group_color == tab_groups::kTabGroupColorOrange) {
    return GroupColor::kOrange;
    // There is no UNKNOWN equivalent in GroupColor, simply default
    // to grey.
  } else if (group_color == tab_groups::kTabGroupColorUnknown) {
    return GroupColor::kGrey;
  } else {
    NOTREACHED_IN_MIGRATION();
    return GroupColor::kGrey;
  }
}

// Constructs a GroupVisualData from value `group_visual_data` IFF all fields
// are present and valid in the value parameter.  Returns true on success, false
// on failure.
bool MakeTabGroupVisualDataFromDict(
    const base::Value::Dict& tab_group,
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
bool MakeTabGroupRangeFromDict(const base::Value::Dict& tab_group,
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
std::optional<tab_groups::TabGroupInfo> MakeTabGroupInfoFromDict(
    const base::Value::Dict& tab_group) {
  std::optional<tab_groups::TabGroupInfo> tab_group_info = std::nullopt;

  tab_groups::TabGroupVisualData visual_data;
  gfx::Range range;
  if (MakeTabGroupRangeFromDict(tab_group, &range) &&
      MakeTabGroupVisualDataFromDict(tab_group, &visual_data)) {
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
    const base::Value::Dict& app) {
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
  if (GetString(app, kAppName, &app_name)) {
    app_launch_info->browser_extra_info.app_name = app_name;
  }

  std::string override_url;
  if (GetString(app, kOverrideUrl, &override_url)) {
    app_launch_info->override_url = GURL(override_url);
  }

  std::string lacros_profile_id_str;
  if (GetString(app, kLacrosProfileId, &lacros_profile_id_str)) {
    uint64_t lacros_profile_id = 0;
    if (base::StringToUint64(lacros_profile_id_str, &lacros_profile_id)) {
      app_launch_info->browser_extra_info.lacros_profile_id = lacros_profile_id;
    }
  }

  // TODO(crbug.com/1311801): Add support for actual event_flag values.
  app_launch_info->event_flag = 0;

  bool app_type_browser;
  if (GetBool(app, kIsAppTypeBrowser, &app_type_browser)) {
    app_launch_info->browser_extra_info.app_type_browser = app_type_browser;
  }

  if (app_id == app_constants::kLacrosAppId ||
      app_id == app_constants::kChromeAppId) {
    int active_tab_index;
    if (GetInt(app, kActiveTabIndex, &active_tab_index)) {
      app_launch_info->browser_extra_info.active_tab_index = active_tab_index;
    }

    int first_non_pinned_tab_index;
    if (GetInt(app, kFirstNonPinnedTabIndex, &first_non_pinned_tab_index)) {
      app_launch_info->browser_extra_info.first_non_pinned_tab_index =
          first_non_pinned_tab_index;
    }

    // Fill in the URL list
    if (const base::Value::List* tabs = app.FindList(kTabs)) {
      for (auto& tab : *tabs) {
        std::string url;
        if (GetString(tab.GetDict(), kTabUrl, &url)) {
          app_launch_info->browser_extra_info.urls.emplace_back(url);
        }
      }
    }

    // Fill the tab groups
    if (const base::Value::List* tab_groups = app.FindList(kTabGroups)) {
      for (auto& tab : *tab_groups) {
        std::optional<tab_groups::TabGroupInfo> tab_group =
            MakeTabGroupInfoFromDict(tab.GetDict());
        if (tab_group.has_value()) {
          app_launch_info->browser_extra_info.tab_group_infos.push_back(
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

// Convert JSON string WindowState `state` to ui::mojom::WindowShowState used by
// the app_restore::WindowInfo struct.
ui::mojom::WindowShowState ToUiWindowState(const std::string& window_state) {
  if (window_state == kWindowStateNormal)
    return ui::mojom::WindowShowState::kNormal;
  else if (window_state == kWindowStateMinimized)
    return ui::mojom::WindowShowState::kMinimized;
  else if (window_state == kWindowStateMaximized)
    return ui::mojom::WindowShowState::kMaximized;
  else if (window_state == kWindowStateFullscreen)
    return ui::mojom::WindowShowState::kFullscreen;
  else if (window_state == kWindowStatePrimarySnapped)
    return ui::mojom::WindowShowState::kNormal;
  else if (window_state == kWindowStateSecondarySnapped)
    return ui::mojom::WindowShowState::kNormal;
  // We should never reach here unless we have been passed an invalid window
  // state
  DCHECK(IsValidWindowState(window_state));
  return ui::mojom::WindowShowState::kNormal;
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
  else if (window_state == kWindowStateFloated)
    return chromeos::WindowStateType::kFloated;

  // We should never reach here unless we have been passed an invalid window
  // state.
  DCHECK(IsValidWindowState(window_state));
  return chromeos::WindowStateType::kNormal;
}

void FillArcExtraWindowInfoFromJson(
    const base::Value::Dict& app,
    app_restore::WindowInfo::ArcExtraInfo* out_window_info) {
  const base::Value::Dict* bounds_in_root = app.FindDict(kBoundsInRoot);
  int top;
  int left;
  int bounds_width;
  int bounds_height;
  if (bounds_in_root && GetInt(*bounds_in_root, kWindowBoundTop, &top) &&
      GetInt(*bounds_in_root, kWindowBoundLeft, &left) &&
      GetInt(*bounds_in_root, kWindowBoundWidth, &bounds_width) &&
      GetInt(*bounds_in_root, kWindowBoundHeight, &bounds_height)) {
    out_window_info->bounds_in_root.emplace(left, top, bounds_width,
                                            bounds_height);
  }

  const base::Value::Dict* maximum_size = app.FindDict(kMaximumSize);
  int max_width;
  int max_height;
  if (maximum_size && GetInt(*maximum_size, kSizeWidth, &max_width) &&
      GetInt(*maximum_size, kSizeHeight, &max_height)) {
    out_window_info->maximum_size.emplace(max_width, max_height);
  }

  const base::Value::Dict* minimum_size = app.FindDict(kMinimumSize);
  int min_width;
  int min_height;
  if (minimum_size && GetInt(*minimum_size, kSizeWidth, &min_width) &&
      GetInt(*minimum_size, kSizeHeight, &min_height)) {
    out_window_info->minimum_size.emplace(min_width, min_height);
  }
}

// Fill `out_window_info` with information from JSON `app`.
void FillWindowInfoFromJson(const base::Value::Dict& app,
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

  const base::Value::Dict* window_bound = app.FindDict(kWindowBound);
  int top;
  int left;
  int width;
  int height;
  if (window_bound && GetInt(*window_bound, kWindowBoundTop, &top) &&
      GetInt(*window_bound, kWindowBoundLeft, &left) &&
      GetInt(*window_bound, kWindowBoundWidth, &width) &&
      GetInt(*window_bound, kWindowBoundHeight, &height)) {
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
    const base::Value::Dict* desk) {
  std::unique_ptr<app_restore::RestoreData> restore_data =
      std::make_unique<app_restore::RestoreData>();

  const base::Value::List* apps = desk->FindList(kApps);
  if (apps) {
    for (const auto& app : *apps) {
      const base::Value::Dict& app_dict = app.GetDict();
      std::unique_ptr<app_restore::AppLaunchInfo> app_launch_info =
          ConvertJsonToAppLaunchInfo(app_dict);
      if (!app_launch_info)
        continue;  // Skip unsupported app.

      int window_id;
      if (!GetInt(app_dict, kWindowId, &window_id)) {
        return nullptr;
      }

      const std::string app_id = app_launch_info->app_id;
      restore_data->AddAppLaunchInfo(std::move(app_launch_info));

      app_restore::WindowInfo app_window_info;
      FillWindowInfoFromJson(app_dict, &app_window_info);

      restore_data->ModifyWindowInfo(app_id, window_id, app_window_info);
    }
  }

  return restore_data;
}

// Conversion to value methods.

base::Value::Dict ConvertWindowBoundToValue(const gfx::Rect& rect) {
  base::Value::Dict rectangle_value;

  rectangle_value.Set(kWindowBoundTop, base::Value(rect.y()));
  rectangle_value.Set(kWindowBoundLeft, base::Value(rect.x()));
  rectangle_value.Set(kWindowBoundHeight, base::Value(rect.height()));
  rectangle_value.Set(kWindowBoundWidth, base::Value(rect.width()));

  return rectangle_value;
}

base::Value::Dict ConvertSizeToValue(const gfx::Size& size) {
  base::Value::Dict size_value;

  size_value.Set(kSizeWidth, base::Value(size.width()));
  size_value.Set(kSizeHeight, base::Value(size.height()));

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
    case chromeos::WindowStateType::kFloated:
      return kWindowStateFloated;
    default:
      // Available states in JSON representation is a subset of all window
      // states enumerated by WindowStateType. Default to normal if not
      // supported.
      return kWindowStateNormal;
  }
}

// Convert ui::mojom::WindowShowState `state` to JSON used by the base::Value
// representation.
std::string UiWindowStateToString(
    const ui::mojom::WindowShowState& window_state) {
  switch (window_state) {
    case ui::mojom::WindowShowState::kNormal:
      return kWindowStateNormal;
    case ui::mojom::WindowShowState::kMinimized:
      return kWindowStateMinimized;
    case ui::mojom::WindowShowState::kMaximized:
      return kWindowStateMaximized;
    case ui::mojom::WindowShowState::kFullscreen:
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

base::Value::List ConvertURLsToBrowserAppTabValues(
    const std::vector<GURL>& urls) {
  base::Value::List tab_list;

  for (const auto& url : urls) {
    base::Value::Dict browser_tab;
    browser_tab.Set(kTabUrl, url.spec());
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

    case apps::AppType::kUnknown:
      return kAppTypeUnknown;

    case apps::AppType::kBuiltIn:
    case apps::AppType::kCrostini:
    case apps::AppType::kPluginVm:
    case apps::AppType::kRemote:
    case apps::AppType::kBorealis:
    case apps::AppType::kBruschetta:
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
  if (app_type == kAppTypeUnsupported) {
    return base::Value(base::Value::Type::NONE);
  }

  base::Value::Dict app_data;
  if (app_type != kAppTypeUnknown) {
    app_data.Set(kAppType, app_type);
  }

  if (app->window_info.current_bounds.has_value()) {
    app_data.Set(kWindowBound, ConvertWindowBoundToValue(
                                   app->window_info.current_bounds.value()));
  }

  const std::optional<app_restore::WindowInfo::ArcExtraInfo>& arc_info =
      app->window_info.arc_extra_info;
  if (arc_info) {
    if (arc_info->bounds_in_root.has_value()) {
      app_data.Set(kBoundsInRoot,
                   ConvertWindowBoundToValue(arc_info->bounds_in_root.value()));
    }

    if (arc_info->minimum_size.has_value()) {
      app_data.Set(kMinimumSize,
                   ConvertSizeToValue(arc_info->minimum_size.value()));
    }

    if (arc_info->maximum_size.has_value()) {
      app_data.Set(kMaximumSize,
                   ConvertSizeToValue(arc_info->maximum_size.value()));
    }
  }

  if (app->window_info.app_title.has_value()) {
    app_data.Set(kTitle, base::UTF16ToUTF8(app->window_info.app_title.value()));
  }

  chromeos::WindowStateType window_state = chromeos::WindowStateType::kDefault;
  if (app->window_info.window_state_type.has_value()) {
    window_state = app->window_info.window_state_type.value();
    app_data.Set(kWindowState, ChromeOsWindowStateToString(window_state));
  }

  // TODO(crbug.com/1311801): Add support for actual event_flag values.
  app_data.Set(kEventFlag, 0);

  if (app->window_info.activation_index.has_value()) {
    app_data.Set(kZIndex, app->window_info.activation_index.value());
  }

  if (!app->browser_extra_info.urls.empty()) {
    app_data.Set(
        kTabs, ConvertURLsToBrowserAppTabValues(app->browser_extra_info.urls));
  }

  if (!app->browser_extra_info.tab_group_infos.empty()) {
    base::Value::List tab_groups_value;

    for (const auto& tab_group : app->browser_extra_info.tab_group_infos) {
      tab_groups_value.Append(ConvertTabGroupInfoToDict(tab_group));
    }

    app_data.Set(kTabGroups, std::move(tab_groups_value));
  }

  if (app->browser_extra_info.active_tab_index.has_value()) {
    app_data.Set(kActiveTabIndex,
                 app->browser_extra_info.active_tab_index.value());
  }

  if (app->browser_extra_info.first_non_pinned_tab_index.has_value()) {
    app_data.Set(kFirstNonPinnedTabIndex,
                 app->browser_extra_info.first_non_pinned_tab_index.value());
  }

  if (app->browser_extra_info.app_type_browser.has_value()) {
    app_data.Set(kIsAppTypeBrowser,
                 app->browser_extra_info.app_type_browser.value());
  }

  app_data.Set(kAppId, app_id);

  app_data.Set(kWindowId, window_id);

  if (app->display_id.has_value()) {
    app_data.Set(kDisplayId, base::NumberToString(app->display_id.value()));
  }

  if (app->window_info.pre_minimized_show_state_type.has_value() &&
      window_state == chromeos::WindowStateType::kMinimized) {
    app_data.Set(kPreMinimizedWindowState,
                 UiWindowStateToString(
                     app->window_info.pre_minimized_show_state_type.value()));
  }

  if (app->window_info.snap_percentage.has_value()) {
    app_data.Set(kSnapPercentage,
                 static_cast<int>(app->window_info.snap_percentage.value()));
  }

  if (app->browser_extra_info.app_name.has_value()) {
    app_data.Set(kAppName, app->browser_extra_info.app_name.value());
  }

  if (app->disposition.has_value()) {
    WindowOpenDisposition disposition =
        static_cast<WindowOpenDisposition>(app->disposition.value());
    app_data.Set(kWindowOpenDisposition,
                 WindowOpenDispositionToString(disposition));
  }

  if (app->container.has_value()) {
    apps::LaunchContainer container =
        static_cast<apps::LaunchContainer>(app->container.value());
    app_data.Set(kLaunchContainer, LaunchContainerToString(container));
  }

  if (app->override_url.has_value()) {
    app_data.Set(kOverrideUrl, app->override_url->spec());
  }

  if (app->browser_extra_info.lacros_profile_id.has_value()) {
    app_data.Set(kLacrosProfileId,
                 base::NumberToString(
                     app->browser_extra_info.lacros_profile_id.value()));
  }

  return base::Value(std::move(app_data));
}

base::Value ConvertRestoreDataToValue(
    const app_restore::RestoreData* restore_data,
    apps::AppRegistryCache* apps_cache) {
  base::Value::List desk_data;

  for (const auto& app : restore_data->app_id_to_launch_list()) {
    for (const auto& window : app.second) {
      auto app_data = ConvertWindowToDeskApp(app.first, window.first,
                                             window.second.get(), apps_cache);
      if (app_data.is_none())
        continue;

      desk_data.Append(std::move(app_data));
    }
  }

  base::Value::Dict apps;
  apps.Set(kApps, std::move(desk_data));
  return base::Value(std::move(apps));
}

std::string SerializeDeskTypeAsString(ash::DeskTemplateType desk_type) {
  switch (desk_type) {
    case ash::DeskTemplateType::kTemplate:
      return kDeskTypeTemplate;
    case ash::DeskTemplateType::kSaveAndRecall:
      return kDeskTypeSaveAndRecall;
    case ash::DeskTemplateType::kFloatingWorkspace:
      return kDeskTypeFloatingWorkspace;
    case ash::DeskTemplateType::kUnknown:
      return kDeskTypeUnknown;
  }
}

bool IsValidDeskTemplateType(const std::string& desk_template_type) {
  return base::Contains(kValidDeskTypes, desk_template_type);
}

// TODO(b/258692868): Currently parse any invalid value for this field as
// SaveAndRecall. Fix by crash / signal some error instead.
ash::DeskTemplateType GetDeskTypeFromString(const std::string& desk_type) {
  DCHECK(IsValidDeskTemplateType(desk_type));
  if (desk_type == kDeskTypeTemplate)
    return ash::DeskTemplateType::kTemplate;
  else if (desk_type == kDeskTypeFloatingWorkspace)
    return ash::DeskTemplateType::kFloatingWorkspace;
  else if (desk_type == kDeskTypeSaveAndRecall)
    return ash::DeskTemplateType::kSaveAndRecall;
  else
    return ash::DeskTemplateType::kUnknown;
}

// Convert from apps::LaunchContainer to sync proto LaunchContainer.
// Assumes caller has cast `container` from int32_t to
// apps::LaunchContainer
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

// Convert from sync proto LaunchContainer to apps::LaunchContainer.
// Assumes caller has cast `container` from int32_t to
// apps::LaunchContainer
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

// Convert sync proto WindowOpenDisposition to base's WindowOpenDisposition.
// This value is cast to int32_t by the caller to be assigned to the
// `disposition` field in AppRestoreData.
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

// Fill `out_gurls` using tabs' URL in `browser_app_window`.
void FillUrlList(const BrowserAppWindow& browser_app_window,
                 std::vector<GURL>* out_gurls) {
  for (auto tab : browser_app_window.tabs()) {
    if (tab.has_url())
      out_gurls->emplace_back(tab.url());
  }
}

// Since tab groups must have completely valid fields therefore this function
// exists to validate that sync tab groups are entirely valid.
bool ValidSyncTabGroup(const SyncTabGroup& sync_tab_group) {
  return sync_tab_group.has_first_index() && sync_tab_group.has_last_index() &&
         sync_tab_group.has_title() && sync_tab_group.has_color();
}

// Converts a sync tab group color to its tab_groups::TabGroupColorId
// equivalent.
TabGroupColor TabGroupColorIdFromSyncTabColor(
    const SyncTabGroupColor& sync_color) {
  switch (sync_color) {
    // Default to grey if unknown.
    case SyncTabGroupColor::WorkspaceDeskSpecifics_TabGroupColor_UNKNOWN_COLOR:
    case SyncTabGroupColor::WorkspaceDeskSpecifics_TabGroupColor_GREY:
      return TabGroupColor::kGrey;
    case SyncTabGroupColor::WorkspaceDeskSpecifics_TabGroupColor_BLUE:
      return TabGroupColor::kBlue;
    case SyncTabGroupColor::WorkspaceDeskSpecifics_TabGroupColor_RED:
      return TabGroupColor::kRed;
    case SyncTabGroupColor::WorkspaceDeskSpecifics_TabGroupColor_YELLOW:
      return TabGroupColor::kYellow;
    case SyncTabGroupColor::WorkspaceDeskSpecifics_TabGroupColor_GREEN:
      return TabGroupColor::kGreen;
    case SyncTabGroupColor::WorkspaceDeskSpecifics_TabGroupColor_PINK:
      return TabGroupColor::kPink;
    case SyncTabGroupColor::WorkspaceDeskSpecifics_TabGroupColor_PURPLE:
      return TabGroupColor::kPurple;
    case SyncTabGroupColor::WorkspaceDeskSpecifics_TabGroupColor_CYAN:
      return TabGroupColor::kCyan;
    case SyncTabGroupColor::WorkspaceDeskSpecifics_TabGroupColor_ORANGE:
      return TabGroupColor::kOrange;
  };
}

// Instantiates a TabGroup from its sync equivalent.
tab_groups::TabGroupInfo FillTabGroupInfoFromProto(
    const SyncTabGroup& sync_tab_group) {
  // This function should never be called with a partially instantiated
  // tab group.
  DCHECK(ValidSyncTabGroup(sync_tab_group));

  return tab_groups::TabGroupInfo(
      {static_cast<uint32_t>(sync_tab_group.first_index()),
       static_cast<uint32_t>(sync_tab_group.last_index())},
      tab_groups::TabGroupVisualData(
          base::UTF8ToUTF16(sync_tab_group.title()),
          TabGroupColorIdFromSyncTabColor(sync_tab_group.color()),
          sync_tab_group.is_collapsed()));
}

// Fill `out_group_infos` using information found in the proto's
// tab group structure.
void FillTabGroupInfosFromProto(
    const BrowserAppWindow& browser_app_window,
    std::vector<tab_groups::TabGroupInfo>* out_group_infos) {
  for (const auto& group : browser_app_window.tab_groups()) {
    if (!ValidSyncTabGroup(group)) {
      continue;
    }

    out_group_infos->push_back(FillTabGroupInfoFromProto(group));
  }
}

// Get App ID from App proto.
std::string GetAppId(const sync_pb::WorkspaceDeskSpecifics_App& app) {
  switch (app.app().app_case()) {
    case sync_pb::WorkspaceDeskSpecifics_AppOneOf::AppCase::APP_NOT_SET:
      // Return an empty string to indicate this app is unsupported.
      return std::string();
    case sync_pb::WorkspaceDeskSpecifics_AppOneOf::AppCase::kBrowserAppWindow: {
      const bool is_lacros =
#if BUILDFLAG(IS_CHROMEOS_LACROS)
          true;
#else
          // Note that this will launch the browser as lacros if it is enabled,
          // even if it was saved as a non-lacros window (and vice-versa).
          crosapi::lacros_startup_state::IsLacrosEnabled();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

      // Browser app has a known app ID.
      return std::string(is_lacros ? app_constants::kLacrosAppId
                                   : app_constants::kChromeAppId);
    }
    case sync_pb::WorkspaceDeskSpecifics_AppOneOf::AppCase::kChromeApp:
      return app.app().chrome_app().app_id();
    case sync_pb::WorkspaceDeskSpecifics_AppOneOf::AppCase::kProgressWebApp:
      return app.app().progress_web_app().app_id();
    case sync_pb::WorkspaceDeskSpecifics_AppOneOf::AppCase::kArcApp:
      return app.app().arc_app().app_id();
  }
}

// Convert App proto to `app_restore::AppLaunchInfo`.
std::unique_ptr<app_restore::AppLaunchInfo> ConvertToAppLaunchInfo(
    const sync_pb::WorkspaceDeskSpecifics_App& app) {
  const std::string app_id = GetAppId(app);

  if (app_id.empty())
    return nullptr;

  auto app_launch_info =
      std::make_unique<app_restore::AppLaunchInfo>(app_id, app.window_id());

  if (app.has_display_id())
    app_launch_info->display_id = app.display_id();

  if (app.has_container()) {
    app_launch_info->container =
        static_cast<int32_t>(ToLaunchContainer(app.container()));
  }

  if (app.has_disposition()) {
    app_launch_info->disposition =
        static_cast<int32_t>(ToBaseWindowOpenDisposition(app.disposition()));
  }

  if (app.has_app_name()) {
    app_launch_info->browser_extra_info.app_name = app.app_name();
  }

  if (app.has_override_url()) {
    app_launch_info->override_url = GURL(app.override_url());
  }

  // This is a short-term fix as `event_flag` is required to launch ArcApp.
  // Currently we don't support persisting user action in template
  // so always default to 0 which is no action.
  // https://source.chromium.org/chromium/chromium/src/
  // +/main:ui/base/window_open_disposition.cc;l=34
  //
  // TODO(crbug.com/1311801): Add support for actual event_flag values.
  app_launch_info->event_flag = 0;

  switch (app.app().app_case()) {
    case sync_pb::WorkspaceDeskSpecifics_AppOneOf::AppCase::APP_NOT_SET:
      // This should never happen. `APP_NOT_SET` corresponds to empty `app_id`.
      // This method will early return when `app_id` is empty.
      NOTREACHED_IN_MIGRATION();
      break;
    case sync_pb::WorkspaceDeskSpecifics_AppOneOf::AppCase::kBrowserAppWindow:
      if (app.app().browser_app_window().has_active_tab_index()) {
        app_launch_info->browser_extra_info.active_tab_index =
            app.app().browser_app_window().active_tab_index();
      }

      FillUrlList(app.app().browser_app_window(),
                  &app_launch_info->browser_extra_info.urls);

      if (app.app().browser_app_window().tab_groups_size() > 0) {
        FillTabGroupInfosFromProto(
            app.app().browser_app_window(),
            &app_launch_info->browser_extra_info.tab_group_infos);
      }

      if (app.app().browser_app_window().has_show_as_app()) {
        app_launch_info->browser_extra_info.app_type_browser =
            app.app().browser_app_window().show_as_app();
      }

      if (app.app().browser_app_window().has_first_non_pinned_tab_index()) {
        app_launch_info->browser_extra_info.first_non_pinned_tab_index =
            app.app().browser_app_window().first_non_pinned_tab_index();
      }

      break;
    case sync_pb::WorkspaceDeskSpecifics_AppOneOf::AppCase::kChromeApp:
      // `app_id` is enough to identify a Chrome app.
      break;
    case sync_pb::WorkspaceDeskSpecifics_AppOneOf::AppCase::kProgressWebApp:
      // `app_id` is enough to identify a Progressive Web app.
      break;
    case sync_pb::WorkspaceDeskSpecifics_AppOneOf::AppCase::kArcApp:
      // `app_id` is enough to identify an Arc app.
      break;
  }

  return app_launch_info;
}

// Convert sync proto WindowOpenDisposition to base's WindowOpenDisposition.
// This value is cast to int32_t by the caller to be assigned to the
// `disposition` field in AppRestoreData.
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

// Convert Sync proto WindowState `state` to ui::mojom::WindowShowState used by
// the app_restore::WindowInfo struct.
ui::mojom::WindowShowState ToUiWindowState(WindowState state) {
  switch (state) {
    case WindowState::WorkspaceDeskSpecifics_WindowState_UNKNOWN_WINDOW_STATE:
      return ui::mojom::WindowShowState::kNormal;
    case WindowState::WorkspaceDeskSpecifics_WindowState_NORMAL:
      return ui::mojom::WindowShowState::kNormal;
    case WindowState::WorkspaceDeskSpecifics_WindowState_MINIMIZED:
      return ui::mojom::WindowShowState::kMinimized;
    case WindowState::WorkspaceDeskSpecifics_WindowState_MAXIMIZED:
      return ui::mojom::WindowShowState::kMaximized;
    case WindowState::WorkspaceDeskSpecifics_WindowState_FULLSCREEN:
      return ui::mojom::WindowShowState::kFullscreen;
    case WindowState::WorkspaceDeskSpecifics_WindowState_PRIMARY_SNAPPED:
      return ui::mojom::WindowShowState::kNormal;
    case WindowState::WorkspaceDeskSpecifics_WindowState_SECONDARY_SNAPPED:
      return ui::mojom::WindowShowState::kNormal;
    case WindowState::WorkspaceDeskSpecifics_WindowState_FLOATED:
      return ui::mojom::WindowShowState::kNormal;
  }
}

// Convert Sync proto WindowState `state` to chromeos::WindowStateType used
// by the app_restore::WindowInfo struct.
chromeos::WindowStateType ToChromeOsWindowState(WindowState state) {
  switch (state) {
    case WindowState::WorkspaceDeskSpecifics_WindowState_UNKNOWN_WINDOW_STATE:
      return chromeos::WindowStateType::kNormal;
    case WindowState::WorkspaceDeskSpecifics_WindowState_NORMAL:
      return chromeos::WindowStateType::kNormal;
    case WindowState::WorkspaceDeskSpecifics_WindowState_MINIMIZED:
      return chromeos::WindowStateType::kMinimized;
    case WindowState::WorkspaceDeskSpecifics_WindowState_MAXIMIZED:
      return chromeos::WindowStateType::kMaximized;
    case WindowState::WorkspaceDeskSpecifics_WindowState_FULLSCREEN:
      return chromeos::WindowStateType::kFullscreen;
    case WindowState::WorkspaceDeskSpecifics_WindowState_PRIMARY_SNAPPED:
      return chromeos::WindowStateType::kPrimarySnapped;
    case WindowState::WorkspaceDeskSpecifics_WindowState_SECONDARY_SNAPPED:
      return chromeos::WindowStateType::kSecondarySnapped;
    case WindowState::WorkspaceDeskSpecifics_WindowState_FLOATED:
      return chromeos::WindowStateType::kFloated;
  }
}

// Convert chromeos::WindowStateType to Sync proto WindowState.
WindowState FromChromeOsWindowState(chromeos::WindowStateType state) {
  switch (state) {
    case chromeos::WindowStateType::kDefault:
    case chromeos::WindowStateType::kNormal:
    case chromeos::WindowStateType::kInactive:
    case chromeos::WindowStateType::kPinned:
    case chromeos::WindowStateType::kTrustedPinned:
    case chromeos::WindowStateType::kPip:
      // TODO(crbug.com/1331825): Float state support for desk template.
      return WindowState::WorkspaceDeskSpecifics_WindowState_NORMAL;
    case chromeos::WindowStateType::kMinimized:
      return WindowState::WorkspaceDeskSpecifics_WindowState_MINIMIZED;
    case chromeos::WindowStateType::kMaximized:
      return WindowState::WorkspaceDeskSpecifics_WindowState_MAXIMIZED;
    case chromeos::WindowStateType::kFullscreen:
      return WindowState::WorkspaceDeskSpecifics_WindowState_FULLSCREEN;
    case chromeos::WindowStateType::kPrimarySnapped:
      return WindowState::WorkspaceDeskSpecifics_WindowState_PRIMARY_SNAPPED;
    case chromeos::WindowStateType::kSecondarySnapped:
      return WindowState::WorkspaceDeskSpecifics_WindowState_SECONDARY_SNAPPED;
    case chromeos::WindowStateType::kFloated:
      return WindowState::WorkspaceDeskSpecifics_WindowState_FLOATED;
  }
}

// Convert ui::mojom::WindowShowState to Sync proto WindowState.
WindowState FromUiWindowState(ui::mojom::WindowShowState state) {
  switch (state) {
    case ui::mojom::WindowShowState::kDefault:
    case ui::mojom::WindowShowState::kNormal:
    case ui::mojom::WindowShowState::kInactive:
    case ui::mojom::WindowShowState::kEnd:
      return WindowState::WorkspaceDeskSpecifics_WindowState_NORMAL;
    case ui::mojom::WindowShowState::kMinimized:
      return WindowState::WorkspaceDeskSpecifics_WindowState_MINIMIZED;
    case ui::mojom::WindowShowState::kMaximized:
      return WindowState::WorkspaceDeskSpecifics_WindowState_MAXIMIZED;
    case ui::mojom::WindowShowState::kFullscreen:
      return WindowState::WorkspaceDeskSpecifics_WindowState_FULLSCREEN;
  }
}

// Converts a sync tab group color to its tab_groups::TabGroupColorId
// equivalent.
SyncTabGroupColor SyncTabColorFromTabGroupColorId(
    const TabGroupColor& sync_color) {
  switch (sync_color) {
    case TabGroupColor::kGrey:
      return SyncTabGroupColor::WorkspaceDeskSpecifics_TabGroupColor_GREY;
    case TabGroupColor::kBlue:
      return SyncTabGroupColor::WorkspaceDeskSpecifics_TabGroupColor_BLUE;
    case TabGroupColor::kRed:
      return SyncTabGroupColor::WorkspaceDeskSpecifics_TabGroupColor_RED;
    case TabGroupColor::kYellow:
      return SyncTabGroupColor::WorkspaceDeskSpecifics_TabGroupColor_YELLOW;
    case TabGroupColor::kGreen:
      return SyncTabGroupColor::WorkspaceDeskSpecifics_TabGroupColor_GREEN;
    case TabGroupColor::kPink:
      return SyncTabGroupColor::WorkspaceDeskSpecifics_TabGroupColor_PINK;
    case TabGroupColor::kPurple:
      return SyncTabGroupColor::WorkspaceDeskSpecifics_TabGroupColor_PURPLE;
    case TabGroupColor::kCyan:
      return SyncTabGroupColor::WorkspaceDeskSpecifics_TabGroupColor_CYAN;
    case TabGroupColor::kOrange:
      return SyncTabGroupColor::WorkspaceDeskSpecifics_TabGroupColor_ORANGE;
    case TabGroupColor::kNumEntries:
      NOTREACHED_IN_MIGRATION() << "kNumEntries is not a supported color enum.";
      return SyncTabGroupColor::WorkspaceDeskSpecifics_TabGroupColor_GREY;
  };
}

void FillSyncTabGroupInfo(const tab_groups::TabGroupInfo& tab_group_info,
                          SyncTabGroup* out_sync_tab_group) {
  out_sync_tab_group->set_first_index(tab_group_info.tab_range.start());
  out_sync_tab_group->set_last_index(tab_group_info.tab_range.end());
  out_sync_tab_group->set_title(
      base::UTF16ToUTF8(tab_group_info.visual_data.title()));
  // Save some storage space by leaving is_collapsed to default value if the
  // tab group isn't collapsed.
  if (tab_group_info.visual_data.is_collapsed()) {
    out_sync_tab_group->set_is_collapsed(
        tab_group_info.visual_data.is_collapsed());
  }
  out_sync_tab_group->set_color(
      SyncTabColorFromTabGroupColorId(tab_group_info.visual_data.color()));
}

void FillBrowserAppTabGroupInfos(
    const std::vector<tab_groups::TabGroupInfo>& tab_group_infos,
    BrowserAppWindow* out_browser_app_window) {
  for (const auto& tab_group : tab_group_infos) {
    SyncTabGroup* sync_tab_group = out_browser_app_window->add_tab_groups();
    FillSyncTabGroupInfo(tab_group, sync_tab_group);
  }
}

// Fill `out_browser_app_window` with the given GURLs as BrowserAppTabs.
void FillBrowserAppTabs(const std::vector<GURL>& gurls,
                        BrowserAppWindow* out_browser_app_window) {
  for (const auto& gurl : gurls) {
    const std::string& url = gurl.spec();
    if (url.empty()) {
      // Skip invalid URLs.
      continue;
    }
    BrowserAppTab* browser_app_tab = out_browser_app_window->add_tabs();
    browser_app_tab->set_url(url);
  }
}

// Fill `out_browser_app_window` with urls and tab information from
// `app_restore_data`.
void FillBrowserAppWindow(const app_restore::AppRestoreData* app_restore_data,
                          BrowserAppWindow* out_browser_app_window) {
  const app_restore::BrowserExtraInfo browser_extra_info =
      app_restore_data->browser_extra_info;
  if (!browser_extra_info.urls.empty()) {
    FillBrowserAppTabs(browser_extra_info.urls, out_browser_app_window);
  }

  if (browser_extra_info.active_tab_index.has_value()) {
    out_browser_app_window->set_active_tab_index(
        browser_extra_info.active_tab_index.value());
  }

  if (browser_extra_info.app_type_browser.has_value()) {
    out_browser_app_window->set_show_as_app(
        browser_extra_info.app_type_browser.value());
  }

  if (!browser_extra_info.tab_group_infos.empty()) {
    FillBrowserAppTabGroupInfos(browser_extra_info.tab_group_infos,
                                out_browser_app_window);
  }

  if (browser_extra_info.first_non_pinned_tab_index.has_value()) {
    out_browser_app_window->set_first_non_pinned_tab_index(
        browser_extra_info.first_non_pinned_tab_index.value());
  }
}

// Fill `out_window_bounds` with information from `bounds`.
void FillWindowBound(const gfx::Rect& bounds, WindowBound* out_window_bounds) {
  out_window_bounds->set_left(bounds.x());
  out_window_bounds->set_top(bounds.y());
  out_window_bounds->set_width(bounds.width());
  out_window_bounds->set_height(bounds.height());
}

// Fill `out_app` with information from `window_info`.
void FillAppWithWindowInfo(const app_restore::WindowInfo* window_info,
                           WorkspaceDeskSpecifics_App* out_app) {
  if (window_info->activation_index.has_value())
    out_app->set_z_index(window_info->activation_index.value());

  if (window_info->current_bounds.has_value()) {
    FillWindowBound(window_info->current_bounds.value(),
                    out_app->mutable_window_bound());
  }

  if (window_info->window_state_type.has_value()) {
    out_app->set_window_state(
        FromChromeOsWindowState(window_info->window_state_type.value()));
  }

  if (window_info->pre_minimized_show_state_type.has_value()) {
    out_app->set_pre_minimized_window_state(
        FromUiWindowState(window_info->pre_minimized_show_state_type.value()));
  }

  if (window_info->snap_percentage.has_value())
    out_app->set_snap_percentage(window_info->snap_percentage.value());

  if (window_info->app_title.has_value())
    out_app->set_title(base::UTF16ToUTF8(window_info->app_title.value()));

  // AppRestoreData.GetWindowInfo does not include `display_id` in the returned
  // WindowInfo. Therefore, we are not filling `display_id` here.
}

//  Fill `out_app` with the `display_id` from `app_restore_data`.
void FillAppWithDisplayId(const app_restore::AppRestoreData* app_restore_data,
                          WorkspaceDeskSpecifics_App* out_app) {
  if (app_restore_data->display_id.has_value())
    out_app->set_display_id(app_restore_data->display_id.value());
}

//  Fill `out_app` with `container` from `app_restore_data`.
void FillAppWithLaunchContainer(
    const app_restore::AppRestoreData* app_restore_data,
    WorkspaceDeskSpecifics_App* out_app) {
  if (app_restore_data->container.has_value()) {
    out_app->set_container(
        FromLaunchContainer(static_cast<apps::LaunchContainer>(
            app_restore_data->container.value())));
  }
}

// Fill `out_app` with `disposition` from `app_restore_data`.
void FillAppWithWindowOpenDisposition(
    const app_restore::AppRestoreData* app_restore_data,
    WorkspaceDeskSpecifics_App* out_app) {
  if (app_restore_data->disposition.has_value()) {
    out_app->set_disposition(
        FromBaseWindowOpenDisposition(static_cast<WindowOpenDisposition>(
            app_restore_data->disposition.value())));
  }
}

// Fills `out_app` with `app_name` and `title` from `app_restore_data`.
void FillAppWithAppNameAndTitle(
    const app_restore::AppRestoreData* app_restore_data,
    WorkspaceDeskSpecifics_App* out_app) {
  const std::string app_name =
      app_restore_data->browser_extra_info.app_name.value_or("");
  if (!app_name.empty()) {
    out_app->set_app_name(app_name);
  }

  if (app_restore_data->window_info.app_title.has_value() &&
      !app_restore_data->window_info.app_title->empty()) {
    out_app->set_title(
        base::UTF16ToUTF8(*app_restore_data->window_info.app_title));
  }
}

void FillAppWithAppOverrideUrl(
    const app_restore::AppRestoreData* app_restore_data,
    WorkspaceDeskSpecifics_App* out_app) {
  if (app_restore_data->override_url.has_value()) {
    out_app->set_override_url(app_restore_data->override_url->spec());
  }
}

void FillArcAppSize(const gfx::Size& size, ArcAppWindowSize* out_window_size) {
  out_window_size->set_width(size.width());
  out_window_size->set_height(size.height());
}

void FillArcBoundsInRoot(const gfx::Rect& data_rect, WindowBound* out_rect) {
  out_rect->set_left(data_rect.x());
  out_rect->set_top(data_rect.y());
  out_rect->set_width(data_rect.width());
  out_rect->set_height(data_rect.height());
}

void FillArcApp(const app_restore::AppRestoreData* app_restore_data,
                ArcApp* out_app) {
  const std::optional<app_restore::WindowInfo::ArcExtraInfo>& arc_info =
      app_restore_data->window_info.arc_extra_info;
  if (!arc_info) {
    return;
  }

  if (arc_info->minimum_size.has_value()) {
    FillArcAppSize(arc_info->minimum_size.value(),
                   out_app->mutable_minimum_size());
  }
  if (arc_info->maximum_size.has_value()) {
    FillArcAppSize(arc_info->maximum_size.value(),
                   out_app->mutable_maximum_size());
  }
  if (arc_info->bounds_in_root.has_value()) {
    FillArcBoundsInRoot(arc_info->bounds_in_root.value(),
                        out_app->mutable_bounds_in_root());
  }
}

// Fills an app with container and open disposition.  This is only done in the
// specific cases of Chrome Apps and PWAs.
void FillAppWithLaunchContainerAndOpenDisposition(
    const app_restore::AppRestoreData* app_restore_data,
    WorkspaceDeskSpecifics_App* out_app) {
  // If present, fills the proto's `container` field with the information stored
  // in the `app_restore_data`'s `container` field.
  FillAppWithLaunchContainer(app_restore_data, out_app);

  // If present, fills the proto's `disposition` field with the information
  // stored in the `app_restore_data`'s `disposition` field.
  FillAppWithWindowOpenDisposition(app_restore_data, out_app);
}

// Fill `out_app` with `app_restore_data`.
// Return `false` if app type is unsupported.
bool FillApp(const std::string& app_id,
             const apps::AppType app_type,
             const app_restore::AppRestoreData* app_restore_data,
             WorkspaceDeskSpecifics_App* out_app) {
  // See definition in components/services/app_service/public/cpp/app_types.h
  switch (app_type) {
    case apps::AppType::kWeb:
    case apps::AppType::kSystemWeb: {
      // System Web Apps.
      // kSystemWeb is returned for System Web Apps in Lacros-primary
      // configuration. These can be persisted and launched the same way as
      // Chrome Apps.
      ChromeApp* chrome_app_window =
          out_app->mutable_app()->mutable_chrome_app();
      chrome_app_window->set_app_id(app_id);
      FillAppWithLaunchContainerAndOpenDisposition(app_restore_data, out_app);
      break;
    }

    case apps::AppType::kChromeApp: {
      // Ash Chrome browser OR PWA OR Chrome App hosted in Ash Chrome.
      if (app_constants::kChromeAppId == app_id) {
        // This window is either a browser window or a PWA window.
        // Both cases are persisted as "browser app" since they are launched the
        // same way. PWA window will have field `app_name` and
        // `app_type_browser` fields set. FillAppWithAppNameAndTitle has
        // persisted `app_name` field. FillBrowserAppWindow will persist
        // `app_type_browser` field.
        BrowserAppWindow* browser_app_window =
            out_app->mutable_app()->mutable_browser_app_window();
        FillBrowserAppWindow(app_restore_data, browser_app_window);
      } else {
        // Chrome App
        ChromeApp* chrome_app_window =
            out_app->mutable_app()->mutable_chrome_app();
        chrome_app_window->set_app_id(app_id);
        FillAppWithLaunchContainerAndOpenDisposition(app_restore_data, out_app);
      }
      break;
    }

    case apps::AppType::kStandaloneBrowser: {
      if (app_constants::kLacrosAppId == app_id) {
        // Lacros Chrome browser window or PWA hosted in Lacros Chrome.
        BrowserAppWindow* browser_app_window =
            out_app->mutable_app()->mutable_browser_app_window();
        FillBrowserAppWindow(app_restore_data, browser_app_window);
      } else {
        // Chrome app running in Lacros should have
        // AppType::kStandaloneBrowserChromeApp and never reach here.
        NOTREACHED_IN_MIGRATION();
        // Ignore this app type.
        return false;
      }

      break;
    }

    case apps::AppType::kStandaloneBrowserChromeApp: {
      // Chrome App hosted in Lacros.
      ChromeApp* chrome_app_window =
          out_app->mutable_app()->mutable_chrome_app();
      chrome_app_window->set_app_id(app_id);
      FillAppWithLaunchContainerAndOpenDisposition(app_restore_data, out_app);
      break;
    }

    case apps::AppType::kArc: {
      ArcApp* arc_app = out_app->mutable_app()->mutable_arc_app();
      arc_app->set_app_id(app_id);
      FillArcApp(app_restore_data, arc_app);
      break;
    }

    case apps::AppType::kBuiltIn:
    case apps::AppType::kCrostini:
    case apps::AppType::kPluginVm:
    case apps::AppType::kUnknown:
    case apps::AppType::kRemote:
    case apps::AppType::kBorealis:
    case apps::AppType::kBruschetta:
    case apps::AppType::kExtension:
    case apps::AppType::kStandaloneBrowserExtension:
      // Unsupported app types will be ignored.
      return false;
  }

  FillAppWithWindowInfo(app_restore_data->GetWindowInfo().get(), out_app);

  // AppRestoreData.GetWindowInfo does not include `display_id` in the returned
  // WindowInfo. We need to fill the `display_id` from AppRestoreData.
  FillAppWithDisplayId(app_restore_data, out_app);

  // If present, fills the proto's `app_name` and `title` fields with the
  // information stored in the `app_restore_data`'s `app_name` and `title`
  // fields.
  FillAppWithAppNameAndTitle(app_restore_data, out_app);

  // If present, fills the proto's `override_url` field with the information
  // from `app_restore_data`.
  FillAppWithAppOverrideUrl(app_restore_data, out_app);

  return true;
}

void FillArcExtraInfoFromProto(const ArcApp& app,
                               app_restore::WindowInfo* out_window_info) {
  out_window_info->arc_extra_info.emplace();
  app_restore::WindowInfo::ArcExtraInfo& arc_info =
      out_window_info->arc_extra_info.value();
  if (app.has_minimum_size()) {
    arc_info.minimum_size.emplace(app.minimum_size().width(),
                                  app.minimum_size().height());
  }
  if (app.has_maximum_size()) {
    arc_info.maximum_size.emplace(app.maximum_size().width(),
                                  app.maximum_size().height());
  }

  if (app.has_bounds_in_root()) {
    arc_info.bounds_in_root.emplace(
        app.bounds_in_root().left(), app.bounds_in_root().top(),
        app.bounds_in_root().width(), app.bounds_in_root().height());
  }
}

// Fill `out_window_info` with information from Sync proto `app`.
void FillWindowInfoFromProto(sync_pb::WorkspaceDeskSpecifics_App& app,
                             app_restore::WindowInfo* out_window_info) {
  if (app.has_window_state() &&
      sync_pb::WorkspaceDeskSpecifics_WindowState_IsValid(app.window_state())) {
    out_window_info->window_state_type.emplace(
        ToChromeOsWindowState(app.window_state()));
  }

  if (app.has_window_bound()) {
    out_window_info->current_bounds.emplace(
        app.window_bound().left(), app.window_bound().top(),
        app.window_bound().width(), app.window_bound().height());
  }

  if (app.has_z_index())
    out_window_info->activation_index.emplace(app.z_index());

  if (app.has_display_id())
    out_window_info->display_id.emplace(app.display_id());

  if (app.has_pre_minimized_window_state() &&
      app.window_state() ==
          sync_pb::WorkspaceDeskSpecifics_WindowState_MINIMIZED) {
    out_window_info->pre_minimized_show_state_type.emplace(
        ToUiWindowState(app.pre_minimized_window_state()));
  }

  if (app.has_snap_percentage() &&
      (app.window_state() ==
           sync_pb::WorkspaceDeskSpecifics_WindowState_PRIMARY_SNAPPED ||
       app.window_state() ==
           sync_pb::WorkspaceDeskSpecifics_WindowState_SECONDARY_SNAPPED)) {
    out_window_info->snap_percentage.emplace(app.snap_percentage());
  }

  if (app.has_title())
    out_window_info->app_title.emplace(base::UTF8ToUTF16(app.title()));

  if (app.app().app_case() ==
      sync_pb::WorkspaceDeskSpecifics_AppOneOf::AppCase::kArcApp) {
    FillArcExtraInfoFromProto(app.app().arc_app(), out_window_info);
  }
}

// Convert a desk template to `app_restore::RestoreData`.
std::unique_ptr<app_restore::RestoreData> ConvertToRestoreData(
    const sync_pb::WorkspaceDeskSpecifics& entry_proto) {
  auto restore_data = std::make_unique<app_restore::RestoreData>();

  for (auto app_proto : entry_proto.desk().apps()) {
    std::unique_ptr<app_restore::AppLaunchInfo> app_launch_info =
        ConvertToAppLaunchInfo(app_proto);
    if (!app_launch_info) {
      // Skip unsupported app.
      continue;
    }

    const std::string app_id = app_launch_info->app_id;
    restore_data->AddAppLaunchInfo(std::move(app_launch_info));

    app_restore::WindowInfo app_window_info;
    FillWindowInfoFromProto(app_proto, &app_window_info);

    restore_data->ModifyWindowInfo(app_id, app_proto.window_id(),
                                   app_window_info);
  }

  return restore_data;
}

// Fill a desk template `out_entry_proto` with information from
// `restore_data`.
void FillWorkspaceDeskSpecifics(
    apps::AppRegistryCache* apps_cache,
    const app_restore::RestoreData* restore_data,
    sync_pb::WorkspaceDeskSpecifics* out_entry_proto) {
  DCHECK(apps_cache);

  for (auto const& app_id_to_launch_list :
       restore_data->app_id_to_launch_list()) {
    const std::string app_id = app_id_to_launch_list.first;

    for (auto const& window_id_to_launch_info : app_id_to_launch_list.second) {
      const int window_id = window_id_to_launch_info.first;
      const app_restore::AppRestoreData* app_restore_data =
          window_id_to_launch_info.second.get();

      const auto app_type = apps_cache->GetAppType(app_id);

      WorkspaceDeskSpecifics_App* app =
          out_entry_proto->mutable_desk()->add_apps();
      app->set_window_id(window_id);
      if (!FillApp(app_id, app_type, app_restore_data, app)) {
        // Unsupported app type, remove this app entry.
        out_entry_proto->mutable_desk()->mutable_apps()->RemoveLast();
      }
    }
  }
}

// Fill a desk template `out_entry_proto` with the type of desk based on the
// desk's type field.
void FillDeskType(const DeskTemplate* desk_template,
                  sync_pb::WorkspaceDeskSpecifics* out_entry_proto) {
  switch (desk_template->type()) {
    case DeskTemplateType::kTemplate:
      out_entry_proto->set_desk_type(
          SyncDeskType::WorkspaceDeskSpecifics_DeskType_TEMPLATE);
      return;
    case DeskTemplateType::kSaveAndRecall:
      out_entry_proto->set_desk_type(
          SyncDeskType::WorkspaceDeskSpecifics_DeskType_SAVE_AND_RECALL);
      return;
    case DeskTemplateType::kFloatingWorkspace:
      out_entry_proto->set_desk_type(
          SyncDeskType::WorkspaceDeskSpecifics_DeskType_FLOATING_WORKSPACE);
      return;
    // Do nothing if type is unknown.
    case DeskTemplateType::kUnknown:
      return;
  }
}

// Takes in the Proto enum for a desk type `proto_type` and returns it's
// DeskTemplateType equivalent.
DeskTemplateType GetDeskTemplateTypeFromProtoType(
    const SyncDeskType& proto_type) {
  switch (proto_type) {
    // Treat unknown desk types as templates.
    case SyncDeskType::WorkspaceDeskSpecifics_DeskType_UNKNOWN_TYPE:
      return DeskTemplateType::kUnknown;
    case SyncDeskType::WorkspaceDeskSpecifics_DeskType_TEMPLATE:
      return DeskTemplateType::kTemplate;
    case SyncDeskType::WorkspaceDeskSpecifics_DeskType_SAVE_AND_RECALL:
      return DeskTemplateType::kSaveAndRecall;
    case SyncDeskType::WorkspaceDeskSpecifics_DeskType_FLOATING_WORKSPACE:
      return DeskTemplateType::kFloatingWorkspace;
  }
}

// Corrects the admin template browser format so that subsequent serialization
// code stores browsers correctly.
void CorrectAdminTemplateBrowserFormat(base::Value& app) {
  if (!app.is_dict()) {
    return;
  }

  auto& app_dict = app.GetDict();
  base::Value::List* tabs = app_dict.FindList(kTabsAdminFormat);

  if (tabs == nullptr) {
    return;
  }

  app_dict.Set(kTabs, tabs->Clone());
}

// Corrects the admin template format for app types.  Modifies the reference
// passed.
void CorrectAdminTemplateAppTypeFormat(base::Value& app) {
  if (!app.is_dict()) {
    return;
  }

  auto& app_dict = app.GetDict();

  std::string app_type;
  if (!GetString(app_dict, kAppType, &app_type)) {
    return;
  }

  // In the future all these types will be supported so we include them here
  // to exhaust the possible enum types that can be given to us.  However
  // app types that are not browser will not be supported in the current version
  // of admin templates so return unsupported for everything other than
  // browsers.
  if (app_type == kAppTypeBrowserAdminFormat) {
    app_dict.Set(kAppType, kAppTypeBrowser);
    CorrectAdminTemplateBrowserFormat(app);
  } else if (app_type == kAppTypeArcAdminFormat) {
    app_dict.Set(kAppType, kAppTypeUnsupported);
  } else if (app_type == kAppTypeChromeAdminFormat) {
    app_dict.Set(kAppType, kAppTypeUnsupported);
  } else if (app_type == kAppTypeProgressiveWebAdminFormat) {
    app_dict.Set(kAppType, kAppTypeUnsupported);
  } else if (app_type == kAppTypeIsolatedWebAppAdminFormat) {
    app_dict.Set(kAppType, kAppTypeUnsupported);
  } else {
    app_dict.Set(kAppType, kAppTypeUnsupported);
  }
}

// Modifies the strings in the desk's apps such that they match the format
// defined by this file.  This does not verify the format, that is handled
// by `ConvertJsonToRestoreData`.  The value is copied and returned corrected.
// If the admin format itself is corrupted return the clone, it will be
// discarded by the parsing code.
base::Value::Dict CorrectAdminTemplateFormat(const base::Value::Dict* desk) {
  auto desk_clone = desk->Clone();
  base::Value::List* apps = desk_clone.FindList(kApps);
  if (apps == nullptr) {
    return desk_clone;
  }

  if (apps) {
    for (auto& app : *apps) {
      CorrectAdminTemplateAppTypeFormat(app);
    }
  }

  return desk_clone;
}

std::unique_ptr<ash::DeskTemplate> ParseAdminTemplate(
    const base::Value& admin_template) {
  if (!admin_template.is_dict()) {
    return nullptr;
  }

  const base::Value::Dict& value_dict = admin_template.GetDict();

  bool auto_launch_on_startup;
  std::string created_time_usec_str;
  int64_t created_time_usec;
  std::string name;
  std::string updated_time_usec_str;
  int64_t updated_time_usec;
  std::string uuid_str;
  const base::Value::Dict* desk = value_dict.FindDict(kDesk);
  if (!desk ||
      !GetBool(value_dict, kAutoLaunchOnStartup, &auto_launch_on_startup) ||
      !GetString(value_dict, kUuid, &uuid_str) ||
      !GetString(value_dict, kName, &name) ||
      !GetString(value_dict, kCreatedTime, &created_time_usec_str) ||
      !base::StringToInt64(created_time_usec_str, &created_time_usec) ||
      !GetString(value_dict, kUpdatedTime, &updated_time_usec_str) ||
      !base::StringToInt64(updated_time_usec_str, &updated_time_usec) ||
      name.empty() || created_time_usec_str.empty() ||
      updated_time_usec_str.empty()) {
    return nullptr;
  }

  base::Uuid uuid = base::Uuid::ParseCaseInsensitive(uuid_str);
  if (!uuid.is_valid()) {
    return nullptr;
  }

  const base::Time created_time =
      desks_storage::desk_template_conversion::ProtoTimeToTime(
          created_time_usec);
  const base::Time updated_time =
      desks_storage::desk_template_conversion::ProtoTimeToTime(
          updated_time_usec);

  auto ash_admin_template = std::make_unique<ash::DeskTemplate>(
      std::move(uuid), ash::DeskTemplateSource::kPolicy, name, created_time,
      ash::DeskTemplateType::kTemplate, auto_launch_on_startup,
      admin_template.Clone());

  auto corrected_desk = CorrectAdminTemplateFormat(desk);
  ash_admin_template->set_updated_time(updated_time);
  ash_admin_template->set_desk_restore_data(
      ConvertJsonToRestoreData(&corrected_desk));

  return ash_admin_template;
}

}  // namespace

namespace desks_storage {

namespace desk_template_conversion {

// Converts the TabGroupColorId passed into its string equivalent
// as defined in the k constants above.
std::string ConvertTabGroupColorIdToString(GroupColor color) {
  switch (color) {
    case GroupColor::kGrey:
      return tab_groups::kTabGroupColorGrey;
    case GroupColor::kBlue:
      return tab_groups::kTabGroupColorBlue;
    case GroupColor::kRed:
      return tab_groups::kTabGroupColorRed;
    case GroupColor::kYellow:
      return tab_groups::kTabGroupColorYellow;
    case GroupColor::kGreen:
      return tab_groups::kTabGroupColorGreen;
    case GroupColor::kPink:
      return tab_groups::kTabGroupColorPink;
    case GroupColor::kPurple:
      return tab_groups::kTabGroupColorPurple;
    case GroupColor::kCyan:
      return tab_groups::kTabGroupColorCyan;
    case GroupColor::kOrange:
      return tab_groups::kTabGroupColorOrange;
    case GroupColor::kNumEntries:
      NOTREACHED_IN_MIGRATION() << "kNumEntries is not a supported color enum.";
      return tab_groups::kTabGroupColorGrey;
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

std::vector<std::unique_ptr<ash::DeskTemplate>>
ParseAdminTemplatesFromPolicyValue(const base::Value& value) {
  std::vector<std::unique_ptr<ash::DeskTemplate>> desk_templates;
  if (!value.is_list()) {
    return desk_templates;
  }

  for (const auto& desk_template : value.GetList()) {
    auto desk_template_ptr = ParseAdminTemplate(desk_template);
    if (desk_template_ptr == nullptr) {
      continue;
    }

    desk_templates.push_back(std::move(desk_template_ptr));
  }

  return desk_templates;
}

ParseSavedDeskResult ParseDeskTemplateFromBaseValue(
    const base::Value& value,
    ash::DeskTemplateSource source) {
  if (!value.is_dict()) {
    return base::unexpected(SavedDeskParseError::kBaseValueIsNotDict);
  }

  const base::Value::Dict& value_dict = value.GetDict();

  std::string created_time_usec_str;
  int64_t created_time_usec;
  std::string name;
  std::string updated_time_usec_str;
  int64_t updated_time_usec;
  std::string uuid_str;
  int version;
  const base::Value::Dict* desk = value_dict.FindDict(kDesk);
  if (!desk || !GetInt(value_dict, kVersion, &version) ||
      !GetString(value_dict, kUuid, &uuid_str) ||
      !GetString(value_dict, kName, &name) ||
      !GetString(value_dict, kCreatedTime, &created_time_usec_str) ||
      !base::StringToInt64(created_time_usec_str, &created_time_usec) ||
      !GetString(value_dict, kUpdatedTime, &updated_time_usec_str) ||
      !base::StringToInt64(updated_time_usec_str, &updated_time_usec) ||
      name.empty() || created_time_usec_str.empty() ||
      updated_time_usec_str.empty()) {
    return base::unexpected(SavedDeskParseError::kMissingRequiredFields);
  }

  base::Uuid uuid = base::Uuid::ParseCaseInsensitive(uuid_str);
  if (!uuid.is_valid()) {
    return base::unexpected(SavedDeskParseError::kInvalidUuid);
  }

  // Set default value for the desk type to template.
  std::string desk_type_string;
  if (!GetString(value_dict, kDeskType, &desk_type_string)) {
    desk_type_string = kDeskTypeTemplate;
  } else if (!IsValidDeskTemplateType(desk_type_string)) {
    return base::unexpected(SavedDeskParseError::kInvalidDeskType);
  }
  const ash::DeskTemplateType desk_type =
      GetDeskTypeFromString(desk_type_string);

  // If policy template set auto launch bool.
  bool auto_launch_on_startup = false;
  GetBool(value_dict, kAutoLaunchOnStartup, &auto_launch_on_startup);

  const base::Time created_time = ProtoTimeToTime(created_time_usec);
  const base::Time updated_time = ProtoTimeToTime(updated_time_usec);

  std::unique_ptr<ash::DeskTemplate> desk_template = nullptr;

  // Note: this method is responsible for parsing both regular and policy
  // templates after said policy templates are pushed to the device.
  if (auto* policy_value = value_dict.FindDict(kPolicy)) {
    desk_template = std::make_unique<ash::DeskTemplate>(
        std::move(uuid), source, name, created_time, desk_type,
        auto_launch_on_startup, base::Value(policy_value->Clone()));
  } else {
    desk_template = std::make_unique<ash::DeskTemplate>(
        std::move(uuid), source, name, created_time, desk_type);
  }

  if (desk_type == ash::DeskTemplateType::kSaveAndRecall) {
    std::string lacros_profile_id_str;
    if (GetString(value_dict, kLacrosProfileId, &lacros_profile_id_str)) {
      uint64_t lacros_profile_id = 0;
      if (base::StringToUint64(lacros_profile_id_str, &lacros_profile_id)) {
        desk_template->set_lacros_profile_id(lacros_profile_id);
      }
    }
  }

  desk_template->set_updated_time(updated_time);
  desk_template->set_desk_restore_data(ConvertJsonToRestoreData(desk));

  return base::ok(std::move(desk_template));
}

base::Value SerializeDeskTemplateAsBaseValue(
    const ash::DeskTemplate* desk,
    apps::AppRegistryCache* app_cache) {
  base::Value::Dict desk_dict;
  desk_dict.Set(kVersion, kVersionNum);
  desk_dict.Set(kUuid, desk->uuid().AsLowercaseString());
  desk_dict.Set(kName, desk->template_name());
  desk_dict.Set(kCreatedTime, base::TimeToValue(desk->created_time()));
  desk_dict.Set(kUpdatedTime, base::TimeToValue(desk->GetLastUpdatedTime()));
  desk_dict.Set(kDeskType, SerializeDeskTypeAsString(desk->type()));
  desk_dict.Set(kAutoLaunchOnStartup, desk->should_launch_on_startup());
  if (desk->type() == ash::DeskTemplateType::kSaveAndRecall &&
      desk->lacros_profile_id()) {
    desk_dict.Set(kLacrosProfileId,
                  base::NumberToString(desk->lacros_profile_id()));
  }
  desk_dict.Set(
      kDesk, ConvertRestoreDataToValue(desk->desk_restore_data(), app_cache));

  if (desk->policy_definition().type() == base::Value::Type::DICT) {
    desk_dict.Set(kPolicy, desk->policy_definition().Clone());
  }

  return base::Value(std::move(desk_dict));
}

std::unique_ptr<DeskTemplate> FromSyncProto(
    const sync_pb::WorkspaceDeskSpecifics& pb_entry) {
  base::Uuid uuid = base::Uuid::ParseCaseInsensitive(pb_entry.uuid());
  if (!uuid.is_valid())
    return nullptr;

  const base::Time created_time =
      ProtoTimeToTime(pb_entry.created_time_windows_epoch_micros());

  const ash::DeskTemplateType desk_type =
      pb_entry.has_desk_type()
          ? GetDeskTemplateTypeFromProtoType(pb_entry.desk_type())
          : ash::DeskTemplateType::kTemplate;

  if (desk_type == ash::DeskTemplateType::kUnknown) {
    return nullptr;
  }

  // Protobuf parsing enforces UTF-8 encoding for all strings.
  auto desk_template = std::make_unique<DeskTemplate>(
      std::move(uuid), ash::DeskTemplateSource::kUser, pb_entry.name(),
      created_time, desk_type);

  if (pb_entry.has_updated_time_windows_epoch_micros()) {
    desk_template->set_updated_time(
        ProtoTimeToTime(pb_entry.updated_time_windows_epoch_micros()));
  }
  desk_template->set_desk_restore_data(ConvertToRestoreData(pb_entry));
  if (pb_entry.has_client_cache_guid()) {
    desk_template->set_client_cache_guid(pb_entry.client_cache_guid());
  }
  if (pb_entry.has_device_form_factor()) {
    desk_template->set_device_form_factor(
        syncer::ToDeviceInfoFormFactor(pb_entry.device_form_factor()));
  } else {
    desk_template->set_device_form_factor(
        syncer::DeviceInfo::FormFactor::kUnknown);
  }
  return desk_template;
}

sync_pb::WorkspaceDeskSpecifics ToSyncProto(const DeskTemplate* desk_template,
                                            apps::AppRegistryCache* cache) {
  DCHECK(cache);

  sync_pb::WorkspaceDeskSpecifics pb_entry;
  FillDeskType(desk_template, &pb_entry);

  pb_entry.set_uuid(desk_template->uuid().AsLowercaseString());
  pb_entry.set_name(base::UTF16ToUTF8(desk_template->template_name()));
  pb_entry.set_created_time_windows_epoch_micros(
      TimeToProtoTime(desk_template->created_time()));
  if (desk_template->WasUpdatedSinceCreation()) {
    pb_entry.set_updated_time_windows_epoch_micros(
        TimeToProtoTime(desk_template->GetLastUpdatedTime()));
  }

  if (desk_template->desk_restore_data()) {
    FillWorkspaceDeskSpecifics(cache, desk_template->desk_restore_data(),
                               &pb_entry);
  }
  if (!desk_template->client_cache_guid().empty()) {
    pb_entry.set_client_cache_guid(desk_template->client_cache_guid());
  }
  pb_entry.set_device_form_factor(
      syncer::ToDeviceFormFactorProto(desk_template->device_form_factor()));
  return pb_entry;
}

}  // namespace desk_template_conversion

}  // namespace desks_storage
