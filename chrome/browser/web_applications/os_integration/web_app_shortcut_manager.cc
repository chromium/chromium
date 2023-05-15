// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/web_app_shortcut_manager.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/barrier_closure.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/os_integration/web_app_file_handler_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_protocol_handler_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"

#if BUILDFLAG(IS_MAC)
#include "base/system/sys_info.h"
#include "chrome/common/mac/app_mode_common.h"
#endif

namespace web_app {

namespace {

// UMA metric name for shortcuts creation result.
constexpr const char* kCreationResultMetric =
    "WebApp.Shortcuts.Creation.Result";

// Result of shortcuts creation process.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CreationResult {
  kSuccess = 0,
  kFailToCreateShortcut = 1,
  kMaxValue = kFailToCreateShortcut
};

WebAppShortcutManager::UpdateShortcutsForAllAppsCallback&
GetUpdateShortcutsForAllAppsCallback() {
  static base::NoDestructor<
      WebAppShortcutManager::UpdateShortcutsForAllAppsCallback>
      callback;
  return *callback;
}

#if BUILDFLAG(IS_MAC)
// This version number is stored in local prefs to check whether app shortcuts
// need to be recreated. This might happen when we change various aspects of app
// shortcuts like command-line flags or associated icons, binaries, etc.
const int kCurrentAppShortcutsVersion = APP_SHIM_VERSION_NUMBER;

// The architecture that was last used to create app shortcuts for this user
// directory.
std::string CurrentAppShortcutsArch() {
  return base::SysInfo::OperatingSystemArchitecture();
}
#else
// Non-mac platforms do not update shortcuts.
const int kCurrentAppShortcutsVersion = 0;
std::string CurrentAppShortcutsArch() {
  return "";
}
#endif

// Delay in seconds before running UpdateShortcutsForAllApps.
const int kUpdateShortcutsForAllAppsDelay = 10;

}  // namespace

void WebAppShortcutManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // Indicates whether app shortcuts have been created.
  registry->RegisterIntegerPref(prefs::kAppShortcutsVersion,
                                kCurrentAppShortcutsVersion);
  registry->RegisterStringPref(prefs::kAppShortcutsArch,
                               CurrentAppShortcutsArch());
}

WebAppShortcutManager::WebAppShortcutManager(
    Profile* profile,
    WebAppIconManager* icon_manager,
    WebAppFileHandlerManager* file_handler_manager,
    WebAppProtocolHandlerManager* protocol_handler_manager)
    : profile_(profile),
      icon_manager_(icon_manager),
      file_handler_manager_(file_handler_manager),
      protocol_handler_manager_(protocol_handler_manager) {}

WebAppShortcutManager::~WebAppShortcutManager() = default;

void WebAppShortcutManager::SetSubsystems(WebAppIconManager* icon_manager,
                                          WebAppRegistrar* registrar) {
  icon_manager_ = icon_manager;
  registrar_ = registrar;
}

void WebAppShortcutManager::Start() {
  UpdateShortcutsForAllAppsIfNeeded();
}

void WebAppShortcutManager::UpdateShortcuts(
    const AppId& app_id,
    base::StringPiece old_name,
    ResultCallback update_finished_callback) {
  DCHECK(CanCreateShortcuts());
  GetShortcutInfoForApp(
      app_id,
      base::BindOnce(
          &WebAppShortcutManager::OnShortcutInfoRetrievedUpdateShortcuts,
          weak_ptr_factory_.GetWeakPtr(), base::UTF8ToUTF16(old_name),
          std::move(update_finished_callback)));
}

void WebAppShortcutManager::GetAppExistingShortCutLocation(
    ShortcutLocationCallback callback,
    std::unique_ptr<ShortcutInfo> shortcut_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Ownership of |shortcut_info| moves to the Reply, which is guaranteed to
  // outlive the const reference.
  const ShortcutInfo& shortcut_info_ref = *shortcut_info;
  internals::GetShortcutIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&internals::GetAppExistingShortCutLocationImpl,
                     std::cref(shortcut_info_ref)),
      base::BindOnce(
          [](std::unique_ptr<ShortcutInfo> shortcut_info,
             ShortcutLocationCallback callback, ShortcutLocations locations) {
            DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
            shortcut_info.reset();
            std::move(callback).Run(locations);
          },
          std::move(shortcut_info), std::move(callback)));
}

void WebAppShortcutManager::SetUpdateShortcutsForAllAppsCallback(
    UpdateShortcutsForAllAppsCallback callback) {
  GetUpdateShortcutsForAllAppsCallback() = std::move(callback);
}

bool WebAppShortcutManager::CanCreateShortcuts() const {
#if BUILDFLAG(IS_CHROMEOS)
  return false;
#else
  return true;
#endif
}

