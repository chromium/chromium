// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/installer_crash_reporter_client.h"

#include "base/environment.h"
#include "base/file_version_info.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "build/branding_buildflags.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_version.h"
#include "chrome/common/env_vars.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/setup/installer_crash_reporting.h"
#include "chrome/installer/util/google_update_settings.h"
#include "components/version_info/channel.h"

InstallerCrashReporterClient::InstallerCrashReporterClient(
    bool is_per_user_install)
    : is_per_user_install_(is_per_user_install) {}

InstallerCrashReporterClient::~InstallerCrashReporterClient() = default;

bool InstallerCrashReporterClient::ShouldCreatePipeName(
    const std::wstring& process_type) {
  return true;
}

bool InstallerCrashReporterClient::GetAlternativeCrashDumpLocation(
    std::wstring* crash_dir) {
  return false;
}

void InstallerCrashReporterClient::GetProductNameAndVersion(
    const std::wstring& exe_path,
    std::wstring* product_name,
    std::wstring* version,
    std::wstring* special_build,
    std::wstring* channel_name) {
  // Report crashes under the same product name as the browser. This string
  // MUST match server-side configuration.
  *product_name = base::ASCIIToWide(PRODUCT_SHORTNAME_STRING);

  std::unique_ptr<FileVersionInfo> version_info(
      FileVersionInfo::CreateFileVersionInfo(base::FilePath(exe_path)));
  if (version_info) {
    *version = base::AsWString(version_info->product_version());
    *special_build = base::AsWString(version_info->special_build());
  } else {
    *version = L"0.0.0.0-devel";
  }

  *channel_name =
      install_static::GetChromeChannelName(/*with_extended_stable=*/true);
}

bool InstallerCrashReporterClient::ShouldShowRestartDialog(
    std::wstring* title,
    std::wstring* message,
    bool* is_rtl_locale) {
  // There is no UX associated with the installer, so no dialog should be shown.
  return false;
}

bool InstallerCrashReporterClient::AboutToRestart() {
  // The installer should never be restarted after a crash.
  return false;
}

bool InstallerCrashReporterClient::GetIsPerUserInstall() {
  return is_per_user_install_;
}

bool InstallerCrashReporterClient::GetShouldDumpLargerDumps() {
  // Use large dumps for all but the stable channel.
  return install_static::GetChromeChannel() != version_info::Channel::STABLE;
}

int InstallerCrashReporterClient::GetResultCodeRespawnFailed() {
  // The restart dialog is never shown for the installer.
  NOTREACHED_IN_MIGRATION();
  return 0;
}

bool InstallerCrashReporterClient::GetCrashDumpLocation(
    std::wstring* crash_dir) {
  base::FilePath crash_directory_path;
  bool ret =
      base::PathService::Get(chrome::DIR_CRASH_DUMPS, &crash_directory_path);
  if (ret)
    *crash_dir = crash_directory_path.value();
  return ret;
}

bool InstallerCrashReporterClient::IsRunningUnattended() {
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  return env->HasVar(env_vars::kHeadless);
}

bool InstallerCrashReporterClient::GetCollectStatsConsent() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return GoogleUpdateSettings::GetCollectStatsConsent();
#else
  return false;
#endif
}

bool InstallerCrashReporterClient::GetCollectStatsInSample() {
  // TODO(grt): remove duplication of code.
  base::win::RegKey key(HKEY_CURRENT_USER,
                        install_static::GetRegistryPath().c_str(),
                        KEY_QUERY_VALUE | KEY_WOW64_32KEY);
  if (!key.Valid())
    return true;
  DWORD out_value = 0;
  if (key.ReadValueDW(install_static::kRegValueChromeStatsSample, &out_value) !=
      ERROR_SUCCESS) {
    return true;
  }
  return out_value == 1;
}

bool InstallerCrashReporterClient::ReportingIsEnforcedByPolicy(bool* enabled) {
  // From the generated policy/policy/policy_constants.cc:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  static const wchar_t kRegistryChromePolicyKey[] =
      L"SOFTWARE\\Policies\\Google\\Chrome";
#else
  static const wchar_t kRegistryChromePolicyKey[] =
      L"SOFTWARE\\Policies\\Chromium";
#endif
  static const wchar_t kMetricsReportingEnabled[] = L"MetricsReportingEnabled";

  // Determine whether configuration management allows loading the crash
  // reporter. Since the configuration management infrastructure is not
  // initialized in the installer, the corresponding registry keys are read
  // directly. The return status indicates whether policy data was successfully
  // read. If it is true, |enabled| contains the value set by policy.
  DWORD value = 0;
  base::win::RegKey policy_key;
  static const HKEY kHives[] = {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER};
  for (HKEY hive : kHives) {
    if (policy_key.Open(hive, kRegistryChromePolicyKey, KEY_READ) ==
            ERROR_SUCCESS &&
        policy_key.ReadValueDW(kMetricsReportingEnabled, &value) ==
            ERROR_SUCCESS) {
      *enabled = value != 0;
      return true;
    }
  }

  return false;
}

bool InstallerCrashReporterClient::EnableBreakpadForProcess(
    const std::string& process_type) {
  return true;
}
