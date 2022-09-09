// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/web_app_uninstallation_via_os_settings_registration.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/hash/md5.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut_win.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/win/uninstallation_via_os_settings.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/install_static/install_util.h"
#include "content/public/browser/browser_thread.h"

namespace web_app {

namespace {

// UninstallationViaOsSettingsHelper is a axilliary class for calculate the
// uninstallation registry key by |profile_path| and |app_id|.
class UninstallationViaOsSettingsHelper {
 public:
  UninstallationViaOsSettingsHelper(const base::FilePath& profile_path,
                                    const AppId& app_id)
      : profile_path_(profile_path), app_id_(app_id) {}

  UninstallationViaOsSettingsHelper(
      const UninstallationViaOsSettingsHelper& other) = delete;
  UninstallationViaOsSettingsHelper& operator=(
      const UninstallationViaOsSettingsHelper& other) = delete;

  // Returns an identifier for the web app installed for the
  // profile at |profile_path|. The identifier is guaranteed to be unique among
  // all web apps installed in all profiles across all browser installations
  // for the user.
  std::wstring GetUninstallStringKey() const {
    // We don't normalize (lower/upper) cases here mainly because people
    // don't change shortcut file case. If anyone changes the file name
    // or case, then it is the user's responsibility for cleanup the apps.
    // (we assume he/she is a power user when could change
    // the system created file.).
    std::wstring key =
        base::StringPrintf(L"%ls_%ls", profile_path_.value().c_str(),
                           base::ASCIIToWide(app_id_).c_str());
    base::MD5Digest digest;
    base::MD5Sum(key.c_str(), key.size() * sizeof(wchar_t), &digest);
    return base::ASCIIToWide(base::MD5DigestToBase16(digest));
  }

  base::CommandLine GetCommandLine() const {
    base::FilePath full_exe_name;

    base::PathService::Get(base::FILE_EXE, &full_exe_name);
    base::CommandLine uninstall_commandline(full_exe_name);

    // If the current user data directory isn't default, the uninstall
    // string should have it.
    base::FilePath default_user_data_dir;
    base::FilePath user_data_dir;
    if (chrome::GetDefaultUserDataDirectory(&default_user_data_dir) &&
        base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir) &&
        default_user_data_dir != user_data_dir) {
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
  const AppId app_id_;
};

}  // namespace

bool ShouldRegisterUninstallationViaOsSettingsWithOs() {
  return true;
}

void RegisterUninstallationViaOsSettingsWithOs(const AppId& app_id,
                                               const std::string& app_name,
                                               Profile* profile) {
  DCHECK(ShouldRegisterUninstallationViaOsSettingsWithOs());

  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  UninstallationViaOsSettingsHelper uninstall_os_settings_helper(
      profile->GetPath(), app_id);
  std::wstring hash_key = uninstall_os_settings_helper.GetUninstallStringKey();

  auto uninstall_commandline = uninstall_os_settings_helper.GetCommandLine();
  base::FilePath icon_path =
      uninstall_os_settings_helper.GetWebAppIconPath(app_name);
  std::wstring product_name = install_static::GetChromeInstallSubDirectory();

  ::RegisterUninstallationViaOsSettings(hash_key, base::UTF8ToWide(app_name),
                                        product_name, uninstall_commandline,
                                        icon_path);
}

void UnegisterUninstallationViaOsSettingsWithOs(const AppId& app_id,
                                                Profile* profile) {
  DCHECK(ShouldRegisterUninstallationViaOsSettingsWithOs());

  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  UninstallationViaOsSettingsHelper uninstall_os_settings_helper(
      profile->GetPath(), app_id);
  std::wstring hash_key = uninstall_os_settings_helper.GetUninstallStringKey();
  ::UnregisterUninstallationViaOsSettings(hash_key);
}

}  // namespace web_app
