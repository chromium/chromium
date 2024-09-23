// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"

#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "base/atomic_ref_count.h"
#include "base/auto_reset.h"
#include "base/barrier_callback.h"
#include "base/barrier_closure.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/concurrent_closures.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/web_applications/os_integration/file_handling_sub_manager.h"
#include "chrome/browser/web_applications/os_integration/os_integration_sub_manager.h"
#include "chrome/browser/web_applications/os_integration/os_integration_test_override.h"
#include "chrome/browser/web_applications/os_integration/protocol_handling_sub_manager.h"
#include "chrome/browser/web_applications/os_integration/run_on_os_login_sub_manager.h"
#include "chrome/browser/web_applications/os_integration/shortcut_menu_handling_sub_manager.h"
#include "chrome/browser/web_applications/os_integration/shortcut_sub_manager.h"
#include "chrome/browser/web_applications/os_integration/uninstallation_via_os_settings_sub_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/os_integration/web_app_uninstallation_via_os_settings_registration.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/gfx/image/image_skia_rep_default.h"

#if BUILDFLAG(IS_MAC)
#include "base/system/sys_info.h"
#include "chrome/common/mac/app_mode_common.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/web_applications/os_integration/mac/app_shim_registry.h"
#endif

namespace web_app {

namespace {
base::AtomicRefCount& GetSuppressCount() {
  static base::AtomicRefCount g_ref_count;
  return g_ref_count;
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

OsIntegrationManager::UpdateShortcutsForAllAppsCallback&
GetUpdateShortcutsForAllAppsCallback() {
  static base::NoDestructor<
      OsIntegrationManager::UpdateShortcutsForAllAppsCallback>
      callback;
  return *callback;
}

}  // namespace

OsIntegrationManager::ScopedSuppressForTesting::ScopedSuppressForTesting() {
// Creating OS hooks on ChromeOS doesn't write files to disk, so it's
// unnecessary to suppress and it provides better crash coverage.
#if !BUILDFLAG(IS_CHROMEOS)
  GetSuppressCount().Increment();
#endif
}

OsIntegrationManager::ScopedSuppressForTesting::~ScopedSuppressForTesting() {
#if !BUILDFLAG(IS_CHROMEOS)
  CHECK(!GetSuppressCount().IsZero());
  GetSuppressCount().Decrement();
#endif
}

// static
bool OsIntegrationManager::AreOsHooksSuppressedForTesting() {
  return !GetSuppressCount().IsZero();
}

// static
void OsIntegrationManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // Indicates whether app shortcuts have been created.
  registry->RegisterIntegerPref(prefs::kAppShortcutsVersion,
                                kCurrentAppShortcutsVersion);
  registry->RegisterStringPref(prefs::kAppShortcutsArch,
                               CurrentAppShortcutsArch());
}

// static
void OsIntegrationManager::SetUpdateShortcutsForAllAppsCallback(
    UpdateShortcutsForAllAppsCallback callback) {
  GetUpdateShortcutsForAllAppsCallback() = std::move(callback);
}

// static
base::OnceClosure&
OsIntegrationManager::OnSetCurrentAppShortcutsVersionCallbackForTesting() {
  static base::NoDestructor<base::OnceClosure> callback;
  return *callback;
}

OsIntegrationManager::OsIntegrationManager(
    Profile* profile,
    std::unique_ptr<WebAppFileHandlerManager> file_handler_manager,
    std::unique_ptr<WebAppProtocolHandlerManager> protocol_handler_manager)
    : profile_(profile),
      file_handler_manager_(std::move(file_handler_manager)),
      protocol_handler_manager_(std::move(protocol_handler_manager)) {}

OsIntegrationManager::~OsIntegrationManager() = default;

void OsIntegrationManager::SetProvider(base::PassKey<WebAppProvider>,
                                       WebAppProvider& provider) {
  CHECK(!first_synchronize_called_);

  provider_ = &provider;

  base::PassKey<OsIntegrationManager> pass_key;
  file_handler_manager_->SetProvider(pass_key, provider);
  if (protocol_handler_manager_)
    protocol_handler_manager_->SetProvider(pass_key, provider);

  sub_managers_.clear();
  sub_managers_.push_back(
      std::make_unique<ShortcutSubManager>(*profile_, provider));
  sub_managers_.push_back(
      std::make_unique<FileHandlingSubManager>(profile_->GetPath(), provider));
  sub_managers_.push_back(std::make_unique<ProtocolHandlingSubManager>(
      profile_->GetPath(), provider));
  sub_managers_.push_back(std::make_unique<ShortcutMenuHandlingSubManager>(
      profile_->GetPath(), provider));
  sub_managers_.push_back(
      std::make_unique<RunOnOsLoginSubManager>(*profile_, provider));
  sub_managers_.push_back(
      std::make_unique<UninstallationViaOsSettingsSubManager>(
          profile_->GetPath(), provider));

  set_provider_called_ = true;
}

void OsIntegrationManager::Start() {
  CHECK(provider_);
  CHECK(file_handler_manager_);

  // Profile manager can be null in unit tests.
  if (ProfileManager* profile_manager = g_browser_process->profile_manager()) {
    profile_manager_observation_.Observe(profile_manager);
  }
  file_handler_manager_->Start();
  if (protocol_handler_manager_) {
    protocol_handler_manager_->Start();
  }
  UpdateShortcutsForAllAppsIfNeeded();
}

void OsIntegrationManager::Shutdown() {
  profile_manager_observation_.Reset();
}

void OsIntegrationManager::Synchronize(
    const webapps::AppId& app_id,
    base::OnceClosure callback,
    std::optional<SynchronizeOsOptions> options) {
  first_synchronize_called_ = true;

  // This is usually called to clean up OS integration states on the OS,
  // regardless of whether there are apps existing in the app registry or not.
  if (options.has_value() && options.value().force_unregister_os_integration) {
    CHECK_OS_INTEGRATION_ALLOWED();
    ForceUnregisterOsIntegrationOnSubManager(
        app_id, /*index=*/0,
        std::move(callback).Then(
            base::BindOnce(force_unregister_callback_for_testing_, app_id)));
    return;
  }

  // If the app does not exist in the DB and an unregistration is required, it
  // should have been done in the past Synchronize call.
  CHECK(provider_->registrar_unsafe().GetAppById(app_id))
      << "Can't perform OS integration without the app existing in the "
         "registrar. If the use-case requires an app to not be installed, "
         "consider setting the force_unregister_os_integration flag inside "
         "SynchronizeOsOptions";

  CHECK(set_provider_called_);

  if (sub_managers_.empty()) {
    std::move(callback).Run();
    return;
  }

  std::unique_ptr<proto::WebAppOsIntegrationState> desired_states =
      std::make_unique<proto::WebAppOsIntegrationState>();
  proto::WebAppOsIntegrationState* desired_states_ptr = desired_states.get();

  // Note: Sometimes the execute step is a no-op based on feature flags or if os
  // integration is disabled for testing. This logic is in the
  // StartSubManagerExecutionIfRequired method.
  base::RepeatingClosure configure_barrier;
  configure_barrier = base::BarrierClosure(
      sub_managers_.size(),
      base::BindOnce(&OsIntegrationManager::StartSubManagerExecutionIfRequired,
                     weak_ptr_factory_.GetWeakPtr(), app_id, options,
                     std::move(desired_states), std::move(callback)));

  for (const auto& sub_manager : sub_managers_) {
    // This dereference is safe because the barrier closure guarantees that it
    // will not be called until `configure_barrier` is called from each sub-
    // manager.
    sub_manager->Configure(app_id, *desired_states_ptr, configure_barrier);
  }
}

void OsIntegrationManager::GetAppExistingShortCutLocation(
    ShortcutLocationCallback callback,
    std::unique_ptr<ShortcutInfo> shortcut_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Ownership of `shortcut_info` moves to the Reply, which is guaranteed to
  // outlive the const reference.
  const ShortcutInfo& shortcut_info_ref = *shortcut_info;
  internals::GetShortcutIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&internals::GetAppExistingShortCutLocationImpl,
                     std::cref(shortcut_info_ref)),
      base::BindOnce(
          [](ShortcutLocationCallback callback, ShortcutLocations locations) {
            DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
            std::move(callback).Run(locations);
          },
          std::move(callback).Then(base::OnceClosure(
              base::DoNothingWithBoundArgs(std::move(shortcut_info))))));
}

