// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/os_integration_manager.h"

#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_app_shortcut.h"
#include "chrome/browser/web_applications/components/web_app_ui_manager.h"
#include "chrome/browser/web_applications/components/web_app_uninstallation_via_os_settings_registration.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_MAC)
#include "chrome/browser/web_applications/components/app_shim_registry_mac.h"
#endif

namespace {
// Used to disable os hooks globally when OsIntegrationManager::SuppressOsHooks
// can't be easily used.
bool g_suppress_os_hooks_for_testing_ = false;
}  // namespace

namespace web_app {

// This barrier is designed to accumulate errors from calls to OS hook
// operations, and call the completion callback when all OS hook operations
// have completed. The |callback| is called when all copies of this object and
// all callbacks created using this object are destroyed.
class OsIntegrationManager::OsHooksBarrier
    : public base::RefCounted<OsHooksBarrier> {
 public:
  explicit OsHooksBarrier(OsHooksResults results_default,
                          InstallOsHooksCallback callback)
      : results_(results_default), callback_(std::move(callback)) {}

  void OnError(OsHookType::Type type) { AddResult(type, false); }

  base::OnceCallback<void(bool)> CreateBarrierCallbackForType(
      OsHookType::Type type) {
    return base::BindOnce(&OsHooksBarrier::AddResult, this, type);
  }

 private:
  friend class base::RefCounted<OsHooksBarrier>;

  ~OsHooksBarrier() {
    DCHECK(callback_);
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_), std::move(results_)));
  }

  void AddResult(OsHookType::Type type, bool result) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    results_[type] = result;
  }

  OsHooksResults results_;
  InstallOsHooksCallback callback_;
};

InstallOsHooksOptions::InstallOsHooksOptions() = default;
InstallOsHooksOptions::InstallOsHooksOptions(
    const InstallOsHooksOptions& other) = default;

OsIntegrationManager::OsIntegrationManager(
    Profile* profile,
    std::unique_ptr<AppShortcutManager> shortcut_manager,
    std::unique_ptr<FileHandlerManager> file_handler_manager,
    std::unique_ptr<ProtocolHandlerManager> protocol_handler_manager,
    std::unique_ptr<UrlHandlerManager> url_handler_manager)
    : profile_(profile),
      shortcut_manager_(std::move(shortcut_manager)),
      file_handler_manager_(std::move(file_handler_manager)),
      protocol_handler_manager_(std::move(protocol_handler_manager)),
      url_handler_manager_(std::move(url_handler_manager)) {}

OsIntegrationManager::~OsIntegrationManager() = default;

void OsIntegrationManager::SetSubsystems(AppRegistrar* registrar,
                                         WebAppUiManager* ui_manager,
                                         AppIconManager* icon_manager) {
  registrar_ = registrar;
  ui_manager_ = ui_manager;
  file_handler_manager_->SetSubsystems(registrar);
  shortcut_manager_->SetSubsystems(icon_manager, registrar);
  if (protocol_handler_manager_)
    protocol_handler_manager_->SetSubsystems(registrar);
  if (url_handler_manager_)
    url_handler_manager_->SetSubsystems(registrar);
}

void OsIntegrationManager::Start() {
  DCHECK(registrar_);
  DCHECK(file_handler_manager_);

#if defined(OS_MAC)
  // Ensure that all installed apps are included in the AppShimRegistry when the
  // profile is loaded. This is redundant, because apps are registered when they
  // are installed. It is necessary, however, because app registration was added
  // long after app installation launched. This should be removed after shipping
  // for a few versions (whereupon it may be assumed that most applications have
  // been registered).
  std::vector<AppId> app_ids = registrar_->GetAppIds();
  for (const auto& app_id : app_ids) {
    AppShimRegistry::Get()->OnAppInstalledForProfile(app_id,
                                                     profile_->GetPath());
  }
#endif
  file_handler_manager_->Start();
  if (protocol_handler_manager_)
    protocol_handler_manager_->Start();
}

