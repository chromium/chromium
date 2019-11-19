// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/setup/gcp_installer_crash_reporting.h"

#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "build/branding_buildflags.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gcp_crash_reporter_client.h"
#include "chrome/credential_provider/gaiacp/gcp_crash_reporting_utils.h"
#include "chrome/credential_provider/setup/setup_lib.h"
#include "components/crash/content/app/crashpad.h"
#include "components/crash/core/common/crash_key.h"
#include "components/crash/core/common/crash_keys.h"

namespace credential_provider {

void ConfigureGcpInstallerCrashReporting(
    const base::CommandLine& command_line) {
  // This is inspired by work done in various parts of Chrome startup to connect
  // to the crash service. Since the installer does not split its work between
  // a stub .exe and a main .dll, crash reporting can be configured in one place
  // right here.

  GcpCrashReporterClient* crash_client = new GcpCrashReporterClient();
  ANNOTATE_LEAKING_OBJECT_PTR(crash_client);

  InitializeGcpwCrashReporting(crash_client);

  crash_reporter::InitializeCrashpadWithEmbeddedHandler(true, "GCPW Installer",
                                                        "", base::FilePath());

  SetCommonCrashKeys(command_line);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  static crash_reporter::CrashKeyString<64> operation("operation");

  bool is_uninstall =
      command_line.HasSwitch(credential_provider::switches::kUninstall);

  operation.Set(is_uninstall ? "uninstall" : "install");
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

}  // namespace credential_provider
