// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/notification_helper/notification_helper_crash_reporter_client.h"

#include <memory>

#include "base/check.h"
#include "base/debug/leak_annotations.h"
#include "base/file_version_info.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chrome_version.h"
#include "chrome/install_static/install_util.h"
#include "chrome/install_static/user_data_dir.h"
#include "components/crash/core/app/crashpad.h"
#include "components/version_info/channel.h"

NotificationHelperCrashReporterClient::NotificationHelperCrashReporterClient() =
    default;

NotificationHelperCrashReporterClient::
    ~NotificationHelperCrashReporterClient() = default;

// static
void NotificationHelperCrashReporterClient::
    InitializeCrashReportingForProcessWithHandler(
        const base::FilePath& exe_path) {
  DCHECK(!exe_path.empty());

  static NotificationHelperCrashReporterClient* instance = nullptr;
  if (instance)
    return;

  instance = new NotificationHelperCrashReporterClient();
  ANNOTATE_LEAKING_OBJECT_PTR(instance);

  crash_reporter::SetCrashReporterClient(instance);

  std::wstring user_data_dir;
  install_static::GetUserDataDirectory(&user_data_dir, nullptr);

  crash_reporter::InitializeCrashpadWithEmbeddedHandler(
      true, "notification-helper", install_static::UTF16ToUTF8(user_data_dir),
      exe_path);
}

bool NotificationHelperCrashReporterClient::ShouldCreatePipeName(
    const base::string16& process_type) {
  return true;
}

bool NotificationHelperCrashReporterClient::GetAlternativeCrashDumpLocation(
    base::string16* crash_dir) {
  return false;
}

void NotificationHelperCrashReporterClient::GetProductNameAndVersion(
    const base::string16& exe_path,
    base::string16* product_name,
    base::string16* version,
    base::string16* special_build,
    base::string16* channel_name) {
  // Report crashes under the same product name as the browser. This string
  // MUST match server-side configuration.
  *product_name = base::ASCIIToUTF16(PRODUCT_SHORTNAME_STRING);

  std::unique_ptr<FileVersionInfo> version_info(
      FileVersionInfo::CreateFileVersionInfo(
          base::FilePath::FromUTF16Unsafe(exe_path)));
  if (version_info) {
    *version = version_info->product_version();
    *special_build = version_info->special_build();
  } else {
    *version = STRING16_LITERAL("0.0.0.0-devel");
    *special_build = base::string16();
  }

  *channel_name = base::WideToUTF16(install_static::GetChromeChannelName());
}

bool NotificationHelperCrashReporterClient::ShouldShowRestartDialog(
    base::string16* title,
    base::string16* message,
    bool* is_rtl_locale) {
  // There is no UX associated with notification_helper, so no dialog should be
  // shown.
  return false;
}

bool NotificationHelperCrashReporterClient::AboutToRestart() {
  // The notification_helper should never be restarted after a crash.
  return false;
}

bool NotificationHelperCrashReporterClient::GetIsPerUserInstall() {
  return !install_static::IsSystemInstall();
}

bool NotificationHelperCrashReporterClient::GetShouldDumpLargerDumps() {
  // Use large dumps for all but the stable channel.
  return install_static::GetChromeChannel() != version_info::Channel::STABLE;
}

int NotificationHelperCrashReporterClient::GetResultCodeRespawnFailed() {
  // The restart dialog is never shown for the notification_helper.
  NOTREACHED();
  return 0;
}

bool NotificationHelperCrashReporterClient::GetCrashDumpLocation(
    base::string16* crash_dir) {
  *crash_dir = base::WideToUTF16(install_static::GetCrashDumpLocation());
  return !crash_dir->empty();
}

bool NotificationHelperCrashReporterClient::GetCrashMetricsLocation(
    base::string16* metrics_dir) {
  std::wstring dir;
  install_static::GetUserDataDirectory(&dir, nullptr);
  *metrics_dir = base::WideToUTF16(dir);
  return !metrics_dir->empty();
}

bool NotificationHelperCrashReporterClient::IsRunningUnattended() {
  return install_static::HasEnvironmentVariable16(install_static::kHeadless);
}

bool NotificationHelperCrashReporterClient::GetCollectStatsConsent() {
  return install_static::GetCollectStatsConsent();
}

bool NotificationHelperCrashReporterClient::GetCollectStatsInSample() {
  return install_static::GetCollectStatsInSample();
}

bool NotificationHelperCrashReporterClient::ReportingIsEnforcedByPolicy(
    bool* enabled) {
  return install_static::ReportingIsEnforcedByPolicy(enabled);
}

bool NotificationHelperCrashReporterClient::
    ShouldMonitorCrashHandlerExpensively() {
  // The expensive mechanism dedicates a process to be crashpad_handler's own
  // crashpad_handler.
  return false;
}

bool NotificationHelperCrashReporterClient::EnableBreakpadForProcess(
    const std::string& process_type) {
  // This is not used by Crashpad (at least on Windows).
  NOTREACHED();
  return true;
}
