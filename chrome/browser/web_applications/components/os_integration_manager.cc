// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/os_integration_manager.h"

#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/optional.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_app_shortcut.h"
#include "chrome/browser/web_applications/components/web_app_ui_manager.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_MAC)
#include "chrome/browser/web_applications/components/app_shim_registry_mac.h"
#endif

namespace {
// Used to  disable os hooks globally when OsIntegrationManager::SuppressOsHooks
// can't be easily used.
bool g_suppress_os_hooks_for_testing_ = false;
}  // namespace

namespace web_app {

InstallOsHooksOptions::InstallOsHooksOptions() = default;
InstallOsHooksOptions::InstallOsHooksOptions(
    const InstallOsHooksOptions& other) = default;

// This is adapted from base/barrier_closure.cc. os_hooks_results is maintained
// to track install results from different OS hooks callers
class OsHooksBarrierInfo {
 public:
  explicit OsHooksBarrierInfo(InstallOsHooksCallback done_callback)
      : done_callback_(std::move(done_callback)) {}

  void Run(OsHookType::Type os_hook, bool completed) {
    DCHECK(!os_hooks_called_[os_hook]);

    os_hooks_called_[os_hook] = true;
    os_hooks_results_[os_hook] = completed;

    if (os_hooks_called_.all()) {
      std::move(done_callback_).Run(os_hooks_results_);
    }
  }

 private:
  OsHooksResults os_hooks_results_{false};
  OsHooksResults os_hooks_called_{false};
  InstallOsHooksCallback done_callback_;
};

class CallbackFactory {
 public:
  explicit CallbackFactory(BarrierCallback barrier_callback)
      : barrier_callback_(std::move(barrier_callback)) {}
  ~CallbackFactory() { DCHECK(callback_created_.all()); }

  base::OnceCallback<void(bool)> CreateCallack(OsHookType::Type os_hook) {
    callback_created_[os_hook] = true;
    return base::BindOnce(barrier_callback_, os_hook);
  }