void OsIntegrationManager::InstallOsHooks(
    const AppId& app_id,
    InstallOsHooksCallback callback,
    std::unique_ptr<WebApplicationInfo> web_app_info,
    InstallOsHooksOptions options) {
  if (g_suppress_os_hooks_for_testing_) {
    OsHooksResults os_hooks_results{true};
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), os_hooks_results));
    return;
  }
  MacAppShimOnAppInstalledForProfile(app_id);

  scoped_refptr<OsHooksBarrier> barrier = base::MakeRefCounted<OsHooksBarrier>(
      options.os_hooks, std::move(callback));

  DCHECK(options.os_hooks[OsHookType::kShortcuts] ||
         !options.os_hooks[OsHookType::kShortcutsMenu])
      << "Cannot install shortcuts menu without installing shortcuts.";

  auto shortcuts_callback = base::BindOnce(
      &OsIntegrationManager::OnShortcutsCreated, weak_ptr_factory_.GetWeakPtr(),
      app_id, std::move(web_app_info), options, barrier);

  // TODO(ortuno): Make adding a shortcut to the applications menu independent
  // from adding a shortcut to desktop.
  if (options.os_hooks[OsHookType::kShortcuts]) {
    CreateShortcuts(app_id, options.add_to_desktop,
                    std::move(shortcuts_callback));
  } else {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(shortcuts_callback),
                                  /*shortcuts_created=*/false));
  }
}

void OsIntegrationManager::UninstallAllOsHooks(
    const AppId& app_id,
    UninstallOsHooksCallback callback) {
  OsHooksResults os_hooks;
  os_hooks.set();
  UninstallOsHooks(app_id, os_hooks, std::move(callback));
}

void OsIntegrationManager::UninstallOsHooks(const AppId& app_id,
                                            const OsHooksResults& os_hooks,
                                            UninstallOsHooksCallback callback) {
  if (g_suppress_os_hooks_for_testing_) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), os_hooks));
    return;
  }

  scoped_refptr<OsHooksBarrier> barrier =
      base::MakeRefCounted<OsHooksBarrier>(os_hooks, std::move(callback));

  if (os_hooks[OsHookType::kShortcutsMenu]) {
    bool success = UnregisterShortcutsMenu(app_id);
    if (!success)
      barrier->OnError(OsHookType::kShortcutsMenu);
  }

  if (os_hooks[OsHookType::kShortcuts] || os_hooks[OsHookType::kRunOnOsLogin]) {
    std::unique_ptr<ShortcutInfo> shortcut_info = BuildShortcutInfo(app_id);
    base::FilePath shortcut_data_dir =
        internals::GetShortcutDataDir(*shortcut_info);

    if (os_hooks[OsHookType::kRunOnOsLogin] &&
        base::FeatureList::IsEnabled(features::kDesktopPWAsRunOnOsLogin)) {
      UnregisterRunOnOsLogin(
          app_id, shortcut_info->profile_path, shortcut_info->title,
          barrier->CreateBarrierCallbackForType(OsHookType::kRunOnOsLogin));
    }

    if (os_hooks[OsHookType::kShortcuts]) {
      DeleteShortcuts(
          app_id, shortcut_data_dir, std::move(shortcut_info),
          barrier->CreateBarrierCallbackForType(OsHookType::kShortcuts));
    }
  }
  // TODO(https://crbug.com/1108109) we should return the result of file handler
  // unregistration and record errors during unregistration.
  // TODO(crbug.com/1076688): Retrieve shortcuts before they're unregistered.
  if (os_hooks[OsHookType::kFileHandlers])
    UnregisterFileHandlers(app_id, nullptr, base::DoNothing());

  // TODO(https://crbug.com/1108109) we should return the result of protocol
  // handler unregistration and record errors during unregistration.
  if (os_hooks[OsHookType::kProtocolHandlers])
    UnregisterProtocolHandlers(app_id);

  if (os_hooks[OsHookType::kUrlHandlers])
    UnregisterUrlHandlers(app_id);

  // There is a chance uninstallation point was created with feature flag
  // enabled so we need to clean it up regardless of feature flag state.
  if (os_hooks[OsHookType::kUninstallationViaOsSettings])
    UnregisterWebAppOsUninstallation(app_id);
}

