// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/resource.h>

#include <memory>
#include <utility>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/task/single_thread_task_executor.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "chromecast/base/cast_paths.h"
#include "chromecast/base/cast_sys_info_util.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/crash/linux/minidump_uploader.h"
#include "chromecast/public/cast_sys_info.h"
#include "chromecast/system/reboot/reboot_util.h"
#include "third_party/crashpad/crashpad/client/crashpad_info.h"

namespace {

// Upload crash dump for every 60 seconds.
const int kUploadRetryIntervalDefault = 60;

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager exit_manager;
  base::CommandLine::Init(argc, argv);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  chromecast::RebootUtil::Initialize(command_line->argv());
  chromecast::RegisterPathProvider();
  logging::InitLogging(logging::LoggingSettings());

  // Allow the system crash handler to handle our own crashes.
  crashpad::CrashpadInfo::GetCrashpadInfo()
      ->set_system_crash_reporter_forwarding(crashpad::TriState::kEnabled);

  LOG(INFO) << "Starting crash uploader...";

  // Nice +19.  Crash uploads are not time critical and we don't want to
  // interfere with user playback.
  setpriority(PRIO_PROCESS, 0, 19);

  // Create the main task executor.
  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);

  std::unique_ptr<chromecast::CastSysInfo> sys_info =
      chromecast::CreateSysInfo();

  std::string server_url(
      command_line->GetSwitchValueASCII(switches::kCrashServerUrl));
  bool daemon =
      chromecast::GetSwitchValueBoolean(switches::kCrashUploaderDaemon, false);
  LOG_IF(INFO, daemon) << "Running crash uploader in daemon-mode";

  chromecast::MinidumpUploader uploader(sys_info.get(), server_url);
  do {
    if (!uploader.UploadAllMinidumps())
      LOG(ERROR) << "Failed to process minidumps";

    if (uploader.reboot_scheduled())
      chromecast::RebootUtil::RebootNow(
          chromecast::RebootShlib::CRASH_UPLOADER);

    if (daemon) {
      base::PlatformThread::Sleep(base::Seconds(kUploadRetryIntervalDefault));
    }
  } while (daemon);

  return EXIT_SUCCESS;
}
