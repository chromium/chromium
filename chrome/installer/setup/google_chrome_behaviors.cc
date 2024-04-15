// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <shellapi.h>

#include <memory>
#include <string_view>

#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/strcat_win.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "base/win/wmi.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/setup/brand_behaviors.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/install_util.h"
#include "components/metrics/metrics_pref_names.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"
#include "third_party/crashpad/crashpad/client/settings.h"
#include "third_party/crashpad/crashpad/util/misc/uuid.h"

namespace installer {

namespace {

constexpr std::wstring_view kUninstallSurveyUrl(
    L"https://support.google.com/chrome?p=chrome_uninstall_survey");

// Launches the url directly with the user's default handler for |url|.
bool NavigateToUrlWithHttps(const std::wstring& url) {
  SHELLEXECUTEINFO info = {sizeof(info)};
  info.fMask = SEE_MASK_NOASYNC;
  info.lpVerb = L"open";
  info.lpFile = url.c_str();
  info.nShow = SW_SHOWNORMAL;
  if (::ShellExecuteEx(&info))
    return true;
  PLOG(ERROR) << "Failed to launch default browser for uninstall survey";
  return false;
}

void NavigateToUrlWithIExplore(const std::wstring& url) {
  base::FilePath iexplore;
  if (!base::PathService::Get(base::DIR_PROGRAM_FILES, &iexplore))
    return;

  iexplore = iexplore.AppendASCII("Internet Explorer");
  iexplore = iexplore.AppendASCII("iexplore.exe");

  std::wstring command = L"\"" + iexplore.value() + L"\" " + url;

  int pid = 0;
  // The reason we use WMI to launch the process is because the uninstall
  // process runs inside a Job object controlled by the shell. As long as there
  // are processes running, the shell will not close the uninstall applet. WMI
  // allows us to escape from the Job object so the applet will close.
  base::win::WmiLaunchProcess(command, &pid);
}

// Returns true if the prefs dictionary located at |local_data_path| contains
// an enabled metrics pref.
// Note: Due to crbug.com/1052816, it is possible a subset of users may return
// false here even though they send UMA, as UMA can also check the registry.
bool IsMetricsEnabled(const base::FilePath& file_path) {
  JSONFileValueDeserializer json_deserializer(file_path);

  std::unique_ptr<base::Value> root =
      json_deserializer.Deserialize(nullptr, nullptr);
  // Preferences should always have a dictionary root.
  if (!root || !root->is_dict())
    return false;

  const std::optional<bool> value = root->GetDict().FindBoolByDottedPath(
      metrics::prefs::kMetricsReportingEnabled);

  return value.value_or(false);
}

}  // namespace

// If |archive_type| is INCREMENTAL_ARCHIVE_TYPE and |install_status| does not
// indicate a successful update, "-full" is appended to Chrome's "ap" value in
// its ClientState key if it is not present, resulting in the full installer
// being returned from the next update check. If |archive_type| is
// FULL_ARCHIVE_TYPE or |install_status| indicates a successful update, "-full"
// is removed from the "ap" value. "-stage:*" values are
// unconditionally removed from the "ap" value.
void UpdateInstallStatus(installer::ArchiveType archive_type,
                         installer::InstallStatus install_status) {
  GoogleUpdateSettings::UpdateInstallStatus(
      install_static::IsSystemInstall(), archive_type,
      InstallUtil::GetInstallReturnCode(install_status));
}

// Returns a string holding the following URL query parameters:
// - brand
// - client
// - ap
// - crash_client_id
std::wstring GetDistributionData() {
  std::wstring result;
  base::win::RegKey client_state_key(
      install_static::IsSystemInstall() ? HKEY_LOCAL_MACHINE
                                        : HKEY_CURRENT_USER,
      install_static::GetClientStateKeyPath().c_str(),
      KEY_QUERY_VALUE | KEY_WOW64_32KEY);
  std::wstring brand_value;
  if (client_state_key.ReadValue(google_update::kRegRLZBrandField,
                                 &brand_value) == ERROR_SUCCESS) {
    result.append(google_update::kRegRLZBrandField);
    result.append(L"=");
    result.append(brand_value);
    result.append(L"&");
  }

  std::wstring client_value;
  if (client_state_key.ReadValue(google_update::kRegClientField,
                                 &client_value) == ERROR_SUCCESS) {
    result.append(google_update::kRegClientField);
    result.append(L"=");
    result.append(client_value);
    result.append(L"&");
  }

  std::wstring ap_value;
  // If we fail to read the ap key, send up "&ap=" anyway to indicate
  // that this was probably a stable channel release.
  client_state_key.ReadValue(google_update::kRegApField, &ap_value);
  result.append(google_update::kRegApField);
  result.append(L"=");
  result.append(ap_value);

  // Crash client id.
  // While it would be convenient to use the path service to get
  // chrome::DIR_CRASH_DUMPS, that points to the dump location for the installer
  // rather than for the browser. For per-user installs they are the same, yet
  // for system-level installs the installer uses the system temp directory (see
  // setup/installer_crash_reporting.cc's ConfigureCrashReporting).
  // TODO(grt): use install_static::GetDefaultCrashDumpLocation (with an option
  // to suppress creating the directory) once setup.exe uses
  // install_static::InstallDetails.
  base::FilePath crash_dir;
  if (chrome::GetDefaultUserDataDirectory(&crash_dir)) {
    crash_dir = crash_dir.Append(FILE_PATH_LITERAL("Crashpad"));
    crashpad::UUID client_id;
    std::unique_ptr<crashpad::CrashReportDatabase> database(
        crashpad::CrashReportDatabase::InitializeWithoutCreating(crash_dir));
    if (database && database->GetSettings()->GetClientID(&client_id))
      result.append(L"&crash_client_id=").append(client_id.ToWString());
  }

  return result;
}

// Launches Edge or IE to show the uninstall survey. The following URL query
// params are included unconditionally in the survey URL:
// - crversion: the version of Chrome being uninstalled
// - os: Major.Minor.Build of the OS version
// If the user is sending crash reports and usage statistics to Google, the
// query params in |distribution_data| are included in the URL.
void DoPostUninstallOperations(const base::Version& version,
                               const base::FilePath& local_data_path,
                               const std::wstring& distribution_data) {
  // Send the Chrome version and OS version as params to the form. It would be
  // nice to send the locale, too, but I don't see an easy way to get that in
  // the existing code. It's something we can add later, if needed. We depend
  // on installed_version.GetString() not having spaces or other characters that
  // need escaping: 0.2.13.4. Should that change, we will need to escape the
  // string before using it in a URL.
  const base::win::OSInfo* os_info = base::win::OSInfo::GetInstance();
  base::win::OSInfo::VersionNumber version_number = os_info->version_number();
  std::wstring os_version =
      base::StrCat({base::NumberToWString(version_number.major), L".",
                    base::NumberToWString(version_number.minor), L".",
                    base::NumberToWString(version_number.build)});

  const std::wstring survey_url = std::wstring(kUninstallSurveyUrl);
#if DCHECK_IS_ON()
  // The URL is expected to have a query part and not end with '&'.
  const size_t pos = survey_url.find(L'?');
  DCHECK_NE(pos, std::wstring::npos);
  DCHECK_EQ(survey_url.find(L'?', pos + 1), std::wstring::npos);
  DCHECK_NE(survey_url.back(), L'&');
#endif
  auto url = base::StrCat({survey_url, L"&crversion=",
                           base::ASCIIToWide(version.GetString()), L"&os=",
                           os_version});

  if (!distribution_data.empty() && IsMetricsEnabled(local_data_path)) {
    url += L"&";
    url += distribution_data;
  }

  if (os_info->version() < base::win::Version::WIN10 ||
      !NavigateToUrlWithHttps(url)) {
    NavigateToUrlWithIExplore(url);
  }
}

}  // namespace installer
