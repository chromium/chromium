// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/app/crash_reporter_client.h"

#include "build/branding_buildflags.h"
#include "build/build_config.h"

// On Windows don't use FilePath and logging.h.
// http://crbug.com/604923
#if !BUILDFLAG(IS_WIN)
#include "base/check.h"
#include "base/files/file_path.h"
#else
#include <assert.h>
#define DCHECK assert
#endif

namespace crash_reporter {

namespace {

CrashReporterClient* g_client = nullptr;

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && defined(OFFICIAL_BUILD)
const char kDefaultUploadURL[] = "https://clients2.google.com/cr/report";
#endif

}  // namespace

void SetCrashReporterClient(CrashReporterClient* client) {
  g_client = client;
}

CrashReporterClient* GetCrashReporterClient() {
  DCHECK(g_client);
  return g_client;
}

CrashReporterClient::CrashReporterClient() = default;
CrashReporterClient::~CrashReporterClient() = default;

#if !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_ANDROID)
void CrashReporterClient::SetCrashReporterClientIdFromGUID(
    const std::string& client_guid) {}
#endif

#if BUILDFLAG(IS_WIN)
bool CrashReporterClient::ShouldCreatePipeName(
    const std::wstring& process_type) {
  return process_type == L"browser";
}

bool CrashReporterClient::GetAlternativeCrashDumpLocation(
    std::wstring* crash_dir) {
  return false;
}

void CrashReporterClient::GetProductNameAndVersion(const std::wstring& exe_path,
                                                   std::wstring* product_name,
                                                   std::wstring* version,
                                                   std::wstring* special_build,
                                                   std::wstring* channel_name) {
}

bool CrashReporterClient::ShouldShowRestartDialog(std::wstring* title,
                                                  std::wstring* message,
                                                  bool* is_rtl_locale) {
  return false;
}

bool CrashReporterClient::AboutToRestart() {
  return false;
}

bool CrashReporterClient::GetIsPerUserInstall() {
  return true;
}

int CrashReporterClient::GetResultCodeRespawnFailed() {
  return 0;
}

std::wstring CrashReporterClient::GetWerRuntimeExceptionModule() {
  return std::wstring();
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_WIN) || (BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC))
bool CrashReporterClient::GetShouldDumpLargerDumps() {
  return false;
}
#endif

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
void CrashReporterClient::GetProductNameAndVersion(const char** product_name,
                                                   const char** version) {
}

void CrashReporterClient::GetProductNameAndVersion(std::string* product_name,
                                                   std::string* version,
                                                   std::string* channel) {}

base::FilePath CrashReporterClient::GetReporterLogFilename() {
  return base::FilePath();
}

bool CrashReporterClient::HandleCrashDump(const char* crashdump_filename,
                                          uint64_t crash_pid) {
  return false;
}
#endif

#if BUILDFLAG(IS_WIN)
bool CrashReporterClient::GetCrashDumpLocation(std::wstring* crash_dir) {
#else
bool CrashReporterClient::GetCrashDumpLocation(base::FilePath* crash_dir) {
#endif
  return false;
}

#if BUILDFLAG(IS_WIN)
bool CrashReporterClient::GetCrashMetricsLocation(std::wstring* crash_dir) {
#else
bool CrashReporterClient::GetCrashMetricsLocation(base::FilePath* crash_dir) {
#endif
  return false;
}

bool CrashReporterClient::IsRunningUnattended() {
  return true;
}

bool CrashReporterClient::GetCollectStatsConsent() {
  return false;
}

bool CrashReporterClient::GetCollectStatsInSample() {
  // By default, clients don't do sampling, so everything will be "in-sample".
  return true;
}

bool CrashReporterClient::ReportingIsEnforcedByPolicy(bool* breakpad_enabled) {
  return false;
}

#if BUILDFLAG(IS_ANDROID)
unsigned int CrashReporterClient::GetCrashDumpPercentage() {
  return 100;
}

bool CrashReporterClient::GetBrowserProcessType(std::string* ptype) {
  return false;
}

int CrashReporterClient::GetAndroidMinidumpDescriptor() {
  return 0;
}

int CrashReporterClient::GetAndroidCrashSignalFD() {
  return -1;
}

bool CrashReporterClient::ShouldEnableBreakpadMicrodumps() {
// Always enable microdumps on Android when stripping unwind tables. Rationale:
// when unwind tables are stripped out (to save binary size) the stack traces
// produced locally in the case of a crash / CHECK are meaningless. In order to
// provide meaningful development diagnostics (and keep the binary size savings)
// on Android we attach a secondary crash handler which serializes a reduced
// form of logcat on the console.
#if defined(NO_UNWIND_TABLES)
  return true;
#else
  return false;
#endif
}

bool CrashReporterClient::ShouldWriteMinidumpToLog() {
  return false;
}

#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
void CrashReporterClient::GetSanitizationInformation(
    const char* const** allowed_annotations,
    void** target_module,
    bool* sanitize_stacks) {
  *allowed_annotations = nullptr;
  *target_module = nullptr;
  *sanitize_stacks = false;
}
#endif

std::string CrashReporterClient::GetUploadUrl() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && defined(OFFICIAL_BUILD)
  return kDefaultUploadURL;
#else
  return std::string();
#endif
}

bool CrashReporterClient::ShouldMonitorCrashHandlerExpensively() {
  return false;
}

bool CrashReporterClient::EnableBreakpadForProcess(
    const std::string& process_type) {
  return false;
}

}  // namespace crash_reporter
