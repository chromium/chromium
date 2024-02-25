// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/util/util.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/updater/util/mac_util.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
#include <shlobj.h>

#include "base/file_version_info_win.h"
#include "base/win/access_token.h"
#include "base/win/windows_types.h"
#endif

namespace updater {

// Tests the updater process returns 0 when run with --test argument.
TEST(UpdaterTest, UpdaterExitCode) {
  base::FilePath out_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &out_dir));
  const base::FilePath updater =
      out_dir.Append(updater::GetExecutableRelativePath());

  base::LaunchOptions options;
#if BUILDFLAG(IS_WIN)
  options.start_hidden = true;
#endif  // BUILDFLAG(IS_WIN)

  base::CommandLine command_line(updater);
  command_line.AppendSwitch("test");
  auto process = base::LaunchProcess(command_line, options);
  ASSERT_TRUE(process.IsValid());
  int exit_code = -1;
  EXPECT_TRUE(process.WaitForExitWithTimeout(TestTimeouts::action_timeout(),
                                             &exit_code));
  EXPECT_EQ(0, exit_code);
}

#if BUILDFLAG(IS_WIN)
// Tests that the updater test target version resource contains specific
// information to disambiguate the binary. For Windows builds and during tests,
// the "updater_test.exe" file is being installed as "updater.exe", therefore
// it is useful to have a hint of information available somewhere, to determine
// whether "updater.exe" is the test variant or the production variant.
TEST(UpdaterTest, UpdaterTestVersionResource) {
  const base::FilePath updater_test_name(FILE_PATH_LITERAL("updater_test.exe"));
  base::FilePath out_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &out_dir));
  const base::FilePath updater_test_path(out_dir.Append(updater_test_name));
  ASSERT_TRUE(base::PathExists(updater_test_path)) << updater_test_path;
  const std::unique_ptr<FileVersionInfoWin> version_info =
      FileVersionInfoWin::CreateFileVersionInfoWin(updater_test_path);
  ASSERT_NE(version_info, nullptr);
  EXPECT_EQ(version_info->original_filename(),
            updater_test_name.AsUTF16Unsafe());
}

// Checks that the unit test has the SE_DEBUG_NAME privilege when the process is
// running as Administrator.
TEST(UpdaterTest, UpdaterTestDebugPrivilege) {
  if (!::IsUserAnAdmin()) {
    return;
  }

  LUID luid = {0};
  ASSERT_TRUE(::LookupPrivilegeValue(nullptr, SE_DEBUG_NAME, &luid));

  CHROME_LUID chrome_luid = {0};
  chrome_luid.LowPart = luid.LowPart;
  chrome_luid.HighPart = luid.HighPart;
  const base::win::AccessToken::Privilege priv(chrome_luid,
                                               SE_PRIVILEGE_ENABLED);

  EXPECT_EQ(priv.GetName(), SE_DEBUG_NAME);
  EXPECT_EQ(priv.GetAttributes(), DWORD{SE_PRIVILEGE_ENABLED});
  EXPECT_TRUE(priv.IsEnabled());
}
#endif  // IS_WIN

}  // namespace updater