void OsIntegrationManager::GetShortcutInfoForAppFromRegistrar(
    const webapps::AppId& app_id,
    GetShortcutInfoCallback callback) {
  const WebApp* app = provider_->registrar_unsafe().GetAppById(app_id);

  // app could be nullptr if registry profile is being deleted or the app is not
  // in the registry.
  if (!app) {
    std::move(callback).Run(nullptr);
    return;
  }

  // Build a common intersection between desired and downloaded icons.
  auto icon_sizes_in_px = base::STLSetIntersection<std::vector<SquareSizePx>>(
      app->downloaded_icon_sizes(IconPurpose::ANY),
      GetDesiredIconSizesForShortcut());

  if (!icon_sizes_in_px.empty()) {
    provider_->icon_manager().ReadIcons(
        app_id, IconPurpose::ANY, icon_sizes_in_px,
        base::BindOnce(&OsIntegrationManager::OnIconsRead,
                       weak_ptr_factory_.GetWeakPtr(), app_id,
                       std::move(callback)));
    return;
  }

  // If there is no single icon at the desired sizes, we will resize what we can
  // get.
  SquareSizePx desired_icon_size = GetDesiredIconSizesForShortcut().back();

  provider_->icon_manager().ReadIconAndResize(
      app_id, IconPurpose::ANY, desired_icon_size,
      base::BindOnce(&OsIntegrationManager::OnIconsRead,
                     weak_ptr_factory_.GetWeakPtr(), app_id,
                     std::move(callback)));
}

