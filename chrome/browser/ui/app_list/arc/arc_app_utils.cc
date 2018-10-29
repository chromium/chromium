// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"

#include <memory>
#include <string>

#include "base/json/json_writer.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/arc/arc_migration_guide_notification.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/boot_phase_monitor/arc_boot_phase_monitor_bridge.h"
#include "chrome/browser/chromeos/arc/notification/arc_supervision_transition_notification.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/ash/launcher/arc_app_shelf_id.h"
#include "chrome/browser/ui/ash/launcher/arc_shelf_spinner_item_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/shelf_spinner_controller.h"
#include "chrome/common/pref_names.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/arc_util.h"
#include "components/arc/common/intent_helper.mojom.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/geometry/rect.h"

// Helper macro which returns the AppInstance.
#define GET_APP_INSTANCE(method_name)                                    \
  (arc::ArcServiceManager::Get()                                         \
       ? ARC_GET_INSTANCE_FOR_METHOD(                                    \
             arc::ArcServiceManager::Get()->arc_bridge_service()->app(), \
             method_name)                                                \
       : nullptr)

// Helper function which returns the IntentHelperInstance.
#define GET_INTENT_HELPER_INSTANCE(method_name)                    \
  (arc::ArcServiceManager::Get()                                   \
       ? ARC_GET_INSTANCE_FOR_METHOD(arc::ArcServiceManager::Get() \
                                         ->arc_bridge_service()    \
                                         ->intent_helper(),        \
                                     method_name)                  \
       : nullptr)

namespace arc {

namespace {

// TODO(djacobo): Evaluate to build these strings by using
// ArcIntentHelperBridge::AppendStringToIntentHelperPackageName.
// Intent helper strings.
constexpr char kIntentHelperClassName[] =
    "org.chromium.arc.intent_helper.SettingsReceiver";
constexpr char kSetInTouchModeIntent[] =
    "org.chromium.arc.intent_helper.SET_IN_TOUCH_MODE";
constexpr char kShowTalkbackSettingsIntent[] =
    "org.chromium.arc.intent_helper.SHOW_TALKBACK_SETTINGS";

constexpr char kAction[] = "action";
constexpr char kActionMain[] = "android.intent.action.MAIN";
constexpr char kCategory[] = "category";
constexpr char kCategoryLauncher[] = "android.intent.category.LAUNCHER";
constexpr char kComponent[] = "component";
constexpr char kEndSuffix[] = "end";
constexpr char kIntentPrefix[] = "#Intent";
constexpr char kLaunchFlags[] = "launchFlags";

constexpr char kAndroidClockAppId[] = "ddmmnabaeomoacfpfjgghfpocfolhjlg";
constexpr char kAndroidFilesAppId[] = "gmiohhmfhgfclpeacmdfancbipocempm";
constexpr char kAndroidCameraAppId[] = "goamfaniemdfcajgcmmflhchgkmbngka";
constexpr char kAndroidLegacyCameraAppId[] = "obfofkigjfamlldmipdegnjlcpincibc";
constexpr char kAndroidCameraMigrationAppId[] =
    "ngmkobaiicipbagcngcmilfkhejlnfci";
constexpr char kAndroidContactsAppId[] = "kipfkokfekalckplgaikemhghlbkgpfl";

constexpr char const* kAppIdsHiddenInLauncher[] = {
    kAndroidClockAppId,        kSettingsAppId,
    kAndroidFilesAppId,        kAndroidCameraAppId,
    kAndroidLegacyCameraAppId, kAndroidCameraMigrationAppId,
    kAndroidContactsAppId};

// Returns true if |event_flags| came from a mouse or touch event.
bool IsMouseOrTouchEventFromFlags(int event_flags) {
  return (event_flags & (ui::EF_LEFT_MOUSE_BUTTON | ui::EF_MIDDLE_MOUSE_BUTTON |
                         ui::EF_RIGHT_MOUSE_BUTTON | ui::EF_BACK_MOUSE_BUTTON |
                         ui::EF_FORWARD_MOUSE_BUTTON | ui::EF_FROM_TOUCH)) != 0;
}

bool Launch(content::BrowserContext* context,
            const std::string& app_id,
            const base::Optional<std::string>& intent,
            int event_flags,
            int64_t display_id) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(context);
  CHECK(prefs);

  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);
  if (!app_info) {
    VLOG(2) << "Cannot launch unavailable app: " << app_id << ".";
    return false;
  }

