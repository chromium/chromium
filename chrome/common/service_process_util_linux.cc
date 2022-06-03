// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <signal.h>
#include <unistd.h>

#include <memory>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/threading/platform_thread.h"
#include "build/branding_buildflags.h"
#include "chrome/common/auto_start_linux.h"
#include "chrome/common/multi_process_lock.h"
#include "chrome/common/service_process_util_posix.h"

namespace {

std::string GetBaseDesktopName() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return "google-chrome-service.desktop";
#else  // BUILDFLAG(CHROMIUM_BRANDING)
  return "chromium-service.desktop";
#endif
}
}  // namespace

std::unique_ptr<MultiProcessLock> TakeServiceRunningLock() {
  std::string lock_name =
      GetServiceProcessScopedName("_service_running");
  return TakeNamedLock(lock_name);
}

bool ForceServiceProcessShutdown(const std::string& version,
                                 base::ProcessId process_id) {
  if (kill(process_id, SIGTERM) < 0) {
    DPLOG(ERROR) << "kill";
    return false;
  }
  return true;
}

// Gets the name of the service process IPC channel.
// Returns an absolute path as required.
mojo::NamedPlatformChannel::ServerName GetServiceProcessServerName() {
  base::FilePath temp_dir;
  base::PathService::Get(base::DIR_TEMP, &temp_dir);
  std::string pipe_name = GetServiceProcessScopedVersionedName("_service_ipc");
  return temp_dir.Append(pipe_name).value();
}

bool CheckServiceProcessReady() {
  std::unique_ptr<MultiProcessLock> running_lock(TakeServiceRunningLock());
  return !running_lock.get();
}

bool ServiceProcessState::TakeSingletonLock() {
  state_->initializing_lock =
      TakeNamedLock(GetServiceProcessScopedName("_service_initializing"));
  return state_->initializing_lock.get();
}

bool ServiceProcessState::AddToAutoRun() {
  DCHECK(autorun_command_line_.get());
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  std::string app_name = "Google Chrome Service";
#else  // BUILDFLAG(CHROMIUM_BRANDING)
  std::string app_name = "Chromium Service";
#endif
  return AutoStart::AddApplication(
      GetServiceProcessScopedName(GetBaseDesktopName()),
      app_name,
      autorun_command_line_->GetCommandLineString(),
      false);
}

bool ServiceProcessState::RemoveFromAutoRun() {
  return AutoStart::Remove(
      GetServiceProcessScopedName(GetBaseDesktopName()));
}
