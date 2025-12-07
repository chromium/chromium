// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chromeos/crosier/chromeos_integration_test_mixin.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "mojo/core/embedder/features.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/gl/gl_switches.h"

namespace {

constexpr char kUseFlagsPathName[] = "/etc/ui_use_flags.txt";
constexpr char kMojoIpczUseFlag[] = "ipcz";

}  // namespace

ChromeOSIntegrationTestMixin::ChromeOSIntegrationTestMixin(
    InProcessBrowserTestMixinHost* host)
    : InProcessBrowserTestMixin(host) {
  base::FilePath use_flags_path(kUseFlagsPathName);
  if (base::PathExists(use_flags_path)) {
    std::string data;
    base::ReadFileToString(use_flags_path, &data);

    std::vector<std::string> flags = base::SplitString(
        data, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (std::find(flags.begin(), flags.end(), kMojoIpczUseFlag) !=
        flags.end()) {
      // Enable MojoIpcz flag in the presence of ipcz USE flag to prevent
      // crbug.com/439917450.
      feature_list_.InitAndEnableFeature(mojo::core::kMojoIpcz);
    }
  }
}

ChromeOSIntegrationTestMixin::~ChromeOSIntegrationTestMixin() = default;

void ChromeOSIntegrationTestMixin::SetUpCommandLine(
    base::CommandLine* command_line) {
  // One of the main reason for using ChromeOS integration test is it can
  // verify graphics stack code path on DUT. So we want to enable GPU and pixel
  // outputs by default. This would also be required for pixel testing
  // and generating screenshots during test failures.
  command_line->AppendSwitch(switches::kEnablePixelOutputInTests);
  command_line->AppendSwitch(switches::kUseGpuInTests);
}

bool ChromeOSIntegrationTestMixin::SetUpUserDataDirectory() {
  // Always have --user-data-dir present in commandline arguments.
  // Without the argument, there are some permission issues. Here the logic
  // is: a temporary dir is created in base framework and set in PathService.
  // Then we retrieve the dir and append it to the commandline arguments.
  base::FilePath user_data_dir;
  CHECK(base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
  base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  if (!cmdline->HasSwitch(switches::kUserDataDir)) {
    cmdline->AppendSwitchPath(switches::kUserDataDir, user_data_dir);
  }
  return InProcessBrowserTestMixin::SetUpUserDataDirectory();
}
