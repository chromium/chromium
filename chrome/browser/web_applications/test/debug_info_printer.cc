// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/debug_info_printer.h"

#include <string_view>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/web_app_internals/web_app_internals_handler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "chrome/browser/web_applications/web_app_utils.h"

#if BUILDFLAG(IS_MAC)
#include <inttypes.h>

#include "base/process/launch.h"
#include "base/strings/stringprintf.h"
#endif

namespace web_app::test {
namespace {
constexpr std::string_view kDisableLogDebugInfoToConsole =
    "disable-web-app-internals-log";
}  // namespace

void LogDebugInfoToConsole(const std::vector<Profile*>& profiles,
                           base::TimeDelta time_ago_for_system_log_capture) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kDisableLogDebugInfoToConsole)) {
    return;
  }

  // Tell users how to disable this potentially very long log output without
  // them having to find the code that produces it by themselves.
  std::string kDisableMessage =
      base::StrCat({"(you can disable printing this debug info using the --",
                    kDisableLogDebugInfoToConsole, " command line switch)"});

  for (Profile* profile : profiles) {
    if (!AreWebAppsEnabled(profile) ||
        !WebAppProviderFactory::IsServiceCreatedForProfile(profile)) {
      LOG(INFO) << "No WebAppProvider on profile" << profile->GetDebugName();
      continue;
    }
    base::RunLoop debug_info_loop;
    WebAppInternalsHandler::BuildDebugInfo(
        profile, base::BindLambdaForTesting([&](base::Value debug_info) {
          LOG(INFO) << "chrome://web-app-internals output for profile "
                    << profile->GetDebugName() << " " << kDisableMessage
                    << ":\n"
                    << debug_info.DebugString() << "\n"
                    << kDisableMessage;
          debug_info_loop.Quit();
        }));
    debug_info_loop.Run();
  }
  // On Mac OS also include system log output, as that is the only place logs
  // from app shims would end up. Do note that this log will include messages
  // from all tests that were running at the time, not just this test.
#if BUILDFLAG(IS_MAC)
  std::vector<std::string> log_argv = {
      "log",
      "show",
      "--process",
      "app_mode_loader",
      "--last",
      base::StringPrintf("%" PRId64 "s",
                         time_ago_for_system_log_capture.InSeconds() + 1)};
  std::string log_output;
  base::GetAppOutputAndError(log_argv, &log_output);
  LOG(INFO) << "System logs during this test run (could include other tests):\n"
            << log_output;
#endif
}

}  // namespace web_app::test