  if (!app_info->ready) {
    VLOG(2) << "Cannot launch not-ready app: " << app_id << ".";
    return false;
  }

  if (!app_info->launchable) {
    VLOG(2) << "Cannot launch non-launchable app: " << app_id << ".";
    return false;
  }

  if (app_info->suspended) {
    VLOG(2) << "Cannot launch suspended app: " << app_id << ".";
    return false;
  }

  arc::mojom::IntentHelperInstance* intent_helper_instance =
      GET_INTENT_HELPER_INSTANCE(SendBroadcast);
  if (intent_helper_instance) {
    base::DictionaryValue extras;
    extras.SetBoolean("inTouchMode", IsMouseOrTouchEventFromFlags(event_flags));
    std::string extras_string;
    base::JSONWriter::Write(extras, &extras_string);
    intent_helper_instance->SendBroadcast(
        kSetInTouchModeIntent,
        ArcIntentHelperBridge::kArcIntentHelperPackageName,
        kIntentHelperClassName, extras_string);
  }

  if (app_info->shortcut || intent.has_value()) {
    const std::string intent_uri = intent.value_or(app_info->intent_uri);
    if (auto* app_instance = GET_APP_INSTANCE(LaunchIntent)) {
      app_instance->LaunchIntent(intent_uri, display_id);
    } else if (auto* app_instance = GET_APP_INSTANCE(LaunchIntentDeprecated)) {
      app_instance->LaunchIntentDeprecated(intent_uri, gfx::Rect());
    } else {
      return false;
    }
  } else {
    if (auto* app_instance = GET_APP_INSTANCE(LaunchApp)) {
      app_instance->LaunchApp(app_info->package_name, app_info->activity,
                              display_id);
    } else if (auto* app_instance = GET_APP_INSTANCE(LaunchAppDeprecated)) {
      app_instance->LaunchAppDeprecated(app_info->package_name,
                                        app_info->activity, gfx::Rect());
    } else {
      return false;
    }
  }
  prefs->SetLastLaunchTime(app_id);

  return true;
}

// Returns primary display id if |display_id| is invalid.
int64_t GetValidDisplayId(int64_t display_id) {
  if (display_id != display::kInvalidDisplayId)
    return display_id;
  if (auto* screen = display::Screen::GetScreen())
    return screen->GetPrimaryDisplay().id();
  return display::kInvalidDisplayId;
}

}  // namespace

const char kGoogleDuo[] = "djkcbcmkefiiphjkonbeknmcgiheajce";
const char kInfinitePainter[] = "afihfgfghkmdmggakhkgnfhlikhdpima";
const char kLightRoom[] = "fpegfnbgomakooccabncdaelhfppceni";
const char kPlayStoreAppId[] = "cnbgggchhmkkdmeppjobngjoejnihlei";
const char kPlayBooksAppId[] = "cafegjnmmjpfibnlddppihpnkbkgicbg";
const char kPlayGamesAppId[] = "nplnnjkbeijcggmpdcecpabgbjgeiedc";
const char kPlayMoviesAppId[] = "dbbihmicnlldbflflckpafphlekmjfnm";
const char kPlayMusicAppId[] = "ophbaopahelaolbjliokocojjbgfadfn";
const char kPlayStorePackage[] = "com.android.vending";
const char kPlayStoreActivity[] = "com.android.vending.AssetBrowserActivity";
const char kSettingsAppId[] = "mconboelelhjpkbdhhiijkgcimoangdj";
const char kInitialStartParam[] = "S.org.chromium.arc.start_type=initialStart";
constexpr char kSettingsAppPackage[] = "com.android.settings";
const char kSettingsAppDomainUrlActivity[] =
    "com.android.settings.Settings$ManageDomainUrlsActivity";