 private:
  BarrierCallback barrier_callback_;
  OsHooksResults callback_created_{false};
};

OsIntegrationManager::OsIntegrationManager(
    Profile* profile,
    std::unique_ptr<AppShortcutManager> shortcut_manager,
    std::unique_ptr<FileHandlerManager> file_handler_manager)
    : profile_(profile),
      shortcut_manager_(std::move(shortcut_manager)),
      file_handler_manager_(std::move(file_handler_manager)) {}

OsIntegrationManager::~OsIntegrationManager() = default;

void OsIntegrationManager::SetSubsystems(AppRegistrar* registrar,
                                         WebAppUiManager* ui_manager,
                                         AppIconManager* icon_manager) {
  registrar_ = registrar;
  ui_manager_ = ui_manager;
  file_handler_manager_->SetSubsystems(registrar);
  shortcut_manager_->SetSubsystems(icon_manager, registrar);
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
}

void OsIntegrationManager::InstallOsHooks(
    const AppId& app_id,
    InstallOsHooksCallback callback,
    std::unique_ptr<WebApplicationInfo> web_app_info,
    InstallOsHooksOptions options) {
  DCHECK(shortcut_manager_);
  if (g_suppress_os_hooks_for_testing_) {
    OsHooksResults os_hooks_results{true};
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), os_hooks_results));
    return;
  }
  MacAppShimOnAppInstalledForProfile(app_id);

  // Note: This barrier protects against multiple calls on the same type, but
  // it doesn't protect against the case where we fail to call Run / create a
  // callback for every type. Developers should double check that Run is
  // called for every OsHookType::Type. If there is any missing type, the
  // InstallOsHooksCallback will not get run.
  BarrierCallback barrier = base::BindRepeating(
      &OsHooksBarrierInfo::Run,
      base::Owned(new OsHooksBarrierInfo(std::move(callback))));

  DCHECK(options.os_hooks[OsHookType::kShortcuts] ||
         !options.os_hooks[OsHookType::kShortcutsMenu])
      << "Cannot install shortcuts menu without installing shortcuts.";

  // TODO(ortuno): Make adding a shortcut to the applications menu independent
  // from adding a shortcut to desktop.
  if (options.os_hooks[OsHookType::kShortcuts] && CanCreateShortcuts()) {
    const bool add_to_desktop = options.add_to_desktop;
    CreateShortcutsCallback shortcuts_callback = base::BindOnce(
        &OsIntegrationManager::OnShortcutsCreated,
        weak_ptr_factory_.GetWeakPtr(), app_id, std::move(web_app_info),
        std::move(options), std::move(barrier));
    CreateShortcuts(app_id, add_to_desktop, std::move(shortcuts_callback));
  } else {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&OsIntegrationManager::OnShortcutsCreated,
                                  weak_ptr_factory_.GetWeakPtr(), app_id,
                                  std::move(web_app_info), std::move(options),
                                  std::move(barrier),
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
  DCHECK(shortcut_manager_);

  if (g_suppress_os_hooks_for_testing_) {
    OsHooksResults os_hooks_results{true};
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), os_hooks_results));
    return;
  }

  base::RepeatingCallback<void(OsHookType::Type os_hook, bool completed)>
      barrier = base::BindRepeating(
          &OsHooksBarrierInfo::Run,
          base::Owned(new OsHooksBarrierInfo(std::move(callback))));
  CallbackFactory callback_factory = CallbackFactory(barrier);

  if (os_hooks[OsHookType::kShortcutsMenu] &&
      ShouldRegisterShortcutsMenuWithOs()) {
    callback_factory.CreateCallack(OsHookType::kShortcutsMenu)
        .Run(UnregisterShortcutsMenuWithOs(app_id, profile_->GetPath()));
  } else {
    callback_factory.CreateCallack(OsHookType::kShortcutsMenu)
        .Run(/*completed=*/true);
  }

  if (os_hooks[OsHookType::kShortcuts] || os_hooks[OsHookType::kRunOnOsLogin]) {
    std::unique_ptr<ShortcutInfo> shortcut_info =
        shortcut_manager_->BuildShortcutInfo(app_id);
    base::FilePath shortcut_data_dir =
        internals::GetShortcutDataDir(*shortcut_info);

    if (os_hooks[OsHookType::kRunOnOsLogin] &&
        base::FeatureList::IsEnabled(features::kDesktopPWAsRunOnOsLogin)) {
      ScheduleUnregisterRunOnOsLogin(
          shortcut_info->profile_path, shortcut_info->title,
          base::BindOnce(
              callback_factory.CreateCallack(OsHookType::kRunOnOsLogin)));
    } else {
      callback_factory.CreateCallack(OsHookType::kRunOnOsLogin)
          .Run(/*completed=*/true);
    }

    if (os_hooks[OsHookType::kShortcuts]) {
      internals::ScheduleDeletePlatformShortcuts(
          shortcut_data_dir, std::move(shortcut_info),
          base::BindOnce(
              &OsIntegrationManager::OnShortcutsDeleted,
              weak_ptr_factory_.GetWeakPtr(), app_id,
              callback_factory.CreateCallack(OsHookType::kShortcuts)));
    } else {
      callback_factory.CreateCallack(OsHookType::kShortcuts)
          .Run(/*completed=*/true);
    }
  } else {
    callback_factory.CreateCallack(OsHookType::kShortcuts)
        .Run(/*completed=*/true);
    callback_factory.CreateCallack(OsHookType::kRunOnOsLogin)
        .Run(/*completed=*/true);
  }

  // TODO(https://crbug.com/1108109) we should return the result of file handler
  // unregistration and record errors during unregistration.
  if (os_hooks[OsHookType::kFileHandlers])
    file_handler_manager_->DisableAndUnregisterOsFileHandlers(app_id);

  callback_factory.CreateCallack(OsHookType::kFileHandlers)
      .Run(/*completed=*/true);
}

