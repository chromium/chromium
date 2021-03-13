// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_file_handler_registration.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration_linux.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/components/web_app_shortcut.h"

namespace web_app {

namespace {

// Result of registering file handlers.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class RegistrationResult {
  kSuccess = 0,
  kFailToCreateTempDir = 1,
  kFailToWriteMimetypeFile = 2,
  kXdgReturnNonZeroCode = 3,
  kMaxValue = kXdgReturnNonZeroCode
};

// Result of re-creating shortcut.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class RecreateShortcutResult {
  kSuccess = 0,
  kFailToCreateShortcut = 1,
  kMaxValue = kFailToCreateShortcut
};

// UMA metric name for file handler registration result.
constexpr const char* kRegistrationResultMetric =
    "Apps.FileHandler.Registration.Linux.Result";

// UMA metric name for file handler shortcut re-create result.
constexpr const char* kRecreateShortcutResultMetric =
    "Apps.FileHandler.Registration.Linux.RecreateShortcut.Result";

// Records UMA metric for result of file handler registration.
void RecordRegistration(RegistrationResult result) {
  UMA_HISTOGRAM_ENUMERATION(kRegistrationResultMetric, result);
}

void OnCreateShortcut(base::OnceCallback<void()> callback,
                      bool shortcut_created) {
  UMA_HISTOGRAM_ENUMERATION(
      kRecreateShortcutResultMetric,
      shortcut_created ? RecreateShortcutResult::kSuccess
                       : RecreateShortcutResult::kFailToCreateShortcut);
  std::move(callback).Run();
}

void OnShortcutInfoReceived(base::OnceCallback<void()> callback,
                            std::unique_ptr<ShortcutInfo> info) {
  if (!info) {
    UMA_HISTOGRAM_ENUMERATION(kRecreateShortcutResultMetric,
                              RecreateShortcutResult::kFailToCreateShortcut);
    return;
  }

  base::FilePath shortcut_data_dir = internals::GetShortcutDataDir(*info);

  ShortcutLocations locations;
  locations.applications_menu_location = APP_MENU_LOCATION_SUBDIR_CHROMEAPPS;

  internals::ScheduleCreatePlatformShortcuts(
      std::move(shortcut_data_dir), locations,
      ShortcutCreationReason::SHORTCUT_CREATION_BY_USER, std::move(info),
      base::BindOnce(OnCreateShortcut, std::move(callback)));
}

void UpdateFileHandlerRegistrationInOs(const AppId& app_id,
                                       Profile* profile,
                                       std::unique_ptr<ShortcutInfo> info,
                                       base::OnceCallback<void()> callback) {
  if (info) {
    // `info` may be prepopulated for unregistration, to avoid updating file
    // handler registrations based on deleted shortcuts.
    OnShortcutInfoReceived(std::move(callback), std::move(info));
    return;
  }
  // On Linux, file associations are managed through shortcuts in the app menu,
  // so after enabling or disabling file handling for an app its shortcuts
  // need to be recreated.
  WebAppProviderBase::GetProviderBase(profile)
      ->os_integration_manager()
      .GetShortcutInfoForApp(
          app_id, base::BindOnce(&OnShortcutInfoReceived, std::move(callback)));
}

void OnRegisterMimeTypes(bool registration_succeeded) {
  if (!registration_succeeded)
    LOG(ERROR) << "Registering MIME types failed.";
}

bool DoRegisterMimeTypes(base::FilePath filename, std::string file_contents) {
  DCHECK(!filename.empty() && !file_contents.empty());

  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDir()) {
    RecordRegistration(RegistrationResult::kFailToCreateTempDir);
    return false;
  }

  base::FilePath temp_file_path(temp_dir.GetPath().Append(filename));
  if (!base::WriteFile(temp_file_path, file_contents)) {
    RecordRegistration(RegistrationResult::kFailToWriteMimetypeFile);
    return false;
  }

  std::vector<std::string> argv{"xdg-mime", "install", "--mode", "user",
                                temp_file_path.value()};

  int exit_code;
  shell_integration_linux::LaunchXdgUtility(argv, &exit_code);
  bool result = exit_code == 0;
  if (!result)
    RecordRegistration(RegistrationResult::kXdgReturnNonZeroCode);
  else
    RecordRegistration(RegistrationResult::kSuccess);
  return result;
}

RegisterMimeTypesOnLinuxCallback& GetRegisterMimeTypesCallbackForTesting() {
  static base::NoDestructor<RegisterMimeTypesOnLinuxCallback> instance;
  return *instance;
}

}  // namespace

bool ShouldRegisterFileHandlersWithOs() {
  return true;
}

void RegisterFileHandlersWithOs(const AppId& app_id,
                                const std::string& app_name,
                                Profile* profile,
                                const apps::FileHandlers& file_handlers) {
  if (!file_handlers.empty()) {
    RegisterMimeTypesOnLinuxCallback callback =
        GetRegisterMimeTypesCallbackForTesting()
            ? std::move(GetRegisterMimeTypesCallbackForTesting())
            : base::BindOnce(&DoRegisterMimeTypes);
    RegisterMimeTypesOnLinux(app_id, profile, file_handlers,
                             std::move(callback));
  }

  UpdateFileHandlerRegistrationInOs(app_id, profile, nullptr,
                                    base::DoNothing());
}

void UnregisterFileHandlersWithOs(const AppId& app_id,
                                  Profile* profile,
                                  std::unique_ptr<ShortcutInfo> info,
                                  base::OnceCallback<void()> callback) {
  // If this was triggered as part of the uninstallation process, nothing more
  // is needed. Uninstalling already cleans up shortcuts (and thus, file
  // handlers).
  auto* provider = WebAppProviderBase::GetProviderBase(profile);
  if (!provider->registrar().IsInstalled(app_id))
    return;

  // TODO(crbug.com/1076688): Fix file handlers unregistration. We can't update
  // registration here asynchronously because app_id is being uninstalled.
  UpdateFileHandlerRegistrationInOs(app_id, profile, std::move(info),
                                    std::move(callback));
}

void RegisterMimeTypesOnLinux(const AppId& app_id,
                              Profile* profile,
                              const apps::FileHandlers& file_handlers,
                              RegisterMimeTypesOnLinuxCallback callback) {
  DCHECK(!app_id.empty() && !file_handlers.empty());

  base::FilePath filename =
      shell_integration_linux::GetMimeTypesRegistrationFilename(
          profile->GetPath(), app_id);
  std::string file_contents =
      shell_integration_linux::GetMimeTypesRegistrationFileContents(
          file_handlers);

  internals::GetShortcutIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(filename),
                     std::move(file_contents)),
      base::BindOnce(&OnRegisterMimeTypes));
}

void SetRegisterMimeTypesOnLinuxCallbackForTesting(
    RegisterMimeTypesOnLinuxCallback callback) {
  GetRegisterMimeTypesCallbackForTesting() = std::move(callback);
}

}  // namespace web_app