bool ShouldShowInLauncher(const std::string& app_id) {
  for (auto* const id : kAppIdsHiddenInLauncher) {
    if (id == app_id)
      return false;
  }
  return true;
}

bool LaunchAndroidSettingsApp(content::BrowserContext* context,
                              int event_flags,
                              int64_t display_id) {
  return LaunchApp(context, kSettingsAppId, event_flags,
                   UserInteractionType::APP_STARTED_FROM_SETTINGS, display_id);
}

bool LaunchPlayStoreWithUrl(const std::string& url) {
  arc::mojom::IntentHelperInstance* instance =
      GET_INTENT_HELPER_INSTANCE(HandleUrl);
  if (!instance) {
    VLOG(1) << "Cannot find a mojo instance, ARC is unreachable or mojom"
            << " version mismatch";
    return false;
  }
  instance->HandleUrl(url, kPlayStorePackage);
  return true;
}

bool LaunchApp(content::BrowserContext* context,
               const std::string& app_id,
               int event_flags,
               arc::UserInteractionType user_action) {
  return LaunchApp(context, app_id, event_flags, user_action,
                   display::kInvalidDisplayId);
}

bool LaunchApp(content::BrowserContext* context,
               const std::string& app_id,
               int event_flags,
               arc::UserInteractionType user_action,
               int64_t display_id) {
  return LaunchAppWithIntent(context, app_id, base::nullopt /* launch_intent */,
                             event_flags, user_action, display_id);
}

bool LaunchAppWithIntent(content::BrowserContext* context,
                         const std::string& app_id,
                         const base::Optional<std::string>& launch_intent,
                         int event_flags,
                         arc::UserInteractionType user_action,
                         int64_t display_id) {
  DCHECK(!launch_intent.has_value() || !launch_intent->empty());
  if (user_action != UserInteractionType::NOT_USER_INITIATED)
    UMA_HISTOGRAM_ENUMERATION("Arc.UserInteraction", user_action);

  Profile* const profile = Profile::FromBrowserContext(context);

  // Even when ARC is not allowed for the profile, ARC apps may still show up
  // as a placeholder to show the guide notification for proper configuration.
  // Handle such a case here and shows the desired notification.
  if (IsArcBlockedDueToIncompatibleFileSystem(profile)) {
    VLOG(1) << "Attempt to launch " << app_id
            << " while ARC++ is blocked due to incompatible file system.";
    arc::ShowArcMigrationGuideNotification(profile);
    return false;
  }

  // In case supervision transition is in progress ARC++ is not available.
  const ArcSupervisionTransition supervision_transition =
      GetSupervisionTransition(profile);
  if (supervision_transition != ArcSupervisionTransition::NO_TRANSITION) {
    VLOG(1) << "Attempt to launch " << app_id
            << " while supervision transition " << supervision_transition
            << " is in progress.";
    arc::ShowSupervisionTransitionNotification(profile);
    return false;
  }

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(context);
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);
  if (app_info && !app_info->ready) {
    if (!IsArcPlayStoreEnabledForProfile(profile)) {
      if (prefs->IsDefault(app_id)) {
        // The setting can fail if the preference is managed.  However, the
        // caller is responsible to not call this function in such case.  DCHECK
        // is here to prevent possible mistake.
        SetArcPlayStoreEnabledForProfile(profile, true);
        DCHECK(IsArcPlayStoreEnabledForProfile(profile));

        // PlayStore item has special handling for shelf controllers. In order
        // to avoid unwanted initial animation for PlayStore item do not create
        // deferred launch request when PlayStore item enables Google Play
        // Store.
        if (app_id == kPlayStoreAppId) {
          prefs->SetLastLaunchTime(app_id);
          return true;
        }
      } else {
        // Only reachable when ARC always starts.
        DCHECK(arc::ShouldArcAlwaysStart());
      }
    } else {
      // Handle the case when default app tries to re-activate OptIn flow.
      if (IsArcPlayStoreEnabledPreferenceManagedForProfile(profile) &&
          !ArcSessionManager::Get()->enable_requested() &&
          prefs->IsDefault(app_id)) {
        SetArcPlayStoreEnabledForProfile(profile, true);
        // PlayStore item has special handling for shelf controllers. In order
        // to avoid unwanted initial animation for PlayStore item do not create
        // deferred launch request when PlayStore item enables Google Play
        // Store.
        if (app_id == kPlayStoreAppId) {
          prefs->SetLastLaunchTime(app_id);
          return true;
        }
      }
    }

    arc::ArcBootPhaseMonitorBridge::RecordFirstAppLaunchDelayUMA(context);
    ChromeLauncherController* chrome_controller =
        ChromeLauncherController::instance();
    // chrome_controller may be null in tests.
    if (chrome_controller) {
      chrome_controller->GetShelfSpinnerController()->AddSpinnerToShelf(
          app_id, std::make_unique<ArcShelfSpinnerItemController>(
                      app_id, event_flags, GetValidDisplayId(display_id)));

      // On some boards, ARC is booted with a restricted set of resources by
      // default to avoid slowing down Chrome's user session restoration.
      // However, the restriction should be lifted once the user explicitly
      // tries to launch an ARC app.
      SetArcCpuRestriction(false);
    }
    prefs->SetLastLaunchTime(app_id);
    return true;
  }

  arc::ArcBootPhaseMonitorBridge::RecordFirstAppLaunchDelayUMA(context);
  return Launch(context, app_id, launch_intent, event_flags,
                GetValidDisplayId(display_id));
}