void OsIntegrationManager::UpdateOsHooks(
    const AppId& app_id,
    base::StringPiece old_name,
    const WebApplicationInfo& web_app_info) {
  DCHECK(shortcut_manager_);

  if (g_suppress_os_hooks_for_testing_)
    return;

  // TODO(crbug.com/1079439): Update file handlers.
  shortcut_manager_->UpdateShortcuts(app_id, old_name);
  if (base::FeatureList::IsEnabled(
          features::kDesktopPWAsAppIconShortcutsMenu) &&
      !web_app_info.shortcuts_menu_item_infos.empty()) {
    shortcut_manager_->RegisterShortcutsMenuWithOs(
        app_id, web_app_info.shortcuts_menu_item_infos,
        web_app_info.shortcuts_menu_icons_bitmaps);
  } else {
    // Unregister shortcuts menu when feature is disabled or
    // shortcuts_menu_item_infos is empty.
    shortcut_manager_->UnregisterShortcutsMenuWithOs(app_id);
  }
}

bool OsIntegrationManager::CanCreateShortcuts() const {
  if (suppress_os_managers_for_testing_)
    return true;

  DCHECK(shortcut_manager_);
  return shortcut_manager_->CanCreateShortcuts();
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

FileHandlerManager& OsIntegrationManager::file_handler_manager_for_testing() {
  DCHECK(file_handler_manager_);
  return *file_handler_manager_;
}

ScopedOsHooksSuppress OsIntegrationManager::ScopedSuppressOsHooksForTesting() {
// Creating OS hooks on ChromeOS doesn't write files to disk, so it's
// unnecessary to suppress and it provides better crash coverage.
#if !defined(OS_CHROMEOS)
  return std::make_unique<base::AutoReset<bool>>(
      &g_suppress_os_hooks_for_testing_, true);
#else
  return std::make_unique<base::AutoReset<bool>>(
      &g_suppress_os_hooks_for_testing_, false);
#endif
}

void OsIntegrationManager::SuppressOsManagersForTesting() {
  suppress_os_managers_for_testing_ = true;
}

TestOsIntegrationManager* OsIntegrationManager::AsTestOsIntegrationManager() {
  return nullptr;
}

void OsIntegrationManager::CreateShortcuts(const AppId& app_id,
                                           bool add_to_desktop,
                                           CreateShortcutsCallback callback) {
  if (suppress_os_managers_for_testing_) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), true));
    return;
  }

  shortcut_manager_->CreateShortcuts(app_id, add_to_desktop,
                                     std::move(callback));
}

void OsIntegrationManager::RegisterFileHandlers(
    const AppId& app_id,
    base::OnceCallback<void(bool success)> callback) {
  if (suppress_os_managers_for_testing_)
    return;

  file_handler_manager_->EnableAndRegisterOsFileHandlers(app_id);

  // TODO(crbug.com/1087219): callback should be run after all hooks are
  // deployed, need to refactor filehandler to allow this.
  std::move(callback).Run(true);
}

void OsIntegrationManager::RegisterShortcutsMenu(
    const AppId& app_id,
    const std::vector<WebApplicationShortcutsMenuItemInfo>&
        shortcuts_menu_item_infos,
    const ShortcutsMenuIconsBitmaps& shortcuts_menu_icons_bitmaps,
    base::OnceCallback<void(bool success)> callback) {
  if (suppress_os_managers_for_testing_)
    return;

  shortcut_manager_->RegisterShortcutsMenuWithOs(
      app_id, shortcuts_menu_item_infos, shortcuts_menu_icons_bitmaps);

  // TODO(https://crbug.com/1098471): fix RegisterShortcutsMenuWithOs to
  // take callback.
  std::move(callback).Run(true);
}