void WebAppShortcutManager::SuppressShortcutsForTesting() {
  suppress_shortcuts_for_testing_ = true;
}

void WebAppShortcutManager::CreateShortcuts(const AppId& app_id,
                                            bool add_to_desktop,
                                            ShortcutCreationReason reason,
                                            CreateShortcutsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(CanCreateShortcuts());

  GetShortcutInfoForApp(
      app_id,
      base::BindOnce(
          &WebAppShortcutManager::OnShortcutInfoRetrievedCreateShortcuts,
          weak_ptr_factory_.GetWeakPtr(), add_to_desktop, reason,
          base::BindOnce(&WebAppShortcutManager::OnShortcutsCreated,
                         weak_ptr_factory_.GetWeakPtr(), app_id,
                         std::move(callback))));
}

void WebAppShortcutManager::DeleteShortcuts(
    const AppId& app_id,
    const base::FilePath& shortcuts_data_dir,
    std::unique_ptr<ShortcutInfo> shortcut_info,
    ResultCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(CanCreateShortcuts());

  internals::ScheduleDeletePlatformShortcuts(
      shortcuts_data_dir, std::move(shortcut_info),
      base::BindOnce(&WebAppShortcutManager::OnShortcutsDeleted,
                     weak_ptr_factory_.GetWeakPtr(), app_id,
                     std::move(callback)));
}

void WebAppShortcutManager::ReadAllShortcutsMenuIconsAndRegisterShortcutsMenu(
    const AppId& app_id,
    const std::vector<WebAppShortcutsMenuItemInfo>& shortcuts_menu_item_infos,
    ResultCallback callback) {
  icon_manager_->ReadAllShortcutsMenuIcons(
      app_id,
      base::BindOnce(
          &WebAppShortcutManager::OnShortcutsMenuIconsReadRegisterShortcutsMenu,
          weak_ptr_factory_.GetWeakPtr(), app_id, shortcuts_menu_item_infos,
          std::move(callback)));
}

void WebAppShortcutManager::RegisterShortcutsMenuWithOs(
    const AppId& app_id,
    const std::vector<WebAppShortcutsMenuItemInfo>& shortcuts_menu_item_infos,
    const ShortcutsMenuIconBitmaps& shortcuts_menu_icon_bitmaps,
    ResultCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!web_app::ShouldRegisterShortcutsMenuWithOs() ||
      suppress_shortcuts_for_testing_) {
    std::move(callback).Run(Result::kOk);
    return;
  }

  std::unique_ptr<ShortcutInfo> shortcut_info = BuildShortcutInfo(app_id);
  if (!shortcut_info) {
    std::move(callback).Run(Result::kError);
    return;
  }

  // |shortcut_data_dir| is located in per-app OS integration resources
  // directory. See GetOsIntegrationResourcesDirectoryForApp function for more
  // info.
  base::FilePath shortcut_data_dir =
      internals::GetShortcutDataDir(*shortcut_info);
  web_app::RegisterShortcutsMenuWithOs(
      shortcut_info->app_id, shortcut_info->profile_path, shortcut_data_dir,
      shortcuts_menu_item_infos, shortcuts_menu_icon_bitmaps,
      std::move(callback));
}

void WebAppShortcutManager::UnregisterShortcutsMenuWithOs(
    const AppId& app_id,
    ResultCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!web_app::ShouldRegisterShortcutsMenuWithOs()) {
    std::move(callback).Run(Result::kOk);
    return;
  }

  web_app::UnregisterShortcutsMenuWithOs(app_id, profile_->GetPath(),
                                         std::move(callback));
}

void WebAppShortcutManager::OnShortcutsCreated(const AppId& app_id,
                                               CreateShortcutsCallback callback,
                                               bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  UMA_HISTOGRAM_ENUMERATION(kCreationResultMetric,
                            success ? CreationResult::kSuccess
                                    : CreationResult::kFailToCreateShortcut);
  std::move(callback).Run(success);
}

void WebAppShortcutManager::OnShortcutsDeleted(const AppId& app_id,
                                               ResultCallback callback,
                                               bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::move(callback).Run(success ? Result::kOk : Result::kError);
}

void WebAppShortcutManager::OnShortcutInfoRetrievedCreateShortcuts(
    bool add_to_desktop,
    ShortcutCreationReason reason,
    CreateShortcutsCallback callback,
    std::unique_ptr<ShortcutInfo> info) {
  if (suppress_shortcuts_for_testing_) {
    std::move(callback).Run(/*shortcut_created=*/true);
    return;
  }

  if (info == nullptr) {
    std::move(callback).Run(/*shortcut_created=*/false);
    return;
  }

  base::FilePath shortcut_data_dir = internals::GetShortcutDataDir(*info);

  ShortcutLocations locations;
  locations.on_desktop = add_to_desktop;
  locations.applications_menu_location = APP_MENU_LOCATION_SUBDIR_CHROMEAPPS;

  internals::ScheduleCreatePlatformShortcuts(shortcut_data_dir, locations,
                                             reason, std::move(info),
                                             std::move(callback));
}