void OsIntegrationManager::UpdateOsHooks(
    const AppId& app_id,
    base::StringPiece old_name,
    std::unique_ptr<ShortcutInfo> old_shortcut,
    bool file_handlers_need_os_update,
    const WebApplicationInfo& web_app_info) {
  if (g_suppress_os_hooks_for_testing_)
    return;

  if (file_handlers_need_os_update)
    UpdateFileHandlers(app_id, std::move(old_shortcut));

  UpdateShortcuts(app_id, old_name);
  UpdateShortcutsMenu(app_id, web_app_info);
  UpdateUrlHandlers(app_id, base::DoNothing());
}

void OsIntegrationManager::GetAppExistingShortCutLocation(
    ShortcutLocationCallback callback,
    std::unique_ptr<ShortcutInfo> shortcut_info) {
  DCHECK(shortcut_manager_);
  shortcut_manager_->GetAppExistingShortCutLocation(std::move(callback),
                                                    std::move(shortcut_info));
}

void OsIntegrationManager::GetShortcutInfoForApp(
    const AppId& app_id,
    AppShortcutManager::GetShortcutInfoCallback callback) {
  DCHECK(shortcut_manager_);
  return shortcut_manager_->GetShortcutInfoForApp(app_id, std::move(callback));
}

bool OsIntegrationManager::IsFileHandlingAPIAvailable(const AppId& app_id) {
  DCHECK(file_handler_manager_);
  return file_handler_manager_->IsFileHandlingAPIAvailable(app_id);
}

const apps::FileHandlers* OsIntegrationManager::GetEnabledFileHandlers(
    const AppId& app_id) {
  DCHECK(file_handler_manager_);
  return file_handler_manager_->GetEnabledFileHandlers(app_id);
}

const base::Optional<GURL> OsIntegrationManager::GetMatchingFileHandlerURL(
    const AppId& app_id,
    const std::vector<base::FilePath>& launch_files) {
  DCHECK(file_handler_manager_);
  return file_handler_manager_->GetMatchingFileHandlerURL(app_id, launch_files);
}

void OsIntegrationManager::MaybeUpdateFileHandlingOriginTrialExpiry(
    content::WebContents* web_contents,
    const AppId& app_id) {
  DCHECK(file_handler_manager_);
  return file_handler_manager_->MaybeUpdateFileHandlingOriginTrialExpiry(
      web_contents, app_id);
}

void OsIntegrationManager::ForceEnableFileHandlingOriginTrial(
    const AppId& app_id) {
  DCHECK(file_handler_manager_);
  return file_handler_manager_->ForceEnableFileHandlingOriginTrial(app_id);
}

void OsIntegrationManager::DisableForceEnabledFileHandlingOriginTrial(
    const AppId& app_id) {
  DCHECK(file_handler_manager_);
  return file_handler_manager_->DisableForceEnabledFileHandlingOriginTrial(
      app_id);
}

base::Optional<GURL> OsIntegrationManager::TranslateProtocolUrl(
    const AppId& app_id,
    const GURL& protocol_url) {
  if (!protocol_handler_manager_)
    return base::Optional<GURL>();

  return protocol_handler_manager_->TranslateProtocolUrl(app_id, protocol_url);
}

FileHandlerManager& OsIntegrationManager::file_handler_manager_for_testing() {
  DCHECK(file_handler_manager_);
  return *file_handler_manager_;
}

UrlHandlerManager& OsIntegrationManager::url_handler_manager_for_testing() {
  DCHECK(url_handler_manager_);
  return *url_handler_manager_;
}

ProtocolHandlerManager&
OsIntegrationManager::protocol_handler_manager_for_testing() {
  DCHECK(protocol_handler_manager_);
  return *protocol_handler_manager_;
}

ScopedOsHooksSuppress OsIntegrationManager::ScopedSuppressOsHooksForTesting() {
// Creating OS hooks on ChromeOS doesn't write files to disk, so it's
// unnecessary to suppress and it provides better crash coverage.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  return std::make_unique<base::AutoReset<bool>>(
      &g_suppress_os_hooks_for_testing_, true);
#else
  return std::make_unique<base::AutoReset<bool>>(
      &g_suppress_os_hooks_for_testing_, false);
#endif
}

TestOsIntegrationManager* OsIntegrationManager::AsTestOsIntegrationManager() {
  return nullptr;
}

