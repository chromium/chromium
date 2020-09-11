// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/file_handler_manager.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/web_app_file_handler_registration.h"
#include "chrome/browser/web_applications/components/web_app_prefs_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/web_launch/file_handling_expiry.mojom.h"

namespace web_app {

namespace {
// Use a large double that can be safely saved in prefs, and be  safely
// represented in JS timestamp (milliseconds from epoch). base::Time::Max() does
// not work here, it returns +Infinity (which is invalid and can not be
// represented in JSON).
//
// This value is `floor((2^53 - 1) / 1000)` because base::Time::FromDoubleT()
// accepts time offset in seconds. In reality, it means 287396-10-12 08:58:59
// UTC, which is a long distant future (long after File Handling goes out of
// origin trial or be deprecated).
//
// Do not change this value, because it is persisted to disk.
const double kMaxOriginTrialExpiryTime = 9007199254740;
}  // namespace

bool FileHandlerManager::disable_automatic_file_handler_cleanup_for_testing_ =
    false;

FileHandlerManager::FileHandlerManager(Profile* profile) : profile_(profile) {}

FileHandlerManager::~FileHandlerManager() = default;

void FileHandlerManager::SetSubsystems(AppRegistrar* registrar) {
  registrar_ = registrar;
}

void FileHandlerManager::Start() {
  DCHECK(registrar_);

  if (!FileHandlerManager::
          disable_automatic_file_handler_cleanup_for_testing_) {
    content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
        ->PostTask(
            FROM_HERE,
            base::BindOnce(base::IgnoreResult(
                               &FileHandlerManager::CleanupAfterOriginTrials),
                           weak_ptr_factory_.GetWeakPtr()));
  }
}

void FileHandlerManager::DisableOsIntegrationForTesting() {
  disable_os_integration_for_testing_ = true;
}

int FileHandlerManager::TriggerFileHandlerCleanupForTesting() {
  return CleanupAfterOriginTrials();
}

void FileHandlerManager::SetOnFileHandlingExpiryUpdatedForTesting(
    base::RepeatingCallback<void()> on_file_handling_expiry_updated) {
  on_file_handling_expiry_updated_for_testing_ =
      on_file_handling_expiry_updated;
}

void FileHandlerManager::EnableAndRegisterOsFileHandlers(const AppId& app_id) {
  if (!IsFileHandlingAPIAvailable(app_id))
    return;

  UpdateBoolWebAppPref(profile()->GetPrefs(), app_id, kFileHandlersEnabled,
                       /*value=*/true);

  if (!ShouldRegisterFileHandlersWithOs() ||
      disable_os_integration_for_testing_) {
    return;
  }

// File handler registration is done via shortcuts creation on MacOS,
// WebAppShortcutManager::BuildShortcutInfoForWebApp collects file handler
// information to shortcut_info->file_handler_extensions, then used by MacOS
// implementation of |internals::CreatePlatformShortcuts|. So we avoid creating
// shortcuts twice here.
#if !defined(OS_MAC)
  std::string app_name = registrar_->GetAppShortName(app_id);
  const apps::FileHandlers* file_handlers = GetAllFileHandlers(app_id);
  if (file_handlers)
    RegisterFileHandlersWithOs(app_id, app_name, profile(), *file_handlers);
#endif
}

void FileHandlerManager::DisableAndUnregisterOsFileHandlers(
    const AppId& app_id) {
  UpdateBoolWebAppPref(profile()->GetPrefs(), app_id, kFileHandlersEnabled,
                       /*value=*/false);

  // Temporarily allow file handlers unregistration only if an app has them.
  // TODO(crbug.com/1088434, crbug.com/1076688): Do not start async
  // CreateShortcuts process in OnWebAppUninstalled / Unregistration.
  const apps::FileHandlers* file_handlers = GetAllFileHandlers(app_id);

  if (!ShouldRegisterFileHandlersWithOs() || !file_handlers ||
      file_handlers->empty() || disable_os_integration_for_testing_) {
    return;
  }

  // File handler information is embedded in the shortcut, when
  // |DeleteSharedAppShims| is called in
  // |OsIntegrationManager::UninstallOsHooks|, file handlers are also
  // unregistered./
#if !defined(OS_MAC)
  UnregisterFileHandlersWithOs(app_id, profile());
#endif
}

void FileHandlerManager::MaybeUpdateFileHandlingOriginTrialExpiry(
    content::WebContents* web_contents,
    const AppId& app_id) {
  // If an App has force enabled file handling, there is no need to check its
  // WebContents.
  if (IsFileHandlingForceEnabled(app_id)) {
    if (on_file_handling_expiry_updated_for_testing_)
      on_file_handling_expiry_updated_for_testing_.Run();
    return;
  }

  mojo::AssociatedRemote<blink::mojom::FileHandlingExpiry> expiry_service;
  web_contents->GetMainFrame()->GetRemoteAssociatedInterfaces()->GetInterface(
      &expiry_service);
  DCHECK(expiry_service);

  auto* raw = expiry_service.get();

  // Here we need to pass the |expiry_service| Mojom remote interface, so it is
  // not destroyed before we get a reply.
  raw->RequestOriginTrialExpiryTime(base::BindOnce(
      &FileHandlerManager::OnOriginTrialExpiryTimeReceived,
      weak_ptr_factory_.GetWeakPtr(), std::move(expiry_service), app_id));
}

void FileHandlerManager::ForceEnableFileHandlingOriginTrial(
    const AppId& app_id) {
  UpdateFileHandlersForOriginTrialExpiryTime(
      app_id, base::Time::FromDoubleT(kMaxOriginTrialExpiryTime));
}

void FileHandlerManager::DisableForceEnabledFileHandlingOriginTrial(
    const AppId& app_id) {
  double pref_expiry_time =
      GetDoubleWebAppPref(profile()->GetPrefs(), app_id,
                          kFileHandlingOriginTrialExpiryTime)
          .value_or(0);
  if (pref_expiry_time == kMaxOriginTrialExpiryTime) {
    UpdateFileHandlersForOriginTrialExpiryTime(app_id, base::Time());
  }
}

const apps::FileHandlers* FileHandlerManager::GetEnabledFileHandlers(
    const AppId& app_id) {
  if (AreFileHandlersEnabled(app_id) && IsFileHandlingAPIAvailable(app_id))
    return GetAllFileHandlers(app_id);

  return nullptr;
}

bool FileHandlerManager::IsFileHandlingAPIAvailable(const AppId& app_id) {
  double pref_expiry_time =
      GetDoubleWebAppPref(profile()->GetPrefs(), app_id,
                          kFileHandlingOriginTrialExpiryTime)
          .value_or(0);
  return base::FeatureList::IsEnabled(blink::features::kFileHandlingAPI) ||
         base::Time::FromDoubleT(pref_expiry_time) >= base::Time::Now();
}

bool FileHandlerManager::AreFileHandlersEnabled(const AppId& app_id) const {
  return GetBoolWebAppPref(profile()->GetPrefs(), app_id, kFileHandlersEnabled);
}

void FileHandlerManager::OnOriginTrialExpiryTimeReceived(
    mojo::AssociatedRemote<blink::mojom::FileHandlingExpiry> /*interface*/,
    const AppId& app_id,
    base::Time expiry_time) {
  // Updates the expiry time, if file handling is enabled by origin trial
  // tokens. If an App has force enabled file handling, it might not have expiry
  // time associated with it.
  if (!IsFileHandlingForceEnabled(app_id)) {
    UpdateFileHandlersForOriginTrialExpiryTime(app_id, expiry_time);
  }

  if (on_file_handling_expiry_updated_for_testing_)
    on_file_handling_expiry_updated_for_testing_.Run();
}

void FileHandlerManager::UpdateFileHandlersForOriginTrialExpiryTime(
    const AppId& app_id,
    const base::Time& expiry_time) {
  web_app::UpdateDoubleWebAppPref(profile_->GetPrefs(), app_id,
                                  kFileHandlingOriginTrialExpiryTime,
                                  expiry_time.ToDoubleT());
  // Only enable/disable file handlers if the state is changing, as
  // enabling/disabling is a potentially expensive operation (it may involve
  // creating an app shim, and will almost certainly involve IO).
  const bool file_handlers_enabled = AreFileHandlersEnabled(app_id);

  // If the trial is valid, ensure the file handlers are enabled.
  // Otherwise disable them.
  if (IsFileHandlingAPIAvailable(app_id)) {
    if (!file_handlers_enabled)
      EnableAndRegisterOsFileHandlers(app_id);
  } else if (file_handlers_enabled) {
    DisableAndUnregisterOsFileHandlers(app_id);
  }
}

void FileHandlerManager::DisableAutomaticFileHandlerCleanupForTesting() {
  disable_automatic_file_handler_cleanup_for_testing_ = true;
}

int FileHandlerManager::CleanupAfterOriginTrials() {
  int cleaned_up_count = 0;
  for (const AppId& app_id : registrar_->GetAppIds()) {
    if (!AreFileHandlersEnabled(app_id))
      continue;

    if (IsFileHandlingAPIAvailable(app_id))
      continue;

    // If the trial has expired, unregister handlers.
    DisableAndUnregisterOsFileHandlers(app_id);
    cleaned_up_count++;
  }

  return cleaned_up_count;
}

const base::Optional<GURL> FileHandlerManager::GetMatchingFileHandlerURL(
    const AppId& app_id,
    const std::vector<base::FilePath>& launch_files) {
  if (!IsFileHandlingAPIAvailable(app_id))
    return base::nullopt;

  const apps::FileHandlers* file_handlers = GetAllFileHandlers(app_id);
  if (!file_handlers || launch_files.empty())
    return base::nullopt;

  std::set<std::string> launch_file_extensions;
  for (const auto& file_path : launch_files) {
    std::string file_extension =
        base::FilePath(file_path.Extension()).AsUTF8Unsafe();
    if (file_extension.length() <= 1)
      return base::nullopt;
    launch_file_extensions.insert(file_extension);
  }

  for (const auto& file_handler : *file_handlers) {
    bool all_launch_file_extensions_supported = true;
    std::set<std::string> supported_file_extensions =
        apps::GetFileExtensionsFromFileHandlers({file_handler});
    for (const auto& file_extension : launch_file_extensions) {
      if (!supported_file_extensions.count(file_extension)) {
        all_launch_file_extensions_supported = false;
        break;
      }
    }

    if (all_launch_file_extensions_supported)
      return file_handler.action;
  }

  return base::nullopt;
}

bool FileHandlerManager::IsFileHandlingForceEnabled(const AppId& app_id) {
  double pref_expiry_time =
      GetDoubleWebAppPref(profile()->GetPrefs(), app_id,
                          kFileHandlingOriginTrialExpiryTime)
          .value_or(0);
  return pref_expiry_time == kMaxOriginTrialExpiryTime;
}

}  // namespace web_app