void OsIntegrationManager::ReadAllShortcutsMenuIconsAndRegisterShortcutsMenu(
    const AppId& app_id,
    base::OnceCallback<void(bool success)> callback) {
  if (suppress_os_managers_for_testing_) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), true));
    return;
  }
  shortcut_manager_->ReadAllShortcutsMenuIconsAndRegisterShortcutsMenu(
      app_id, std::move(callback));
}

void OsIntegrationManager::RegisterRunOnOsLogin(
    const AppId& app_id,
    RegisterRunOnOsLoginCallback callback) {
  if (suppress_os_managers_for_testing_) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), true));
    return;
  }
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  shortcut_manager_->GetShortcutInfoForApp(
      app_id,
      base::BindOnce(
          &OsIntegrationManager::OnShortcutInfoRetrievedRegisterRunOnOsLogin,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void OsIntegrationManager::MacAppShimOnAppInstalledForProfile(
    const AppId& app_id) {
  if (suppress_os_managers_for_testing_)
    return;
#if defined(OS_MAC)
  AppShimRegistry::Get()->OnAppInstalledForProfile(app_id, profile_->GetPath());
#endif
}

void OsIntegrationManager::AddAppToQuickLaunchBar(const AppId& app_id) {
  if (suppress_os_managers_for_testing_)
    return;

  DCHECK(ui_manager_);
  if (ui_manager_->CanAddAppToQuickLaunchBar()) {
    ui_manager_->AddAppToQuickLaunchBar(app_id);
  }
}

void OsIntegrationManager::OnShortcutsCreated(
    const AppId& app_id,
    std::unique_ptr<WebApplicationInfo> web_app_info,
    InstallOsHooksOptions options,
    BarrierCallback barrier,
    bool shortcuts_created) {
  DCHECK(file_handler_manager_);
  CallbackFactory callback_factory = CallbackFactory(barrier);

  callback_factory.CreateCallack(OsHookType::kShortcuts)
      .Run(/*completed=*/shortcuts_created);

  if (options.os_hooks[OsHookType::kFileHandlers]) {
    RegisterFileHandlers(
        app_id, callback_factory.CreateCallack(OsHookType::kFileHandlers));
  } else {
    callback_factory.CreateCallack(OsHookType::kFileHandlers)
        .Run(/*completed=*/false);
  }

  if (options.os_hooks[OsHookType::kShortcuts] &&
      options.add_to_quick_launch_bar) {
    AddAppToQuickLaunchBar(app_id);
  }
  if (shortcuts_created && options.os_hooks[OsHookType::kShortcutsMenu] &&
      base::FeatureList::IsEnabled(
          features::kDesktopPWAsAppIconShortcutsMenu)) {
    if (web_app_info) {
      if (web_app_info->shortcuts_menu_item_infos.empty()) {
        callback_factory.CreateCallack(OsHookType::kShortcutsMenu)
            .Run(/*completed=*/false);
      } else {
        RegisterShortcutsMenu(
            app_id, web_app_info->shortcuts_menu_item_infos,
            web_app_info->shortcuts_menu_icons_bitmaps,
            callback_factory.CreateCallack(OsHookType::kShortcutsMenu));
      }
    } else {
      ReadAllShortcutsMenuIconsAndRegisterShortcutsMenu(
          app_id, callback_factory.CreateCallack(OsHookType::kShortcutsMenu));
    }
  } else {
    callback_factory.CreateCallack(OsHookType::kShortcutsMenu)
        .Run(/*completed=*/false);
  }

  if (options.os_hooks[OsHookType::kRunOnOsLogin] &&
      base::FeatureList::IsEnabled(features::kDesktopPWAsRunOnOsLogin)) {
    // TODO(crbug.com/897302): Implement Run on OS Login mode selection.
    // Currently it is set to be the default: RunOnOsLoginMode::kWindowed
    RegisterRunOnOsLogin(app_id, base::BindOnce(callback_factory.CreateCallack(
                                     OsHookType::kRunOnOsLogin)));
  } else {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(callback_factory.CreateCallack(
                                      OsHookType::kRunOnOsLogin),
                                  /*completed=*/false));
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