void OsIntegrationManager::CreateShortcuts(const AppId& app_id,
                                           bool add_to_desktop,
                                           CreateShortcutsCallback callback) {
  if (shortcut_manager_->CanCreateShortcuts()) {
    shortcut_manager_->CreateShortcuts(app_id, add_to_desktop,
                                       std::move(callback));
  } else {
    std::move(callback).Run(false);
  }
}

void OsIntegrationManager::RegisterFileHandlers(
    const AppId& app_id,
    base::OnceCallback<void(bool success)> callback) {
  DCHECK(file_handler_manager_);
  file_handler_manager_->EnableAndRegisterOsFileHandlers(app_id);

  // TODO(crbug.com/1087219): callback should be run after all hooks are
  // deployed, need to refactor filehandler to allow this.
  std::move(callback).Run(true);
}

void OsIntegrationManager::RegisterProtocolHandlers(
    const AppId& app_id,
    base::OnceCallback<void(bool success)> callback) {
  if (!protocol_handler_manager_) {
    std::move(callback).Run(true);
    return;
  }

  protocol_handler_manager_->RegisterOsProtocolHandlers(app_id,
                                                        std::move(callback));
}

void OsIntegrationManager::RegisterUrlHandlers(
    const AppId& app_id,
    base::OnceCallback<void(bool success)> callback) {
  if (!url_handler_manager_) {
    std::move(callback).Run(true);
    return;
  }

  url_handler_manager_->RegisterUrlHandlers(app_id, std::move(callback));
}

void OsIntegrationManager::RegisterShortcutsMenu(
    const AppId& app_id,
    const std::vector<WebApplicationShortcutsMenuItemInfo>&
        shortcuts_menu_item_infos,
    const ShortcutsMenuIconBitmaps& shortcuts_menu_icon_bitmaps,
    base::OnceCallback<void(bool success)> callback) {
  if (!ShouldRegisterShortcutsMenuWithOs()) {
    std::move(callback).Run(true);
    return;
  }

  DCHECK(shortcut_manager_);
  shortcut_manager_->RegisterShortcutsMenuWithOs(
      app_id, shortcuts_menu_item_infos, shortcuts_menu_icon_bitmaps);

  // TODO(https://crbug.com/1098471): fix RegisterShortcutsMenuWithOs to
  // take callback.
  std::move(callback).Run(true);
}

void OsIntegrationManager::ReadAllShortcutsMenuIconsAndRegisterShortcutsMenu(
    const AppId& app_id,
    base::OnceCallback<void(bool success)> callback) {
  if (!ShouldRegisterShortcutsMenuWithOs()) {
    std::move(callback).Run(true);
    return;
  }

  shortcut_manager_->ReadAllShortcutsMenuIconsAndRegisterShortcutsMenu(
      app_id, std::move(callback));
}

