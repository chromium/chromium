// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "chrome/enterprise_companion/enterprise_companion.h"
#include "chrome/enterprise_companion/enterprise_companion_branding.h"

#if BUILDFLAG(IS_POSIX)
#include <unistd.h>
#elif BUILDFLAG(IS_WIN)
#include <shlobj.h>
#endif

namespace {

bool IsUserElevated() {
#if BUILDFLAG(IS_POSIX)
  return getuid() == 0;
#elif BUILDFLAG(IS_WIN)
  return ::IsUserAnAdmin();
#endif
}

std::optional<base::FilePath> GetLogFilePath() {
  const char* var = std::getenv("ISOLATED_OUTDIR");
  return var ? std::make_optional(
                   base::FilePath::FromUTF8Unsafe(var).AppendASCII(
                       "enterprise_companion_integration_test.log"))
             : std::nullopt;
}

}  // namespace

int main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(enterprise_companion::kLoggingModuleSwitch)) {
    command_line->AppendSwitchASCII(
        enterprise_companion::kLoggingModuleSwitch,
        enterprise_companion::kLoggingModuleSwitchValue);
  }

  logging::LoggingSettings settings{.logging_dest = logging::LOG_TO_STDERR};
  std::optional<base::FilePath> log_file_path = GetLogFilePath();
  if (log_file_path) {
    settings.log_file_path = log_file_path->value().c_str();
    settings.logging_dest |= logging::LOG_TO_FILE;
  }
  logging::InitLogging(settings);
  logging::SetLogItems(/*enable_process_id=*/true,
                       /*enable_thread_id=*/true,
                       /*enable_timestamp=*/true,
                       /*enable_tickcount=*/false);

  if (!IsUserElevated()) {
    LOG(ERROR) << "Integration tests must be run as root/Admin.";
    return 1;
  }

  // Assume all test bots have the {ISOLATED_OUTDIR} environment variable set.
  // Otherwise, don't run branded tests on a developer's system because doing so
  // can break the updater on the system.
  if (!std::getenv("ISOLATED_OUTDIR") &&
      std::strcmp(PRODUCT_FULLNAME_STRING, "ChromiumEnterpriseCompanion")) {
    LOG(ERROR) << "Running branded enterprise companion tests can break the "
                  "updater for the branded browser. If you don't care about "
                  "broken updaters and want to run the branded enterprise "
                  "companion tests locally, define an environment variable "
                  "ISOLATED_OUTDIR and set it to a local directory.";
    return 1;
  }

  base::TestSuite test_suite(argc, argv);
  return base::LaunchUnitTestsSerially(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