bool OsIntegrationManager::IsFileHandlingAPIAvailable(
    const webapps::AppId& app_id) {
  return true;
}

const apps::FileHandlers* OsIntegrationManager::GetEnabledFileHandlers(
    const webapps::AppId& app_id) const {
  CHECK(file_handler_manager_);
  return file_handler_manager_->GetEnabledFileHandlers(app_id);
}

std::optional<GURL> OsIntegrationManager::TranslateProtocolUrl(
    const webapps::AppId& app_id,
    const GURL& protocol_url) {
  if (!protocol_handler_manager_)
    return std::optional<GURL>();

  return protocol_handler_manager_->TranslateProtocolUrl(app_id, protocol_url);
}

std::vector<custom_handlers::ProtocolHandler>
OsIntegrationManager::GetAppProtocolHandlers(const webapps::AppId& app_id) {
  if (!protocol_handler_manager_)
    return std::vector<custom_handlers::ProtocolHandler>();

  return protocol_handler_manager_->GetAppProtocolHandlers(app_id);
}

std::vector<custom_handlers::ProtocolHandler>
OsIntegrationManager::GetAllowedHandlersForProtocol(
    const std::string& protocol) {
  if (!protocol_handler_manager_)
    return std::vector<custom_handlers::ProtocolHandler>();

  return protocol_handler_manager_->GetAllowedHandlersForProtocol(protocol);
}

std::vector<custom_handlers::ProtocolHandler>
OsIntegrationManager::GetDisallowedHandlersForProtocol(
    const std::string& protocol) {
  if (!protocol_handler_manager_)
    return std::vector<custom_handlers::ProtocolHandler>();

  return protocol_handler_manager_->GetDisallowedHandlersForProtocol(protocol);
}

WebAppProtocolHandlerManager&
OsIntegrationManager::protocol_handler_manager_for_testing() {
  CHECK(protocol_handler_manager_);
  return *protocol_handler_manager_;
}

FakeOsIntegrationManager* OsIntegrationManager::AsTestOsIntegrationManager() {
  return nullptr;
}

void OsIntegrationManager::OnProfileMarkedForPermanentDeletion(
    Profile* profile_to_be_deleted) {
  if (profile_ != profile_to_be_deleted) {
    return;
  }

  WebAppRegistrar& registrar = provider_->registrar_unsafe();

  for (const webapps::AppId& app_id : registrar.GetAppIds()) {
    UnregisterOsIntegrationOnProfileMarkedForDeletion(app_id);
  }
}