void WebAppShortcutManager::OnShortcutsMenuIconsReadRegisterShortcutsMenu(
    const AppId& app_id,
    const std::vector<WebAppShortcutsMenuItemInfo>& shortcuts_menu_item_infos,
    ResultCallback callback,
    ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps) {
  RegisterShortcutsMenuWithOs(app_id, shortcuts_menu_item_infos,
                              shortcuts_menu_icon_bitmaps, std::move(callback));
}

void WebAppShortcutManager::OnShortcutInfoRetrievedUpdateShortcuts(
    std::u16string old_name,
    ResultCallback update_finished_callback,
    std::unique_ptr<ShortcutInfo> shortcut_info) {
  if (suppress_shortcuts_for_testing_ || !shortcut_info) {
    std::move(update_finished_callback).Run(Result::kOk);
    return;
  }

  base::FilePath shortcut_data_dir =
      internals::GetShortcutDataDir(*shortcut_info);
  internals::PostShortcutIOTaskAndReplyWithResult(
      base::BindOnce(&internals::UpdatePlatformShortcuts,
                     std::move(shortcut_data_dir), std::move(old_name),
                     /*user_specified_locations=*/absl::nullopt),
      std::move(shortcut_info), std::move(update_finished_callback));
}

std::unique_ptr<ShortcutInfo> WebAppShortcutManager::BuildShortcutInfo(
    const AppId& app_id) {
  const WebApp* app = registrar_->GetAppById(app_id);
  DCHECK(app);
  return BuildShortcutInfoForWebApp(app);
}

void WebAppShortcutManager::GetShortcutInfoForApp(
    const AppId& app_id,
    GetShortcutInfoCallback callback) {
  const WebApp* app = registrar_->GetAppById(app_id);

  // app could be nullptr if registry profile is being deleted.
  if (!app) {
    std::move(callback).Run(nullptr);
    return;
  }

  // Build a common intersection between desired and downloaded icons.
  auto icon_sizes_in_px = base::STLSetIntersection<std::vector<SquareSizePx>>(
      app->downloaded_icon_sizes(IconPurpose::ANY),
      GetDesiredIconSizesForShortcut());

  DCHECK(icon_manager_);
  if (!icon_sizes_in_px.empty()) {
    icon_manager_->ReadIcons(app_id, IconPurpose::ANY, icon_sizes_in_px,
                             base::BindOnce(&WebAppShortcutManager::OnIconsRead,
                                            weak_ptr_factory_.GetWeakPtr(),
                                            app_id, std::move(callback)));
    return;
  }

  // If there is no single icon at the desired sizes, we will resize what we can
  // get.
  SquareSizePx desired_icon_size = GetDesiredIconSizesForShortcut().back();

  icon_manager_->ReadIconAndResize(
      app_id, IconPurpose::ANY, desired_icon_size,
      base::BindOnce(&WebAppShortcutManager::OnIconsRead,
                     weak_ptr_factory_.GetWeakPtr(), app_id,
                     std::move(callback)));
}

void WebAppShortcutManager::OnIconsRead(
    const AppId& app_id,
    GetShortcutInfoCallback callback,
    std::map<SquareSizePx, SkBitmap> icon_bitmaps) {
  // |icon_bitmaps| can be empty here if no icon found.
  const WebApp* app = registrar_->GetAppById(app_id);
  if (!app) {
    std::move(callback).Run(nullptr);
    return;
  }

  gfx::ImageFamily image_family;
  for (auto& size_and_bitmap : icon_bitmaps) {
    image_family.Add(gfx::ImageSkia(
        gfx::ImageSkiaRep(size_and_bitmap.second, /*scale=*/0.0f)));
  }

  // If the image failed to load, use the standard application icon.
  if (image_family.empty()) {
    SquareSizePx icon_size_in_px = GetDesiredIconSizesForShortcut().back();
    gfx::ImageSkia image_skia = CreateDefaultApplicationIcon(icon_size_in_px);
    image_family.Add(gfx::Image(image_skia));
  }

  std::unique_ptr<ShortcutInfo> shortcut_info = BuildShortcutInfoForWebApp(app);
  shortcut_info->favicon = std::move(image_family);

  std::move(callback).Run(std::move(shortcut_info));
}

