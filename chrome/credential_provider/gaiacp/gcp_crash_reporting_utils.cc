// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/gcp_crash_reporting_utils.h"

#include <winerror.h>

#include <string>

#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "build/branding_buildflags.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gcp_crash_reporter_client.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "components/crash/core/common/crash_key.h"
#include "components/crash/core/common/crash_keys.h"

namespace {

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
void SetCurrentVersionCrashKey() {
  static crash_reporter::CrashKeyString<32> version_key("current-version");
  version_key.Clear();

  base::win::RegKey key(HKEY_LOCAL_MACHINE,
                        credential_provider::kRegUpdaterClientsAppPath,
                        KEY_QUERY_VALUE | KEY_WOW64_32KEY);

  // Read from the Clients key.
  std::wstring version_str;
  if (key.ReadValue(L"pv", &version_str) == ERROR_SUCCESS) {
    version_key.Set(base::WideToUTF8(version_str));
  }
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace

namespace credential_provider {

base::FilePath GetFolderForCrashDumps() {
  base::FilePath data_dir = GetDataDirectory();
  if (data_dir.empty()) {
    return data_dir;
  }

  // Crashpad will create the directory as-needed.
  return data_dir.Append(FILE_PATH_LITERAL("Crashpad"));
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