bool LaunchAppShortcutItem(content::BrowserContext* context,
                           const std::string& app_id,
                           const std::string& shortcut_id,
                           int64_t display_id) {
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      ArcAppListPrefs::Get(context)->GetApp(app_id);
  if (!app_info) {
    LOG(ERROR) << "App " << app_id << " is not available.";
    return false;
  }

  mojom::AppInstance* app_instance =
      ArcServiceManager::Get()
          ? ARC_GET_INSTANCE_FOR_METHOD(
                ArcServiceManager::Get()->arc_bridge_service()->app(),
                LaunchAppShortcutItem)
          : nullptr;

  if (!app_instance) {
    LOG(ERROR) << "Cannot find a mojo instance, ARC is unreachable or mojom"
               << " version mismatch.";
    return false;
  }

  app_instance->LaunchAppShortcutItem(app_info->package_name, shortcut_id,
                                      GetValidDisplayId(display_id));
  return true;
}

bool LaunchSettingsAppActivity(content::BrowserContext* context,
                               const std::string& activity,
                               int event_flags,
                               int64_t display_id) {
  const std::string launch_intent = GetLaunchIntent(
      kSettingsAppPackage, activity, std::vector<std::string>());
  return LaunchAppWithIntent(
      context, kSettingsAppId, launch_intent, event_flags,
      UserInteractionType::APP_STARTED_FROM_SETTINGS, display_id);
}

void SetTaskActive(int task_id) {
  arc::mojom::AppInstance* app_instance = GET_APP_INSTANCE(SetTaskActive);
  if (!app_instance)
    return;
  app_instance->SetTaskActive(task_id);
}

void CloseTask(int task_id) {
  arc::mojom::AppInstance* app_instance = GET_APP_INSTANCE(CloseTask);
  if (!app_instance)
    return;
  app_instance->CloseTask(task_id);
}

void ShowTalkBackSettings() {
  arc::mojom::IntentHelperInstance* intent_helper_instance =
      GET_INTENT_HELPER_INSTANCE(SendBroadcast);
  if (!intent_helper_instance)
    return;

  intent_helper_instance->SendBroadcast(
      kShowTalkbackSettingsIntent,
      ArcIntentHelperBridge::kArcIntentHelperPackageName,
      kIntentHelperClassName, "{}");
}

void StartPaiFlow() {
  arc::mojom::AppInstance* app_instance = GET_APP_INSTANCE(StartPaiFlow);
  if (!app_instance)
    return;
  app_instance->StartPaiFlow();
}

