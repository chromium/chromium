// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/web_app_file_handler_registration.h"

#include <memory>
#include <string>
#include <vector>

#include "base/check_is_test.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/nix/xdg_util.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "chrome/browser/shell_integration_linux.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/os_integration_test_override.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"

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
  kUpdateDesktopDatabaseFailed = 4,
  kMaxValue = kUpdateDesktopDatabaseFailed
};

// UMA metric name for MIME type registration result.
constexpr const char* kRegistrationResultMetric =
    "Apps.FileHandler.Registration.Linux.Result";

// UMA metric name for MIME type unregistration result.
constexpr const char* kUnregistrationResultMetric =
    "Apps.FileHandler.Unregistration.Linux.Result";

// Records UMA metric for result of MIME type registration/unregistration.
void RecordRegistration(RegistrationResult result, bool install) {
  base::UmaHistogramEnumeration(
      install ? kRegistrationResultMetric : kUnregistrationResultMetric,
      result);
}

void OnMimeInfoDatabaseUpdated(bool install,
                               ResultCallback result_callback,
                               bool registration_succeeded) {
  if (!registration_succeeded) {
    LOG(ERROR) << (install ? "Registering MIME types failed."
                           : "Unregistering MIME types failed.");
  }
  std::move(result_callback)
      .Run(registration_succeeded ? Result::kOk : Result::kError);
}

UpdateMimeInfoDatabaseOnLinuxCallback&
GetUpdateMimeInfoDatabaseCallbackForTesting() {
  static base::NoDestructor<UpdateMimeInfoDatabaseOnLinuxCallback> instance;
  return *instance;
}

void RefreshMimeInfoCache() {
  scoped_refptr<OsIntegrationTestOverride> test_override =
      OsIntegrationTestOverride::Get();

  std::unique_ptr<base::Environment> env(base::Environment::Create());
  std::vector<std::string> argv;

  base::Environment* env_ptr = env.get();
  if (test_override) {
    env_ptr = test_override->environment();
  }

  // Some Linux file managers (Nautilus and Nemo) depend on an up to date
  // mimeinfo.cache file to detect whether applications can open files, so
  // manually run update-desktop-database on the user applications folder.
  // See this bug on xdg desktop-file-utils.
  // https://gitlab.freedesktop.org/xdg/desktop-file-utils/issues/54
  base::FilePath user_dir = base::nix::GetXDGDataWriteLocation(env_ptr);
  base::FilePath user_applications_dir = user_dir.Append("applications");

  argv.push_back("update-desktop-database");
  argv.push_back(user_applications_dir.value());

  // The failure for this is ignored because the mime-types have already been
  // updated due to xdg-mime calls and is non-critical to file handling
  // working. The only downside is that file type associations for a
  // particular app may not show up in some file managers.
  int mime_cache_update_exit_code = 0;
  if (!GetUpdateMimeInfoDatabaseCallbackForTesting()) {
    shell_integration_linux::LaunchXdgUtility(argv,
                                              &mime_cache_update_exit_code);
    RecordRegistration(RegistrationResult::kUpdateDesktopDatabaseFailed,
                       (mime_cache_update_exit_code == 0));
  } else {
    CHECK_IS_TEST();
    GetUpdateMimeInfoDatabaseCallbackForTesting().Run(  // IN-TEST
        base::FilePath(), base::JoinString(argv, " "), "");
  }
}

bool UpdateMimeInfoDatabase(bool install,
                            base::FilePath filename,
                            std::string file_contents) {
  DCHECK(!filename.empty());
  DCHECK_NE(install, file_contents.empty());

  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDir()) {
    RecordRegistration(RegistrationResult::kFailToCreateTempDir, install);
    return false;
  }

  base::FilePath temp_file_path(temp_dir.GetPath().Append(filename));
  if (!base::WriteFile(temp_file_path, file_contents)) {
    RecordRegistration(RegistrationResult::kFailToWriteMimetypeFile, install);
    return false;
  }

  const std::vector<std::string> install_argv{"xdg-mime", "install", "--mode",
                                              "user", temp_file_path.value()};
  const std::vector<std::string> uninstall_argv{"xdg-mime", "uninstall",
                                                temp_file_path.value()};

  int exit_code;
  bool success = false;
  std::vector<std::string> argv = install ? install_argv : uninstall_argv;

  if (!GetUpdateMimeInfoDatabaseCallbackForTesting()) {
    shell_integration_linux::LaunchXdgUtility(argv, &exit_code);
    success = exit_code == 0;
    RecordRegistration(success ? RegistrationResult::kSuccess
                               : RegistrationResult::kXdgReturnNonZeroCode,
                       install);
  } else {
    GetUpdateMimeInfoDatabaseCallbackForTesting().Run(
        filename, base::JoinString(argv, " "), file_contents);
    success = true;
  }
  RefreshMimeInfoCache();
  return success;
}

void UninstallMimeInfoOnLinux(const webapps::AppId& app_id,
                              const base::FilePath& profile_path,
                              ResultCallback on_done) {
  base::FilePath filename =
      shell_integration_linux::GetMimeTypesRegistrationFilename(profile_path,
                                                                app_id);

  // Empty file contents because xdg-mime uninstall only cares about the file
  // name (it uninstalls whatever associations were previously installed via the
  // same filename).
  std::string file_contents;
  internals::GetShortcutIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(base::BindOnce(&UpdateMimeInfoDatabase, /*install=*/false),
                     std::move(filename), std::move(file_contents)),
      base::BindOnce(&OnMimeInfoDatabaseUpdated, /*install=*/false,
                     std::move(on_done)));
}

}  // namespace

bool ShouldRegisterFileHandlersWithOs() {
  return true;
}

bool FileHandlingIconsSupportedByOs() {
  // File type icons are not supported on Linux: see https://crbug.com/1218235
  return false;
}

void RegisterFileHandlersWithOs(const webapps::AppId& app_id,
                                const std::string& app_name,
                                const base::FilePath& profile_path,
                                const apps::FileHandlers& file_handlers,
                                ResultCallback callback) {
  DCHECK(!file_handlers.empty());
  InstallMimeInfoOnLinux(app_id, profile_path, file_handlers,
                         std::move(callback));
}

void UnregisterFileHandlersWithOs(const webapps::AppId& app_id,
                                  const base::FilePath& profile_path,
                                  ResultCallback callback) {
  UninstallMimeInfoOnLinux(app_id, profile_path, std::move(callback));
}

void InstallMimeInfoOnLinux(const webapps::AppId& app_id,
                            const base::FilePath& profile_path,
                            const apps::FileHandlers& file_handlers,
                            ResultCallback done_callback) {
  DCHECK(!app_id.empty() && !file_handlers.empty());

  base::FilePath filename =
      shell_integration_linux::GetMimeTypesRegistrationFilename(profile_path,
                                                                app_id);
  std::string file_contents =
      shell_integration_linux::GetMimeTypesRegistrationFileContents(
          file_handlers);

  internals::GetShortcutIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(base::BindOnce(&UpdateMimeInfoDatabase, /*install=*/true),
                     std::move(filename), std::move(file_contents)),
      base::BindOnce(&OnMimeInfoDatabaseUpdated, /*install=*/true,
                     std::move(done_callback)));
}

void SetUpdateMimeInfoDatabaseOnLinuxCallbackForTesting(  // IN-TEST
    UpdateMimeInfoDatabaseOnLinuxCallback callback) {
  GetUpdateMimeInfoDatabaseCallbackForTesting() =  // IN-TEST
      std::move(callback);
}

}  // namespace web_app
