// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/gcp_crash_reporter_client.h"

#include "base/file_version_info.h"
#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "build/branding_buildflags.h"
#include "chrome/credential_provider/gaiacp/gcp_crash_reporting_utils.h"

namespace credential_provider {

GcpCrashReporterClient::~GcpCrashReporterClient() = default;

base::FilePath GcpCrashReporterClient::GetPathForFileVersionInfo(
    const base::string16& exe_path) {
  return base::FilePath(exe_path);
}

bool GcpCrashReporterClient::ShouldCreatePipeName(
    const base::string16& process_type) {
  return true;
}

bool GcpCrashReporterClient::GetAlternativeCrashDumpLocation(
    base::string16* crash_dir) {
  return false;
}

void GcpCrashReporterClient::GetProductNameAndVersion(
    const base::string16& exe_path,
    base::string16* product_name,
    base::string16* version,
    base::string16* special_build,
    base::string16* channel_name) {
  DCHECK(product_name);
  DCHECK(version);
  DCHECK(special_build);
  DCHECK(channel_name);
  // Report crashes under the same product name as the browser. This string
  // MUST match server-side configuration.
  *product_name = L"Chrome";
  special_build->clear();
  channel_name->clear();
  std::unique_ptr<FileVersionInfo> version_info(
      FileVersionInfo::CreateFileVersionInfo(
          GetPathForFileVersionInfo(exe_path)));

  if (version_info) {
    *version = version_info->product_version();
    *special_build = version_info->special_build();
  } else {
    *version = L"0.0.0.0-devel";
  }
}

bool GcpCrashReporterClient::ShouldShowRestartDialog(base::string16* title,
                                                     base::string16* message,
                                                     bool* is_rtl_locale) {
  // There is no UX associated with GCPW, so no dialog should be shown.
  return false;
}

bool GcpCrashReporterClient::AboutToRestart() {
  // GCPW should never be restarted after a crash.
  return false;
}

bool GcpCrashReporterClient::GetDeferredUploadsSupported(
    bool is_per_user_install) {
  return false;
}

bool GcpCrashReporterClient::GetIsPerUserInstall() {
  // GCPW can only be installed at system level.
  return false;
}

bool GcpCrashReporterClient::GetShouldDumpLargerDumps() {
  return false;
}

int GcpCrashReporterClient::GetResultCodeRespawnFailed() {
  // The restart dialog is never shown for GCPW.
  NOTREACHED();
  return 0;
}

bool GcpCrashReporterClient::GetCrashDumpLocation(base::string16* crash_dir) {
  base::FilePath crash_directory_path = GetFolderForCrashDumps();
  if (crash_directory_path.empty())
    return false;
  *crash_dir = crash_directory_path.value();
  return true;
}

bool GcpCrashReporterClient::IsRunningUnattended() {
  return false;
}

bool GcpCrashReporterClient::GetCollectStatsConsent() {
  return GetGCPWCollectStatsConsent();
}

bool GcpCrashReporterClient::EnableBreakpadForProcess(
    const std::string& process_type) {
  // This function is only called on Linux which the GCPW does not support.
  NOTREACHED();
  return false;
}

}  // namespace credential_provider