void OsIntegrationManager::OnProfileManagerDestroying() {
  profile_manager_observation_.Reset();
}

void OsIntegrationManager::SetForceUnregisterCalledForTesting(
    base::RepeatingCallback<void(const webapps::AppId&)> on_force_unregister) {
  force_unregister_callback_for_testing_ = on_force_unregister;
}

void OsIntegrationManager::StartSubManagerExecutionIfRequired(
    const webapps::AppId& app_id,
    std::optional<SynchronizeOsOptions> options,
    std::unique_ptr<proto::WebAppOsIntegrationState> desired_states,
    base::OnceClosure on_all_execution_done) {
  // The "execute" step is skipped in the following cases:
  // 1. The app is no longer in the registrar. The whole synchronize process is
  //    stopped here.
  // 2. The `g_suppress_os_hooks_for_testing_` flag is set.

  const WebApp* web_app = provider_->registrar_unsafe().GetAppById(app_id);
  if (!web_app) {
    std::move(on_all_execution_done).Run();
    return;
  }

  proto::WebAppOsIntegrationState* desired_states_ptr = desired_states.get();
  auto write_state_to_db = base::BindOnce(
      &OsIntegrationManager::WriteStateToDB, weak_ptr_factory_.GetWeakPtr(),
      app_id, std::move(desired_states), std::move(on_all_execution_done));

  if (AreOsHooksSuppressedForTesting()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(write_state_to_db));
    return;
  }

  ExecuteNextSubmanager(app_id, options, desired_states_ptr,
                        web_app->current_os_integration_states(), /*index=*/0,
                        std::move(write_state_to_db));
}

void OsIntegrationManager::ExecuteNextSubmanager(
    const webapps::AppId& app_id,
    std::optional<SynchronizeOsOptions> options,
    proto::WebAppOsIntegrationState* desired_state,
    const proto::WebAppOsIntegrationState current_state,
    size_t index,
    base::OnceClosure on_all_execution_done_db_write) {
  CHECK(index < sub_managers_.size());
  base::OnceClosure next_callback = base::OnceClosure();
  if (index == sub_managers_.size() - 1) {
    next_callback = std::move(on_all_execution_done_db_write);
  } else {
    next_callback = base::BindOnce(
        &OsIntegrationManager::ExecuteNextSubmanager,
        weak_ptr_factory_.GetWeakPtr(), app_id, options, desired_state,
        current_state, index + 1, std::move(on_all_execution_done_db_write));
  }
  sub_managers_[index]->Execute(app_id, options, *desired_state, current_state,
                                std::move(next_callback));
}

void OsIntegrationManager::WriteStateToDB(
    const webapps::AppId& app_id,
    std::unique_ptr<proto::WebAppOsIntegrationState> desired_states,
    base::OnceClosure callback) {
  // Exit early if the app is already uninstalled. We still need to write the
  // desired_states to the web_app DB during the uninstallation process since
  // that helps make decisions on whether the uninstallation went successfully
  // or not inside the RemoveWebAppJob.
  const WebApp* existing_app = provider_->registrar_unsafe().GetAppById(app_id);
  if (!existing_app) {
    std::move(callback).Run();
    return;
  }

  {
    ScopedRegistryUpdate update = provider_->sync_bridge_unsafe().BeginUpdate();
    WebApp* web_app = update->UpdateApp(app_id);
    CHECK(web_app);
    web_app->SetCurrentOsIntegrationStates(*desired_states.get());
  }

  std::move(callback).Run();
}

void OsIntegrationManager::UnregisterOsIntegrationOnProfileMarkedForDeletion(
    const webapps::AppId& app_id) {
  CHECK_OS_INTEGRATION_ALLOWED();
  // This is used to keep the profile from being deleted while doing a
  // ForceUnregister when profile deletion is started.
  auto profile_keep_alive = std::make_unique<ScopedProfileKeepAlive>(
      profile_, ProfileKeepAliveOrigin::kOsIntegrationForceUnregistration);
  ForceUnregisterOsIntegrationOnSubManager(
      app_id, 0,
      base::BindOnce(&OsIntegrationManager::SubManagersUnregistered,
                     weak_ptr_factory_.GetWeakPtr(), app_id,
                     std::move(profile_keep_alive)));
}