std::vector<std::string> GetSelectedPackagesFromPrefs(
    content::BrowserContext* context) {
  std::vector<std::string> packages;
  const Profile* const profile = Profile::FromBrowserContext(context);
  const PrefService* prefs = profile->GetPrefs();

  const base::ListValue* selected_package_prefs =
      prefs->GetList(arc::prefs::kArcFastAppReinstallPackages);
  for (const base::Value& item : selected_package_prefs->GetList()) {
    std::string item_str;
    item.GetAsString(&item_str);
    packages.push_back(std::move(item_str));
  }

  return packages;
}

void StartFastAppReinstallFlow(const std::vector<std::string>& package_names) {
  arc::mojom::AppInstance* app_instance =
      GET_APP_INSTANCE(StartFastAppReinstallFlow);
  if (!app_instance) {
    LOG(ERROR) << "Failed to start Fast App Reinstall flow because app "
                  "instance is not connected.";
    return;
  }
  app_instance->StartFastAppReinstallFlow(package_names);
}

void UninstallPackage(const std::string& package_name) {
  VLOG(2) << "Uninstalling " << package_name;

  arc::mojom::AppInstance* app_instance = GET_APP_INSTANCE(UninstallPackage);
  if (!app_instance)
    return;

  app_instance->UninstallPackage(package_name);
}

void UninstallArcApp(const std::string& app_id, Profile* profile) {
  ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(profile);
  DCHECK(arc_prefs);
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      arc_prefs->GetApp(app_id);
  if (!app_info) {
    VLOG(2) << "Package being uninstalled does not exist: " << app_id << ".";
    return;
  }
  // For shortcut we just remove the shortcut instead of the package.
  if (app_info->shortcut)
    arc_prefs->RemoveApp(app_id);
  else
    UninstallPackage(app_info->package_name);
}

void RemoveCachedIcon(const std::string& icon_resource_id) {
  VLOG(2) << "Removing icon " << icon_resource_id;

  arc::mojom::AppInstance* app_instance = GET_APP_INSTANCE(RemoveCachedIcon);
  if (!app_instance)
    return;

  app_instance->RemoveCachedIcon(icon_resource_id);
}

bool ShowPackageInfo(const std::string& package_name,
                     mojom::ShowPackageInfoPage page,
                     int64_t display_id) {
  VLOG(2) << "Showing package info for " << package_name;

  if (auto* app_instance = GET_APP_INSTANCE(ShowPackageInfoOnPage)) {
    app_instance->ShowPackageInfoOnPage(package_name, page, display_id);
    return true;
  }

  if (auto* app_instance = GET_APP_INSTANCE(ShowPackageInfoOnPageDeprecated)) {
    app_instance->ShowPackageInfoOnPageDeprecated(package_name, page,
                                                  gfx::Rect());
    return true;
  }

  if (auto* app_instance = GET_APP_INSTANCE(ShowPackageInfoDeprecated)) {
    app_instance->ShowPackageInfoDeprecated(package_name, gfx::Rect());
    return true;
  }

  return false;
}

bool IsArcItem(content::BrowserContext* context, const std::string& id) {
  DCHECK(context);

  // Some unit tests use empty ids, some app ids are not valid ARC app ids.
  const ArcAppShelfId arc_app_shelf_id = ArcAppShelfId::FromString(id);
  if (!arc_app_shelf_id.valid())
    return false;

  const ArcAppListPrefs* const arc_prefs = ArcAppListPrefs::Get(context);
  if (!arc_prefs)
    return false;

  return arc_prefs->IsRegistered(arc_app_shelf_id.app_id());
}