void OsIntegrationManager::RegisterRunOnOsLogin(
    const AppId& app_id,
    RegisterRunOnOsLoginCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  GetShortcutInfoForApp(
      app_id,
      base::BindOnce(
          &OsIntegrationManager::OnShortcutInfoRetrievedRegisterRunOnOsLogin,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void OsIntegrationManager::MacAppShimOnAppInstalledForProfile(
    const AppId& app_id) {
#if defined(OS_MAC)
  AppShimRegistry::Get()->OnAppInstalledForProfile(app_id, profile_->GetPath());
#endif
}

void OsIntegrationManager::AddAppToQuickLaunchBar(const AppId& app_id) {
  DCHECK(ui_manager_);
  if (ui_manager_->CanAddAppToQuickLaunchBar()) {
    ui_manager_->AddAppToQuickLaunchBar(app_id);
  }
}

void OsIntegrationManager::RegisterWebAppOsUninstallation(
    const AppId& app_id,
    const std::string& name) {
  if (ShouldRegisterUninstallationViaOsSettingsWithOs()) {
    RegisterUninstallationViaOsSettingsWithOs(app_id, name, profile_);
  }
}

bool OsIntegrationManager::UnregisterShortcutsMenu(const AppId& app_id) {
  if (!ShouldRegisterShortcutsMenuWithOs())
    return true;
  return UnregisterShortcutsMenuWithOs(app_id, profile_->GetPath());
}

void OsIntegrationManager::UnregisterRunOnOsLogin(
    const AppId& app_id,
    const base::FilePath& profile_path,
    const std::u16string& shortcut_title,
    UnregisterRunOnOsLoginCallback callback) {
  ScheduleUnregisterRunOnOsLogin(app_id, profile_path, shortcut_title,
                                 std::move(callback));
}

void OsIntegrationManager::DeleteShortcuts(
    const AppId& app_id,
    const base::FilePath& shortcuts_data_dir,
    std::unique_ptr<ShortcutInfo> shortcut_info,
    DeleteShortcutsCallback callback) {
  if (shortcut_manager_->CanCreateShortcuts()) {
    auto shortcuts_callback = base::BindOnce(
        &OsIntegrationManager::OnShortcutsDeleted,
        weak_ptr_factory_.GetWeakPtr(), app_id, std::move(callback));

    shortcut_manager_->DeleteShortcuts(app_id, shortcuts_data_dir,
                                       std::move(shortcut_info),
                                       std::move(shortcuts_callback));
  } else {
    std::move(callback).Run(false);
  }
}

void OsIntegrationManager::UnregisterFileHandlers(
    const AppId& app_id,
    std::unique_ptr<ShortcutInfo> info,
    base::OnceCallback<void()> callback) {
  DCHECK(file_handler_manager_);

  file_handler_manager_->DisableAndUnregisterOsFileHandlers(
      app_id, std::move(info), std::move(callback));
}

void OsIntegrationManager::UnregisterProtocolHandlers(const AppId& app_id) {
  if (!protocol_handler_manager_)
    return;

  // TODO(https://crbug.com/1019239) Make this take a callback, and return bool
  // success or a single/list of enum errors.
  protocol_handler_manager_->UnregisterOsProtocolHandlers(app_id);
}

void OsIntegrationManager::UnregisterUrlHandlers(const AppId& app_id) {
  if (!url_handler_manager_)
    return;

  url_handler_manager_->UnregisterUrlHandlers(app_id);
}

void OsIntegrationManager::UnregisterWebAppOsUninstallation(
    const AppId& app_id) {
  if (ShouldRegisterUninstallationViaOsSettingsWithOs())
    UnegisterUninstallationViaOsSettingsWithOs(app_id, profile_);
}

void OsIntegrationManager::UpdateShortcuts(const AppId& app_id,
                                           base::StringPiece old_name) {
  DCHECK(shortcut_manager_);
  shortcut_manager_->UpdateShortcuts(app_id, old_name);
}

void OsIntegrationManager::UpdateShortcutsMenu(
    const AppId& app_id,
    const WebApplicationInfo& web_app_info) {
  DCHECK(shortcut_manager_);
  if (base::FeatureList::IsEnabled(
          features::kDesktopPWAsAppIconShortcutsMenu) &&
      !web_app_info.shortcuts_menu_item_infos.empty()) {
    shortcut_manager_->RegisterShortcutsMenuWithOs(
        app_id, web_app_info.shortcuts_menu_item_infos,
        web_app_info.shortcuts_menu_icon_bitmaps);
  } else {
    // Unregister shortcuts menu when feature is disabled or
    // shortcuts_menu_item_infos is empty.
    shortcut_manager_->UnregisterShortcutsMenuWithOs(app_id);
  }
}

void OsIntegrationManager::UpdateUrlHandlers(
    const AppId& app_id,
    base::OnceCallback<void(bool success)> callback) {
  if (!url_handler_manager_)
    return;

  url_handler_manager_->UpdateUrlHandlers(app_id, std::move(callback));
}

void OsIntegrationManager::UpdateFileHandlers(
    const AppId& app_id,
    std::unique_ptr<ShortcutInfo> info) {
  if (!IsFileHandlingAPIAvailable(app_id))
    return;

  // Update file handlers via complete uninstallation, then reinstallation.
  auto callback = base::BindOnce(&OsIntegrationManager::RegisterFileHandlers,
                                 weak_ptr_factory_.GetWeakPtr(), app_id,
                                 base::DoNothing::Once<bool>());
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&OsIntegrationManager::UnregisterFileHandlers,
                                weak_ptr_factory_.GetWeakPtr(), app_id,
                                std::move(info), std::move(callback)));
}

std::unique_ptr<ShortcutInfo> OsIntegrationManager::BuildShortcutInfo(
    const AppId& app_id) {
  DCHECK(shortcut_manager_);
  return shortcut_manager_->BuildShortcutInfo(app_id);
}

void OsIntegrationManager::OnShortcutsCreated(
    const AppId& app_id,
    std::unique_ptr<WebApplicationInfo> web_app_info,
    InstallOsHooksOptions options,
    scoped_refptr<OsHooksBarrier> barrier,
    bool shortcuts_created) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(barrier);

  bool shortcut_creation_failure =
      !shortcuts_created && options.os_hooks[OsHookType::kShortcuts];
  if (shortcut_creation_failure)
    barrier->OnError(OsHookType::kShortcuts);

  if (options.os_hooks[OsHookType::kFileHandlers]) {
    RegisterFileHandlers(app_id, barrier->CreateBarrierCallbackForType(
                                     OsHookType::kFileHandlers));
  }

  if (options.os_hooks[OsHookType::kProtocolHandlers]) {
    RegisterProtocolHandlers(app_id, barrier->CreateBarrierCallbackForType(
                                         OsHookType::kProtocolHandlers));
  }

  if (options.os_hooks[OsHookType::kUrlHandlers]) {
    RegisterUrlHandlers(app_id, barrier->CreateBarrierCallbackForType(
                                    OsHookType::kUrlHandlers));
  }

  if (options.os_hooks[OsHookType::kShortcuts] &&
      options.add_to_quick_launch_bar) {
    AddAppToQuickLaunchBar(app_id);
  }
  if (shortcuts_created && options.os_hooks[OsHookType::kShortcutsMenu] &&
      base::FeatureList::IsEnabled(
          features::kDesktopPWAsAppIconShortcutsMenu)) {
    if (web_app_info) {
      RegisterShortcutsMenu(
          app_id, web_app_info->shortcuts_menu_item_infos,
          web_app_info->shortcuts_menu_icon_bitmaps,
          barrier->CreateBarrierCallbackForType(OsHookType::kShortcutsMenu));
    } else {
      ReadAllShortcutsMenuIconsAndRegisterShortcutsMenu(
          app_id,
          barrier->CreateBarrierCallbackForType(OsHookType::kShortcutsMenu));
    }
  }

  if (options.os_hooks[OsHookType::kRunOnOsLogin] &&
      base::FeatureList::IsEnabled(features::kDesktopPWAsRunOnOsLogin)) {
    // TODO(crbug.com/1091964): Implement Run on OS Login mode selection.
    // Currently it is set to be the default: RunOnOsLoginMode::kWindowed
    RegisterRunOnOsLogin(app_id, barrier->CreateBarrierCallbackForType(
                                     OsHookType::kRunOnOsLogin));
  }

  if (options.os_hooks[OsHookType::kUninstallationViaOsSettings] &&
      base::FeatureList::IsEnabled(
          features::kEnableWebAppUninstallFromOsSettings)) {
    RegisterWebAppOsUninstallation(app_id, registrar_->GetAppShortName(app_id));
  }
}

void OsIntegrationManager::OnShortcutsDeleted(const AppId& app_id,
                                              DeleteShortcutsCallback callback,
                                              bool shortcuts_deleted) {
#if defined(OS_MAC)
  bool delete_multi_profile_shortcuts =
      AppShimRegistry::Get()->OnAppUninstalledForProfile(app_id,
                                                         profile_->GetPath());
  if (delete_multi_profile_shortcuts) {
    internals::ScheduleDeleteMultiProfileShortcutsForApp(app_id,
                                                         std::move(callback));
  }
#else
  std::move(callback).Run(shortcuts_deleted);
#endif
}

void OsIntegrationManager::OnShortcutInfoRetrievedRegisterRunOnOsLogin(
    RegisterRunOnOsLoginCallback callback,
    std::unique_ptr<ShortcutInfo> info) {
  ScheduleRegisterRunOnOsLogin(std::move(info), std::move(callback));
}

}  // namespace web_app