void OsIntegrationManager::SubManagersUnregistered(
    const webapps::AppId& app_id,
    std::unique_ptr<ScopedProfileKeepAlive> keep_alive) {
  force_unregister_callback_for_testing_.Run(app_id);
  keep_alive.reset();
}

void OsIntegrationManager::ForceUnregisterOsIntegrationOnSubManager(
    const webapps::AppId& app_id,
    size_t index,
    base::OnceClosure final_callback) {
  CHECK(index < sub_managers_.size());
  base::OnceClosure next_callback = base::OnceClosure();
  if (index == sub_managers_.size() - 1) {
    next_callback = std::move(final_callback);
  } else {
    next_callback = base::BindOnce(
        &OsIntegrationManager::ForceUnregisterOsIntegrationOnSubManager,
        weak_ptr_factory_.GetWeakPtr(), app_id, index + 1,
        std::move(final_callback));
  }
  sub_managers_[index]->ForceUnregister(app_id, std::move(next_callback));
}

void OsIntegrationManager::UpdateShortcutsForAllAppsIfNeeded() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Updating shortcuts writes to user home folders, which can not be done in
  // tests without exploding disk space usage on the bots.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType)) {
    return;
  }

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
      base::BindOnce(&OsIntegrationManager::UpdateShortcutsForAllAppsNow,
                     weak_ptr_factory_.GetWeakPtr()),
      base::Seconds(kUpdateShortcutsForAllAppsDelay));
}

void OsIntegrationManager::UpdateShortcutsForAllAppsNow() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::ConcurrentClosures concurrent;

  SynchronizeOsOptions options;
  options.force_update_shortcuts = true;

  for (const auto& app_id : provider_->registrar_unsafe().GetAppIds()) {
    Synchronize(app_id, concurrent.CreateClosure(), options);
  }

  UpdateShortcutsForAllAppsCallback update_callback =
      GetUpdateShortcutsForAllAppsCallback();
  if (update_callback) {
    update_callback.Run(profile_, concurrent.CreateClosure());
  } else {
    concurrent.CreateClosure().Run();
  }

  std::move(concurrent)
      .Done(base::BindOnce(&OsIntegrationManager::SetCurrentAppShortcutsVersion,
                           weak_ptr_factory_.GetWeakPtr()));
}

void OsIntegrationManager::SetCurrentAppShortcutsVersion() {
  profile_->GetPrefs()->SetInteger(prefs::kAppShortcutsVersion,
                                   kCurrentAppShortcutsVersion);
  profile_->GetPrefs()->SetString(prefs::kAppShortcutsArch,
                                  CurrentAppShortcutsArch());

  if (base::OnceClosure& callback =
          OnSetCurrentAppShortcutsVersionCallbackForTesting()) {
    std::move(callback).Run();
  }
}

void OsIntegrationManager::OnIconsRead(
    const webapps::AppId& app_id,
    GetShortcutInfoCallback callback,
    std::map<SquareSizePx, SkBitmap> icon_bitmaps) {
  const WebApp* app = provider_->registrar_unsafe().GetAppById(app_id);
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

// TODO(crbug.com/40250591): Merge into BuildShortcutInfoWithoutFavicon() for
// web_app_shortcut.cc.
std::unique_ptr<ShortcutInfo> OsIntegrationManager::BuildShortcutInfoForWebApp(
    const WebApp* app) {
  auto shortcut_info = std::make_unique<ShortcutInfo>();

  shortcut_info->app_id = app->app_id();
  shortcut_info->url = app->start_url();
  shortcut_info->title = base::UTF8ToUTF16(
      provider_->registrar_unsafe().GetAppShortName(app->app_id()));
  shortcut_info->description = base::UTF8ToUTF16(
      provider_->registrar_unsafe().GetAppDescription(app->app_id()));
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

}  // namespace web_app
