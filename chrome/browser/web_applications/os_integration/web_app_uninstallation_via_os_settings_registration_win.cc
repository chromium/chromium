// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/web_app_uninstallation_via_os_settings_registration.h"

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut_win.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/win/uninstallation_via_os_settings.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/install_static/install_util.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/obsolete/md5.h"

namespace web_app {

namespace internals {

// Deliberately not in namespace{} so it can be friended by
// crypto::obsolete::Md5.
std::wstring Md5AsHexForUninstall(const std::wstring& key) {
  return base::ASCIIToWide(base::HexEncodeLower(
      crypto::obsolete::Md5::Hash(base::as_byte_span(key))));
}

}  // namespace internals

namespace {

// Returns an identifier for the web app installed for the
// profile at `profile_path`. The identifier is guaranteed to be unique among
// all web apps installed in all profiles across all browser installations
// for the user.
std::wstring GetUninstallStringKey(const base::FilePath& profile_path,
                                   const webapps::AppId& app_id) {
  // We don't normalize (lower/upper) cases here mainly because people
  // don't change shortcut file case. If anyone changes the file name
  // or case, then it is the user's responsibility to clean up the apps.
  // (we assume he/she is a power user if they change the  system created
  // file.).
  std::wstring key =
      base::StrCat({profile_path.value(), base::ASCIIToWide(app_id)});
  return internals::Md5AsHexForUninstall(key);
}

// UninstallationViaOsSettingsHelper is a axilliary class for calculate the
// uninstallation registry key by |profile_path| and |app_id|.
class UninstallationViaOsSettingsHelper {
 public:
  UninstallationViaOsSettingsHelper(const base::FilePath& profile_path,
                                    const webapps::AppId& app_id)
      : profile_path_(profile_path), app_id_(app_id) {}

  UninstallationViaOsSettingsHelper(
      const UninstallationViaOsSettingsHelper& other) = delete;
  UninstallationViaOsSettingsHelper& operator=(
      const UninstallationViaOsSettingsHelper& other) = delete;

  base::CommandLine GetCommandLine() const {
    base::FilePath full_exe_name;

    base::PathService::Get(base::FILE_EXE, &full_exe_name);
    base::CommandLine uninstall_commandline(full_exe_name);

    // If the current user data directory isn't default, the uninstall
    // string should have it.
    base::FilePath user_data_dir;
    if (base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir) &&
        !chrome::IsUsingDefaultDataDirectory().value_or(true)) {
      uninstall_commandline.AppendSwitchNative(switches::kUserDataDir,
                                               user_data_dir.value());
    }

    base::FilePath profile_name = profile_path_.BaseName();
    uninstall_commandline.AppendSwitchNative(switches::kProfileDirectory,
                                             profile_name.value());

    uninstall_commandline.AppendSwitchASCII(switches::kUninstallAppId, app_id_);

    // e.g. uninstall_commandline
    // "C:\Users\account\AppData\Local\Microsoft\Chromium\
    //        Application\chrome.exe"
    // --user-data-dir=c:\users\account\appdata\local\chromium\
    //        CustomUserData (optional)
    // --profile-directory=Default
    // --uninstall-app-id=dadckofbdkccdemmkofcgkcbpjbnafgf
    return uninstall_commandline;
  }

  base::FilePath GetWebAppIconPath(const std::string& app_name) {
    base::FilePath web_app_icon_dir = GetOsIntegrationResourcesDirectoryForApp(
        profile_path_, app_id_, GURL());

    return internals::GetIconFilePath(web_app_icon_dir,
                                      base::UTF8ToUTF16(app_name));
  }

 private:
  const base::FilePath profile_path_;
  const webapps::AppId app_id_;
};

}  // namespace

std::wstring GetUninstallStringKeyForTesting(const base::FilePath& profile_path,
                                             const webapps::AppId& app_id) {
  return GetUninstallStringKey(profile_path, app_id);
}

bool ShouldRegisterUninstallationViaOsSettingsWithOs() {
  return true;
}

bool RegisterUninstallationViaOsSettingsWithOs(
    const webapps::AppId& app_id,
    const std::string& app_name,
    const base::FilePath& profile_path) {
  DCHECK(ShouldRegisterUninstallationViaOsSettingsWithOs());

  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  UninstallationViaOsSettingsHelper uninstall_os_settings_helper(profile_path,
                                                                 app_id);
  std::wstring hash_key = GetUninstallStringKey(profile_path, app_id);

  auto uninstall_commandline = uninstall_os_settings_helper.GetCommandLine();
  base::FilePath icon_path =
      uninstall_os_settings_helper.GetWebAppIconPath(app_name);
  std::wstring product_name = install_static::GetChromeInstallSubDirectory();

  return ::RegisterUninstallationViaOsSettings(
      hash_key, base::UTF8ToWide(app_name), product_name, uninstall_commandline,
      icon_path);
}

bool UnregisterUninstallationViaOsSettingsWithOs(
    const webapps::AppId& app_id,
    const base::FilePath& profile_path) {
  DCHECK(ShouldRegisterUninstallationViaOsSettingsWithOs());

  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  return ::UnregisterUninstallationViaOsSettings(
      GetUninstallStringKey(profile_path, app_id));
}

}  // namespace web_app