std::unique_ptr<ShortcutInfo> WebAppShortcutManager::BuildShortcutInfoForWebApp(
    const WebApp* app) {
  auto shortcut_info = std::make_unique<ShortcutInfo>();

  shortcut_info->app_id = app->app_id();
  shortcut_info->url = app->start_url();
  shortcut_info->title =
      base::UTF8ToUTF16(registrar_->GetAppShortName(app->app_id()));
  shortcut_info->description =
      base::UTF8ToUTF16(registrar_->GetAppDescription(app->app_id()));
  shortcut_info->profile_path = profile_->GetPath();
  shortcut_info->profile_name =
      profile_->GetPrefs()->GetString(prefs::kProfileName);
  shortcut_info->is_multi_profile = true;

  if (const apps::FileHandlers* file_handlers =
          file_handler_manager_->GetEnabledFileHandlers(app->app_id())) {
    shortcut_info->file_handler_extensions =
        GetFileExtensionsFromFileHandlers(*file_handlers);
    shortcut_info->file_handler_mime_types =
        GetMimeTypesFromFileHandlers(*file_handlers);
  }

  std::vector<apps::ProtocolHandlerInfo> protocol_handlers =
      protocol_handler_manager_->GetAppProtocolHandlerInfos(app->app_id());
  for (const auto& protocol_handler : protocol_handlers) {
    if (!protocol_handler.protocol.empty()) {
      shortcut_info->protocol_handlers.emplace(protocol_handler.protocol);
    }
  }

#if BUILDFLAG(IS_LINUX)
  const std::vector<WebAppShortcutsMenuItemInfo>& shortcuts_menu_item_infos =
      app->shortcuts_menu_item_infos();
  DCHECK_LE(shortcuts_menu_item_infos.size(), kMaxApplicationDockMenuItems);
  for (const auto& shortcuts_menu_item_info : shortcuts_menu_item_infos) {
    if (!shortcuts_menu_item_info.name.empty() &&
        !shortcuts_menu_item_info.url.is_empty()) {
      // Generates ID from the name by replacing all characters that are not
      // numbers, letters, or '-' with '-'.
      std::string id = base::UTF16ToUTF8(shortcuts_menu_item_info.name);
      RE2::GlobalReplace(&id, "[^a-zA-Z0-9\\-]", "-");
      shortcut_info->actions.emplace(
          id, base::UTF16ToUTF8(shortcuts_menu_item_info.name),
          shortcuts_menu_item_info.url);
    }
  }
#endif  // BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_MAC)
  shortcut_info->handlers_per_profile =
      AppShimRegistry::Get()->GetHandlersForApp(app->app_id());
#endif

  return shortcut_info;
}

void WebAppShortcutManager::UpdateShortcutsForAllAppsIfNeeded() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Updating shortcuts writes to user home folders, which can not be done in
  // tests without exploding disk space usage on the bots.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType))
    return;

  int last_version =
      profile_->GetPrefs()->GetInteger(prefs::kAppShortcutsVersion);
  std::string last_arch =
      profile_->GetPrefs()->GetString(prefs::kAppShortcutsArch);

  if (last_version == kCurrentAppShortcutsVersion &&
      last_arch == CurrentAppShortcutsArch()) {
    // This either means this is a profile where installed shortcuts already
    // match the expected version and arch, or this could be a fresh profile.
    // For the latter, make sure to actually store version and arch in prefs,
    // as otherwise this code would always just read the defaults for these
    // prefs, and not actually ever detect a version change.
    SetCurrentAppShortcutsVersion();
    return;
  }

  content::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&WebAppShortcutManager::UpdateShortcutsForAllAppsNow,
                     weak_ptr_factory_.GetWeakPtr()),
      base::Seconds(kUpdateShortcutsForAllAppsDelay));
}

void WebAppShortcutManager::UpdateShortcutsForAllAppsNow() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (suppress_shortcuts_for_testing_)
    return;

  std::vector<AppId> app_ids = registrar_->GetAppIds();
  auto done_callback = base::BarrierClosure(
      app_ids.size() + 1,
      base::BindOnce(&WebAppShortcutManager::SetCurrentAppShortcutsVersion,
                     weak_ptr_factory_.GetWeakPtr()));

  for (const auto& app_id : app_ids) {
    UpdateShortcuts(app_id, /*old_name=*/{},
                    base::IgnoreArgs<Result>(done_callback));
  }

  UpdateShortcutsForAllAppsCallback update_callback =
      GetUpdateShortcutsForAllAppsCallback();
  if (update_callback) {
    update_callback.Run(profile_, done_callback);
  } else {
    done_callback.Run();
  }
}

void WebAppShortcutManager::SetCurrentAppShortcutsVersion() {
  profile_->GetPrefs()->SetInteger(prefs::kAppShortcutsVersion,
                                   kCurrentAppShortcutsVersion);
  profile_->GetPrefs()->SetString(prefs::kAppShortcutsArch,
                                  CurrentAppShortcutsArch());
}

}  // namespace web_app
