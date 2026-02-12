// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <optional>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/logging/logging_settings.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/win/access_token.h"
#include "build/branding_buildflags.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/isolation_support.h"

namespace elevation_service {

int RunTest() {
  auto process_token = base::win::AccessToken::FromCurrentProcess();
  if (!process_token) {
    return -1;
  }

  auto sa = process_token->GetSecurityAttribute(
      installer::GetIsolationAttributeName());

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  const bool expect_sa = true;
#else
  const bool expect_sa = false;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (sa.has_value() != expect_sa) {
    return -2;
  }
  const auto* cmd_line = base::CommandLine::ForCurrentProcess();

  // A dangerous switch is filtered.
  if (cmd_line->HasSwitch(::switches::kDisableComponentUpdate)) {
    return -3;
  }

  // A safe switch is allowed.
  const auto udd = cmd_line->GetSwitchValuePath(::switches::kUserDataDir);
  if (udd.empty() || !base::PathExists(udd)) {
    return -4;
  }

  // The elevated service adds the switch --isolated.
  if (!cmd_line->HasSwitch(::switches::kIsolated)) {
    return -5;
  }

  const auto args = cmd_line->GetArgs();
  if (args.size() != 2) {
    return -6;
  }

  if (args[1] != L"another_arg") {
    return -7;
  }

  // This HANDLE leaks, but the process is terminated soon anyway, so it doesn't
  // matter.
  ::SetEvent(::OpenEventW(EVENT_MODIFY_STATE, /*bInheritHandle=*/FALSE,
                          std::data(args[0])));

  // The test parent process will kill this process.
  base::PlatformThread::Sleep(TestTimeouts::action_timeout());

  return 0;
}

}  // namespace elevation_service

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  logging::InitLogging(logging::LoggingSettings{
      .logging_dest =
          logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR});

  return elevation_service::RunTest();
}
