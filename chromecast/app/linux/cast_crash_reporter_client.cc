// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/app/linux/cast_crash_reporter_client.h"

#include "base/time/time.h"
#include "chromecast/base/error_codes.h"
#include "chromecast/crash/linux/crash_util.h"
#include "components/crash/content/app/breakpad_linux.h"
#include "content/public/common/content_switches.h"
#include "services/service_manager/sandbox/switches.h"

namespace chromecast {

namespace {

uint64_t g_process_start_time_ms = 0u;

}  // namespace

// static
void CastCrashReporterClient::InitCrashReporter(
    const std::string& process_type) {
  DCHECK(!g_process_start_time_ms);
  g_process_start_time_ms =
      (base::TimeTicks::Now() - base::TimeTicks()).InMilliseconds();

  // Start up breakpad for this process, if applicable.
  breakpad::InitCrashReporter(process_type);
}

// static
uint64_t CastCrashReporterClient::GetProcessStartTime() {
  return g_process_start_time_ms;
}

CastCrashReporterClient::CastCrashReporterClient() {}

CastCrashReporterClient::~CastCrashReporterClient() {}

bool CastCrashReporterClient::EnableBreakpadForProcess(
    const std::string& process_type) {
  return process_type == switches::kRendererProcess ||
         process_type == service_manager::switches::kZygoteProcess ||
         process_type == switches::kGpuProcess ||
         process_type == switches::kUtilityProcess;
}

bool CastCrashReporterClient::HandleCrashDump(const char* crashdump_filename,
                                              uint64_t crash_pid) {
  // Set the initial error code to ERROR_WEB_CONTENT_RENDER_VIEW_GONE to show
  // an error message on next cast_shell run. Though the error code is for
  // renderer process crash, the actual messages can be used for browser process
  // as well.
  SetInitialErrorCode(ERROR_WEB_CONTENT_RENDER_VIEW_GONE);

  // Upload crash dump. If user didn't opt-in crash report, minidump writer
  // instantiated within CrashUtil::RequestUploadCrashDump() does nothing.
  CrashUtil::RequestUploadCrashDump(crashdump_filename, crash_pid,
                                    GetProcessStartTime());

  // Always return true to indicate that this crash dump has been processed,
  // so that it won't fallback to use chrome's default uploader.
  return true;
}

}  // namespace chromecast
