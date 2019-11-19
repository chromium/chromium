// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/gcp_crash_reporting_utils.h"

#include <winerror.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "build/branding_buildflags.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gcp_crash_reporter_client.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "components/crash/core/common/crash_key.h"
#include "components/crash/core/common/crash_keys.h"

namespace {

constexpr wchar_t kCrashpadDumpsFolder[] = L"GCPW Crashpad";

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
void SetCurrentVersionCrashKey() {
  static crash_reporter::CrashKeyString<32> version_key("current-version");
  version_key.Clear();

  base::win::RegKey key(HKEY_LOCAL_MACHINE,
                        credential_provider::kRegUpdaterClientsAppPath,
                        KEY_QUERY_VALUE | KEY_WOW64_32KEY);

  // Read from the Clients key.
  base::string16 version_str;
  if (key.ReadValue(L"pv", &version_str) == ERROR_SUCCESS) {
    version_key.Set(base::WideToUTF8(version_str));
  }
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

// Returns the SYSTEM version of TEMP. We do this instead of GetTempPath so
// that both elevated and SYSTEM runs share the same directory.
base::FilePath GetSystemTempFolder() {
  base::win::RegKey reg_key(
      HKEY_LOCAL_MACHINE,
      L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment",
      KEY_QUERY_VALUE);

  base::string16 temp_string;
  if (reg_key.ReadValue(L"TEMP", &temp_string) != ERROR_SUCCESS)
    return base::FilePath();

  return base::FilePath(temp_string);
}

}  // namespace

namespace credential_provider {

void InitializeGcpwCrashReporting(GcpCrashReporterClient* crash_client) {
  crash_reporter::SetCrashReporterClient(crash_client);

  base::FilePath crash_dir = GetFolderForCrashDumps();

  if (crash_dir.empty() ||
      (!base::PathExists(crash_dir) && !base::CreateDirectory(crash_dir))) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    if (hr != HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS)) {
      LOGFN(ERROR) << "Failed to create directory for crash dumps: "
                   << crash_dir << " hr=" << putHR(hr);
    }
  }
}

base::FilePath GetFolderForCrashDumps() {
  base::FilePath system_temp_dir = GetSystemTempFolder();
  if (system_temp_dir.empty())
    // Failed to get a temp dir, something's gone wrong.
    return system_temp_dir;
  return system_temp_dir.Append(kCrashpadDumpsFolder);
}


void SetCommonCrashKeys(const base::CommandLine& command_line) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  SetCurrentVersionCrashKey();

  crash_keys::SetSwitchesFromCommandLine(command_line, nullptr);
#endif
}

bool GetGCPWCollectStatsConsent() {
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return false;
#else
  // This value is provided by Omaha during install based on how the installer
  // is tagged. The installer is tagged based on the consent checkbox found
  // on the download page.
  //
  // This value can also be changed after install by running the setup
  // program with --enable-stats or --disable-stats.
  //
  // This consent is different from chrome's consent in that each products
  // stores the consent in its own part of the registry.
  DWORD collect_stats = 0;
  base::win::RegKey key(HKEY_LOCAL_MACHINE,
                        credential_provider::kRegUpdaterClientStateAppPath,
                        KEY_QUERY_VALUE | KEY_WOW64_32KEY);
  key.ReadValueDW(kRegUsageStatsName, &collect_stats);
  return collect_stats == 1;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

}  // namespace credential_provider