std::string GetLaunchIntent(const std::string& package_name,
                            const std::string& activity,
                            const std::vector<std::string>& extra_params) {
  std::string extra_params_extracted;
  for (const auto& extra_param : extra_params) {
    extra_params_extracted += extra_param;
    extra_params_extracted += ";";
  }

  // Remove the |package_name| prefix, if activity starts with it.
  const char* activity_compact_name =
      activity.find(package_name.c_str()) == 0
          ? activity.c_str() + package_name.length()
          : activity.c_str();

  // Construct a string in format:
  // #Intent;action=android.intent.action.MAIN;
  //         category=android.intent.category.LAUNCHER;
  //         launchFlags=0x10210000;
  //         component=package_name/activity;
  //         param1;param2;end
  return base::StringPrintf(
      "%s;%s=%s;%s=%s;%s=0x%x;%s=%s/%s;%s%s", kIntentPrefix, kAction,
      kActionMain, kCategory, kCategoryLauncher, kLaunchFlags,
      Intent::FLAG_ACTIVITY_NEW_TASK |
          Intent::FLAG_ACTIVITY_RESET_TASK_IF_NEEDED,
      kComponent, package_name.c_str(), activity_compact_name,
      extra_params_extracted.c_str(), kEndSuffix);
}

bool ParseIntent(const std::string& intent_as_string, Intent* intent) {
  DCHECK(intent);
  const std::vector<base::StringPiece> parts = base::SplitStringPiece(
      intent_as_string, ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (parts.size() < 2 || parts.front() != kIntentPrefix ||
      parts.back() != kEndSuffix) {
    DVLOG(1) << "Failed to split intent " << intent_as_string << ".";
    return false;
  }

  for (size_t i = 1; i < parts.size() - 1; ++i) {
    const size_t separator = parts[i].find('=');
    if (separator == std::string::npos) {
      intent->AddExtraParam(parts[i].as_string());
      continue;
    }
    const base::StringPiece key = parts[i].substr(0, separator);
    const base::StringPiece value = parts[i].substr(separator + 1);
    if (key == kAction) {
      intent->set_action(value.as_string());
    } else if (key == kCategory) {
      intent->set_category(value.as_string());
    } else if (key == kLaunchFlags) {
      uint32_t launch_flags;
      const bool parsed =
          base::HexStringToUInt(value.as_string(), &launch_flags);
      if (!parsed) {
        DVLOG(1) << "Failed to parse launchFlags: " << value.as_string() << ".";
        return false;
      }
      intent->set_launch_flags(launch_flags);
    } else if (key == kComponent) {
      const size_t component_separator = value.find('/');
      if (component_separator == std::string::npos)
        return false;
      intent->set_package_name(
          value.substr(0, component_separator).as_string());
      const base::StringPiece activity_compact_name =
          value.substr(component_separator + 1);
      if (!activity_compact_name.empty() && activity_compact_name[0] == '.') {
        std::string activity = value.substr(0, component_separator).as_string();
        activity += activity_compact_name.as_string();
        intent->set_activity(activity);
      } else {
        intent->set_activity(activity_compact_name.as_string());
      }
    } else {
      intent->AddExtraParam(parts[i].as_string());
    }
  }

  return true;
}

void GetLocaleAndPreferredLanguages(const Profile* profile,
                                    std::string* out_locale,
                                    std::string* out_preferred_languages) {
  const PrefService::Preference* locale_pref =
      profile->GetPrefs()->FindPreference(
          ::language::prefs::kApplicationLocale);
  DCHECK(locale_pref);
  const bool value_exists = locale_pref->GetValue()->GetAsString(out_locale);
  DCHECK(value_exists);
  if (out_locale->empty())
    *out_locale = g_browser_process->GetApplicationLocale();

  // |preferredLanguages| consists of comma separated locale strings. It may be
  // empty or contain empty items, but those are ignored on ARC.  If an item
  // has no country code, it is derived in ARC.  In such a case, it may
  // conflict with another item in the list, then these will be dedupped (the
  // first one is taken) in ARC.
  *out_preferred_languages =
      profile->GetPrefs()->GetString(::prefs::kLanguagePreferredLanguages);
}

Intent::Intent() = default;

Intent::~Intent() = default;

void Intent::AddExtraParam(const std::string& extra_param) {
  extra_params_.push_back(extra_param);
}

bool Intent::HasExtraParam(const std::string& extra_param) const {
  return base::ContainsValue(extra_params_, extra_param);
}

}  // namespace arc
