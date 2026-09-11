// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <array>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "base/apple/foundation_util.h"
#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/environment.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/installer/mac/install_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace {

using ::installer::mac::test::ExecutionRecord;
using ::installer::mac::test::ProcessExitedWithValue;
using ::installer::mac::test::ProcessNotValid;
using ::installer::mac::test::ProcessStatus;
using ::installer::mac::test::ProcessStillRunning;
using ::installer::mac::test::ProcessStoppedWithSignal;
using ::installer::mac::test::ProcessTerminatedWithSignal;
using ::installer::mac::test::ReplaceAll;
using ::installer::mac::test::RunWithTimeout;

constexpr base::TimeDelta kScriptTimeout = base::Minutes(1);

class KeystoneInstallTest : public testing::Test {
 protected:
  base::FilePath dest_contents_path() const {
    return app_install_dir_.AppendUTF8("Contents");
  }

  base::FilePath dest_info_plist_path() const {
    return dest_contents_path().AppendUTF8("Info.plist");
  }

  base::FilePath dest_versioned_path(const std::string& version) const {
    return dest_contents_path().AppendUTF8("Versions").AppendUTF8(version);
  }

  void SetUp() override {
    ASSERT_TRUE(temp_.CreateUniqueTempDir());
    script_temp_ = temp_.GetPath().AppendUTF8("script_tmp");

    // Create a fake app bundle
    mount_dir_ = temp_.GetPath().AppendUTF8("mount");
    base::CreateDirectory(mount_dir_);
    base::CreateDirectory(mount_dir_.AppendUTF8("Google Chrome.app")
                              .AppendUTF8("Contents")
                              .AppendUTF8("Versions")
                              .AppendUTF8("1")
                              .AppendUTF8("Google Chrome Framework.framework")
                              .AppendUTF8("Resources"));
    SetInfoPlistItem(mount_dir_, "CFBundleShortVersionString", "1");
    SetInfoPlistItem(mount_dir_, "KSProductID", "com.google.Chrome");
    SetInfoPlistItem(mount_dir_, "KSVersion", "2");
    SetInfoPlistItem(mount_dir_, "KSUpdateURL", "https://example");
    base::FilePath pkg_info_path = mount_dir_.AppendUTF8("Google Chrome.app")
                                       .AppendUTF8("Contents")
                                       .AppendUTF8("PkgInfo");
    // PkgInfo content is copied from Chrome; its specific value is not relevant
    // to this test. Presence of the file helps test top-level app copying.
    ASSERT_TRUE(base::WriteFile(pkg_info_path, "APPLrimZ"));

    // Create the destination directory.
    app_install_dir_ = temp_.GetPath().AppendUTF8("dest");
    base::CreateDirectory(app_install_dir_);

    // Create a fake ksadmin.
    std::string ksadmin = ReplaceAll(
        R"(#!/bin/bash
set -e
if [ "${1}" = "--ksadmin-version" ] ; then
  # version check
  echo "137.0.0.0"
  exit 0
fi
if [ "${1}" = "-pP" ] ; then
  # finding app to update
  echo " xc=<KSPathExistenceChecker:0x45 path=@DEST@>"
  exit 0
fi
# otherwise, just save args
while (( "$#" )) ; do
  echo -n "${1} " >> "@OUTFILE@"
  shift
done
echo "\n" >> "@OUTFILE@"
exit 0
)",
        {{"@DEST@", app_install_dir_.AsUTF8Unsafe()},
         {"@OUTFILE@",
          temp_.GetPath().AppendUTF8("ksadmin_out").AsUTF8Unsafe()}});
    base::CreateDirectory(temp_.GetPath().AppendUTF8("ksadmin-bin"));
    ASSERT_TRUE(base::WriteFile(
        temp_.GetPath().AppendUTF8("ksadmin-bin").AppendUTF8("ksadmin"),
        ksadmin));
    ASSERT_TRUE(base::SetPosixFilePermissions(
        temp_.GetPath().AppendUTF8("ksadmin-bin").AppendUTF8("ksadmin"),
        base::FILE_PERMISSION_READ_BY_USER |
            base::FILE_PERMISSION_WRITE_BY_USER |
            base::FILE_PERMISSION_EXECUTE_BY_USER));
  }

  base::FilePath GetBrandFilePath() const {
    return base::apple::GetUserLibraryPath().AppendUTF8("Google").AppendUTF8(
        "Google Chrome Brand");
  }

  void TearDown() override { base::DeleteFile(GetBrandFilePath()); }

  base::EnvironmentMap GetDefaultEnvironment() {
    base::FilePath home;
    if (!base::PathService::Get(base::DIR_HOME, &home)) {
      ADD_FAILURE() << "Test issue: Cannot get home directory from PathService";
      return {};
    }
    return {
        {"KS_TICKET_AP", "ap"},
        {"KS_TICKET_SERVER_URL", "https://exampleserverurl"},
        {"KS_TICKET_XC_PATH", app_install_dir_.AsUTF8Unsafe()},
        {"PATH",
         base::StrCat(
             {"/bin:/usr/bin:",
              temp_.GetPath().AppendUTF8("ksadmin-bin").AsUTF8Unsafe()})},
        {"PREVIOUS_VERSION", "1.0.0.0"},
        {"SERVER_ARGS", ""},
        {"UPDATE_IS_MACHINE", "0"},
        {"UNPACK_DIR", mount_dir_.AsUTF8Unsafe()},
        {"GOOGLE_USAGE_STATS_ENABLED", "0"},
        {"GOOGLE_CHROME_UPDATER_DEBUG", "1"},
        {"GOOGLE_CHROME_UPDATER_TEST_ENROLLMENT_PATH",
         temp_.GetPath().AppendUTF8("Enrollment").AsUTF8Unsafe()},
        {"TMPDIR", script_temp_.AsUTF8Unsafe()},
        {base::env_vars::kHome, home.AsUTF8Unsafe()},
    };
  }

  void PrependToEnvPath(base::EnvironmentMap& env,
                        const base::FilePath& extra_path) {
    ASSERT_FALSE(extra_path.empty());
    env["PATH"] = base::StrCat({extra_path.AsUTF8Unsafe(), ":", env["PATH"]});
  }

  void RunInstallScript(
      ProcessStatus want_status,
      std::optional<base::FilePath> prepend_path = std::nullopt,
      base::ProcessId* pid_out = nullptr) {
    base::FilePath ksinstall;
    ASSERT_TRUE(
        base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &ksinstall));
    ksinstall = ksinstall.AppendUTF8("chrome")
                    .AppendUTF8("installer")
                    .AppendUTF8("mac")
                    .AppendUTF8("keystone_install.sh");
    base::CommandLine cmd(ksinstall);
    cmd.AppendArgPath(mount_dir_);
    cmd.AppendArgPath(app_install_dir_);
    cmd.AppendArg("1.0.0.0");  // Previous version.

    base::EnvironmentMap env = GetDefaultEnvironment();
    if (prepend_path) {
      ASSERT_NO_FATAL_FAILURE(PrependToEnvPath(env, *prepend_path));
    }
    ExecutionRecord result =
        RunWithTimeout(cmd, env, mount_dir_, kScriptTimeout);
    if (pid_out) {
      *pid_out = result.pid;
    }
    ASSERT_EQ(result.status, want_status) << result.combined_output;
  }

  void SetInfoPlistItem(base::FilePath dir,
                        const std::string& key,
                        const std::string& value) {
    SetPlistItem(dir.AppendUTF8("Google Chrome.app")
                     .AppendUTF8("Contents")
                     .AppendUTF8("Info"),
                 key, value);
  }

  void SetLibraryBrand(const std::string& brand) {
    SetPlistItem(GetBrandFilePath(), "KSBrandID", brand);
  }

  // TODO(crbug.com/546260882): Use NSPropertyListSerialization instead.
  void SetPlistItem(base::FilePath plist,
                    const std::string& key,
                    const std::string& value) {
    std::string output;
    base::CommandLine cmd(base::FilePath("defaults"));
    cmd.AppendArg("write");
    cmd.AppendArgPath(plist);
    cmd.AppendArg(key);
    cmd.AppendArg("-string");
    cmd.AppendArg(value);
    ASSERT_TRUE(base::GetAppOutput(cmd, &output)) << output;
  }

  // TODO(crbug.com/546260882): Use NSPropertyListSerialization instead.
  std::string ReadPlistItem(const base::FilePath& plist,
                            const std::string& key) {
    base::CommandLine cmd(base::FilePath("defaults"));
    cmd.AppendArg("read");
    cmd.AppendArgPath(plist);
    cmd.AppendArg(key);
    ExecutionRecord result =
        RunWithTimeout(cmd, {{"__CFPREFERENCES_AVOID_DAEMON", "1"}}, mount_dir_,
                       kScriptTimeout);
    EXPECT_EQ(result.status, ProcessExitedWithValue{0})
        << result.combined_output;
    return result.combined_output;
  }

  std::string ReadLibraryBrand() {
    return ReadPlistItem(GetBrandFilePath(), "KSBrandID");
  }

  std::string GetLastKSAdminArgs() {
    std::string args;
    EXPECT_TRUE(base::ReadFileToString(
        temp_.GetPath().AppendUTF8("ksadmin_out"), &args));
    return args;
  }

  void SetEnrolled(bool enroll) {
    const base::FilePath enrollment = temp_.GetPath().AppendUTF8("Enrollment");
    if (enroll) {
      EXPECT_TRUE(base::WriteFile(enrollment,
                                  "fake from keystone_install_unittest.cc"));
    } else {
      EXPECT_TRUE(base::DeleteFile(enrollment));
    }
  }

  base::ScopedTempDir temp_;
  base::FilePath script_temp_;
  base::FilePath mount_dir_;
  base::FilePath app_install_dir_;
};

TEST_F(KeystoneInstallTest, RunScript) {
  ASSERT_NO_FATAL_FAILURE(RunInstallScript(ProcessExitedWithValue{0}));
}

TEST_F(KeystoneInstallTest, CBCMBrandSubstitution) {
  SetEnrolled(true);

  ASSERT_NO_FATAL_FAILURE(SetLibraryBrand("GCEA"));
  ASSERT_NO_FATAL_FAILURE(RunInstallScript(ProcessExitedWithValue{0}));
  ASSERT_EQ(ReadLibraryBrand(), "GCCA\n");

  ASSERT_NO_FATAL_FAILURE(SetLibraryBrand("FPAZ"));
  ASSERT_NO_FATAL_FAILURE(RunInstallScript(ProcessExitedWithValue{0}));
  ASSERT_EQ(ReadLibraryBrand(), "FPJZ\n");
}

TEST_F(KeystoneInstallTest, CBCMReverseBrandSubstitution) {
  SetEnrolled(false);

  ASSERT_NO_FATAL_FAILURE(SetLibraryBrand("GCCA"));
  ASSERT_NO_FATAL_FAILURE(RunInstallScript(ProcessExitedWithValue{0}));
  ASSERT_EQ(ReadLibraryBrand(), "GCEA\n");

  ASSERT_NO_FATAL_FAILURE(SetLibraryBrand("FPJZ"));
  ASSERT_NO_FATAL_FAILURE(RunInstallScript(ProcessExitedWithValue{0}));
  ASSERT_EQ(ReadLibraryBrand(), "FPAZ\n");
}

TEST_F(KeystoneInstallTest, CBCMBrandSubstitutionNoOpUnenrolled) {
  SetEnrolled(false);

  ASSERT_NO_FATAL_FAILURE(SetLibraryBrand("GCEA"));
  ASSERT_NO_FATAL_FAILURE(RunInstallScript(ProcessExitedWithValue{0}));
  ASSERT_EQ(ReadLibraryBrand(), "GCEA\n");

  ASSERT_NO_FATAL_FAILURE(SetLibraryBrand("FPAZ"));
  ASSERT_NO_FATAL_FAILURE(RunInstallScript(ProcessExitedWithValue{0}));
  ASSERT_EQ(ReadLibraryBrand(), "FPAZ\n");
}

TEST_F(KeystoneInstallTest, CBCMBrandSubstitutionNoOpEnrolled) {
  SetEnrolled(true);

  ASSERT_NO_FATAL_FAILURE(SetLibraryBrand("GCCA"));
  ASSERT_NO_FATAL_FAILURE(RunInstallScript(ProcessExitedWithValue{0}));
  ASSERT_EQ(ReadLibraryBrand(), "GCCA\n");

  ASSERT_NO_FATAL_FAILURE(SetLibraryBrand("FPJZ"));
  ASSERT_NO_FATAL_FAILURE(RunInstallScript(ProcessExitedWithValue{0}));
  ASSERT_EQ(ReadLibraryBrand(), "FPJZ\n");

  ASSERT_NO_FATAL_FAILURE(SetLibraryBrand("GGLS"));
  ASSERT_NO_FATAL_FAILURE(RunInstallScript(ProcessExitedWithValue{0}));
  ASSERT_EQ(ReadLibraryBrand(), "GGLS\n");
}

// keystone_install_test.sh contains a number of tests for keystone_install.sh.
// They could eventually be migrated to this C++ test fixture, but for now just
// run the existing script.
TEST_F(KeystoneInstallTest, RunTestScript) {
  std::string output;
  base::FilePath ksinstall_test;
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &ksinstall_test));
  ksinstall_test = ksinstall_test.AppendUTF8("chrome")
                       .AppendUTF8("installer")
                       .AppendUTF8("mac")
                       .AppendUTF8("keystone_install_test.sh");
  ASSERT_TRUE(
      base::GetAppOutputAndError(base::CommandLine(ksinstall_test), &output))
      << output;
}

std::string MakeInterceptorScript(std::string_view pre_tool_hook) {
  return base::StrCat({R"-(#!/bin/bash
set -e
# Remove the interceptor from PATH by removing the first entry.
PATH="$(sed -e 's/^[^:]*://' <<< "${PATH}")"
)-",
                       pre_tool_hook, "\n",
                       R"-(exec "$(basename "$0")" "$@")-"});
}

// Intercepts and controls rsync execution using a wrapper; test setup must
// ensure that the RsyncInterceptor's `bin_dir()` appears on the target's PATH
// before the real rsync.
//
// Rsync interception serves as a synchronization point for testing behaviors
// around interruptions during critical parts of the installation process.
class RsyncInterceptor {
 public:
  explicit RsyncInterceptor(const base::FilePath& temp_dir,
                            std::string_view pre_rsync_hook)
      : bin_dir_(temp_dir.AppendUTF8("rsync-bin")),
        rsync_script_(MakeInterceptorScript(pre_rsync_hook)) {}

  RsyncInterceptor(const RsyncInterceptor&) = delete;
  RsyncInterceptor& operator=(const RsyncInterceptor&) = delete;

  ~RsyncInterceptor() { TearDown(); }

  void SetUp() {
    ASSERT_TRUE(base::CreateDirectory(bin_dir_));
    mock_rsync_path_ = bin_dir_.AppendUTF8("rsync");
    ASSERT_TRUE(base::WriteFile(mock_rsync_path_, rsync_script_));
    ASSERT_TRUE(base::SetPosixFilePermissions(
        mock_rsync_path_, base::FILE_PERMISSION_READ_BY_USER |
                              base::FILE_PERMISSION_WRITE_BY_USER |
                              base::FILE_PERMISSION_EXECUTE_BY_USER));
  }

  void TearDown() { base::DeleteFile(mock_rsync_path_); }

  const base::FilePath& bin_dir() const { return bin_dir_; }

 private:
  base::FilePath bin_dir_;
  base::FilePath mock_rsync_path_;
  std::string rsync_script_;
};

TEST_F(KeystoneInstallTest, VerifyProcessGroupSignalDeferral) {
  base::FilePath pgid_path = temp_.GetPath().AppendUTF8("pgid.out");
  base::FilePath fail_path = temp_.GetPath().AppendUTF8("fail.out");
  RsyncInterceptor interceptor(
      temp_.GetPath(), installer::mac::test::ReplaceAll(
                           R"-(
if [[ "$*" == *"--include"* ]] &&
    [[ "$*" == *"--exclude"* ]] &&
    [[ "$*" == *"/Current"* ]] ; then
  if [[ -f "@PGID_FILE@" ]]; then
    local msg="Unexpected extra matching rsync args: $*"
    echo "${msg}" >> "@FAIL_FILE@"
    echo "${msg}" >& 2
    exit 100
  fi
  PGID=$(ps -o pgid= -p "$$")
  PGID=${PGID//[[:space:]]/}
  echo "${PGID}" >> "@PGID_FILE@"
  kill -s TERM -- "-${PGID}"
fi)-",
                           {{"@PGID_FILE@", pgid_path.AsUTF8Unsafe()},
                            {"@FAIL_FILE@", fail_path.AsUTF8Unsafe()}}));
  interceptor.SetUp();

  base::ProcessId pid = 0;
  EXPECT_NO_FATAL_FAILURE(RunInstallScript(ProcessTerminatedWithSignal{SIGTERM},
                                           interceptor.bin_dir(), &pid));
  if (base::PathExists(fail_path)) {
    std::string fail_msg;
    if (!base::ReadFileToString(fail_path, &fail_msg)) {
      fail_msg = "<error reading fail.out>";
    }
    std::string_view trimmed =
        base::TrimWhitespaceASCII(fail_msg, base::TRIM_ALL);
    ADD_FAILURE() << "rsync interceptor error: " << trimmed;
  }

  // The signal was deferred, so the entire copy should be complete before
  // halting.
  EXPECT_TRUE(base::PathExists(dest_versioned_path("1")));
  EXPECT_TRUE(base::PathExists(dest_contents_path().AppendUTF8("PkgInfo")));
  EXPECT_TRUE(base::PathExists(dest_info_plist_path()));

  // Verify the interceptor recorded that it sent a signal to the right pgroup.
  ASSERT_TRUE(base::PathExists(pgid_path));
  std::string pgid_val;
  ASSERT_TRUE(base::ReadFileToString(pgid_path, &pgid_val));
  EXPECT_EQ(pgid_val, base::StrCat({base::NumberToString(pid), "\n"}));
}

TEST_F(KeystoneInstallTest, VerifyScriptSignalDeferral) {
  base::FilePath pgid_path = temp_.GetPath().AppendUTF8("pgid.out");
  base::FilePath fail_path = temp_.GetPath().AppendUTF8("fail.out");

  // The interceptor uses the PGID as the PID of the install script because
  // ${PPID} is a subshell rather than the intended target.
  RsyncInterceptor interceptor(
      temp_.GetPath(), installer::mac::test::ReplaceAll(
                           R"-(
if [[ "$*" == *"--include"* ]] &&
    [[ "$*" == *"--exclude"* ]] &&
    [[ "$*" == *"/Current"* ]] ; then
  if [[ -f "@PGID_FILE@" ]]; then
    local msg="Unexpected extra matching rsync args: $*"
    echo "${msg}" >> "@FAIL_FILE@"
    echo "${msg}" >& 2
    exit 100
  fi
  PGID=$(ps -o pgid= -p "$$")
  PGID=${PGID//[[:space:]]/}
  echo "${PGID}" >> "@PGID_FILE@"
  kill -s INT -- "${PGID}"
fi)-",
                           {{"@PGID_FILE@", pgid_path.AsUTF8Unsafe()},
                            {"@FAIL_FILE@", fail_path.AsUTF8Unsafe()}}));
  interceptor.SetUp();

  base::ProcessId pid = 0;
  EXPECT_NO_FATAL_FAILURE(RunInstallScript(ProcessTerminatedWithSignal{SIGINT},
                                           interceptor.bin_dir(), &pid));
  if (base::PathExists(fail_path)) {
    std::string fail_msg;
    if (!base::ReadFileToString(fail_path, &fail_msg)) {
      fail_msg = "<error reading fail.out>";
    }
    std::string_view trimmed =
        base::TrimWhitespaceASCII(fail_msg, base::TRIM_ALL);
    ADD_FAILURE() << "rsync interceptor error: " << trimmed;
  }

  // The signal was deferred, so the entire copy should be complete before
  // halting.
  EXPECT_TRUE(base::PathExists(dest_versioned_path("1")));
  EXPECT_TRUE(base::PathExists(dest_contents_path().AppendUTF8("PkgInfo")));
  EXPECT_TRUE(base::PathExists(dest_info_plist_path()));

  // Verify the interceptor recorded that it sent a signal to the right process.
  ASSERT_TRUE(base::PathExists(pgid_path));
  std::string pgid_val;
  ASSERT_TRUE(base::ReadFileToString(pgid_path, &pgid_val));
  EXPECT_EQ(pgid_val, base::StrCat({base::NumberToString(pid), "\n"}));
}

TEST_F(KeystoneInstallTest, VerifyRsyncSignalIgnored) {
  base::FilePath pid_path = temp_.GetPath().AppendUTF8("pid.out");
  base::FilePath fail_path = temp_.GetPath().AppendUTF8("fail.out");
  RsyncInterceptor interceptor(
      temp_.GetPath(), installer::mac::test::ReplaceAll(
                           R"-(
if [[ "$*" == *"--include"* ]] &&
    [[ "$*" == *"--exclude"* ]] &&
    [[ "$*" == *"/Current"* ]] ; then
  if [[ -f "@PID_FILE@" ]]; then
    local msg="Unexpected extra matching rsync args: $*"
    echo "${msg}" >> "@FAIL_FILE@"
    echo "${msg}" >& 2
    exit 100
  fi
  echo "$$" >> "@PID_FILE@"
  kill -s HUP -- "$$"
fi)-",
                           {{"@PID_FILE@", pid_path.AsUTF8Unsafe()},
                            {"@FAIL_FILE@", fail_path.AsUTF8Unsafe()}}));
  interceptor.SetUp();

  base::ProcessId pid = 0;
  EXPECT_NO_FATAL_FAILURE(
      RunInstallScript(ProcessExitedWithValue{0}, interceptor.bin_dir(), &pid));
  if (base::PathExists(fail_path)) {
    std::string fail_msg;
    if (!base::ReadFileToString(fail_path, &fail_msg)) {
      fail_msg = "<error reading fail.out>";
    }
    std::string_view trimmed =
        base::TrimWhitespaceASCII(fail_msg, base::TRIM_ALL);
    ADD_FAILURE() << "rsync interceptor error: " << trimmed;
  }

  // The signal was ignored, so the entire script should have completed.
  EXPECT_TRUE(base::PathExists(dest_versioned_path("1")));
  EXPECT_TRUE(base::PathExists(dest_contents_path().AppendUTF8("PkgInfo")));
  EXPECT_TRUE(base::PathExists(dest_info_plist_path()));

  // Verify the interceptor recorded that it sent a signal to some process other
  // than the install script itself.
  ASSERT_TRUE(base::PathExists(pid_path));
  std::string pid_val;
  ASSERT_TRUE(base::ReadFileToString(pid_path, &pid_val));
  EXPECT_NE(pid_val, base::StrCat({base::NumberToString(pid), "\n"}));
}

TEST_F(KeystoneInstallTest, VerifyInfoPlistCreatedLast) {
  base::FilePath fail_path = temp_.GetPath().AppendUTF8("fail.out");
  base::FilePath plist_rsync_path =
      temp_.GetPath().AppendUTF8("plist_rsync.out");

  RsyncInterceptor interceptor(
      temp_.GetPath(),
      installer::mac::test::ReplaceAll(
          R"-(
if [[ -e "@DEST_INFO_PLIST@" ]]; then
  msg="Top-level Info.plist already exists before rsync call: $*"
  echo "${msg}" >> "@FAIL_FILE@"
  echo "${msg}" >& 2
  exit 100
fi

if [[ -f "@PLIST_RSYNC_FILE@" ]]; then
  msg="Unexpected extra rsync call after Info.plist rsync: $*"
  echo "${msg}" >> "@FAIL_FILE@"
  echo "${msg}" >& 2
  exit 100
fi

non_flags=()
for arg in "$@"; do
  if [[ "${arg}" != -* ]]; then
    non_flags+=("${arg}")
  fi
done

if [[ "${#non_flags[@]}" -eq 2 ]] && \
   [[ "${non_flags[0]}" == *"/Contents/Info.plist" ]] && \
   [[ "${non_flags[1]}" == *"/Contents" ]] && \
   [[ "$*" != *"--exclude"* ]] && \
   [[ "$*" != *"--include"* ]]; then
  echo "$*" >> "@PLIST_RSYNC_FILE@"
fi)-",
          {{"@DEST_INFO_PLIST@", dest_info_plist_path().AsUTF8Unsafe()},
           {"@FAIL_FILE@", fail_path.AsUTF8Unsafe()},
           {"@PLIST_RSYNC_FILE@", plist_rsync_path.AsUTF8Unsafe()}}));
  interceptor.SetUp();

  EXPECT_NO_FATAL_FAILURE(
      RunInstallScript(ProcessExitedWithValue{0}, interceptor.bin_dir()));
  if (base::PathExists(fail_path)) {
    std::string fail_msg;
    if (!base::ReadFileToString(fail_path, &fail_msg)) {
      fail_msg = "<error reading fail.out>";
    }
    std::string_view trimmed =
        base::TrimWhitespaceASCII(fail_msg, base::TRIM_ALL);
    ADD_FAILURE() << "rsync interceptor error: " << trimmed;
  }

  // Verify that an rsync call copying only Info.plist was intercepted.
  ASSERT_TRUE(base::PathExists(plist_rsync_path));
  std::string plist_rsync_args;
  ASSERT_TRUE(base::ReadFileToString(plist_rsync_path, &plist_rsync_args));
  EXPECT_NE(plist_rsync_args.find("Contents/Info.plist"), std::string::npos);

  // Verify all files were copied successfully.
  EXPECT_TRUE(base::PathExists(dest_versioned_path("1")));
  EXPECT_TRUE(base::PathExists(dest_contents_path().AppendUTF8("PkgInfo")));
  EXPECT_TRUE(base::PathExists(dest_info_plist_path()));
}

TEST_F(KeystoneInstallTest, VerifyInfoPlistUpdatedLast) {
  // Set up a preexisting installation at the destination with an older version.
  ASSERT_TRUE(base::CreateDirectory(dest_contents_path()));
  SetPlistItem(dest_contents_path().AppendUTF8("Info"),
               "CFBundleShortVersionString", "1");
  SetPlistItem(dest_contents_path().AppendUTF8("Info"), "KSProductID",
               "com.google.Chrome");
  SetPlistItem(dest_contents_path().AppendUTF8("Info"), "KSVersion", "1");
  SetPlistItem(dest_contents_path().AppendUTF8("Info"), "KSUpdateURL",
               "https://example");
  ASSERT_EQ(ReadPlistItem(dest_contents_path().AppendUTF8("Info"), "KSVersion"),
            "1\n");

  base::FilePath fail_path = temp_.GetPath().AppendUTF8("fail.out");
  base::FilePath plist_rsync_path =
      temp_.GetPath().AppendUTF8("plist_rsync.out");

  RsyncInterceptor interceptor(
      temp_.GetPath(),
      installer::mac::test::ReplaceAll(
          R"-(
if [[ "$(__CFPREFERENCES_AVOID_DAEMON=1 defaults read \
          "@DEST_CONTENTS@/Info" KSVersion 2>/dev/null)" != "1" ]]; then
  msg="Top-level Info.plist already updated before rsync call: $*"
  echo "${msg}" >> "@FAIL_FILE@"
  echo "${msg}" >& 2
  exit 100
fi

if [[ -f "@PLIST_RSYNC_FILE@" ]]; then
  msg="Unexpected extra rsync call after Info.plist rsync: $*"
  echo "${msg}" >> "@FAIL_FILE@"
  echo "${msg}" >& 2
  exit 100
fi

non_flags=()
for arg in "$@"; do
  if [[ "${arg}" != -* ]]; then
    non_flags+=("${arg}")
  fi
done

if [[ "${#non_flags[@]}" -eq 2 ]] && \
   [[ "${non_flags[0]}" == *"/Contents/Info.plist" ]] && \
   [[ "${non_flags[1]}" == *"/Contents" ]] && \
   [[ "$*" != *"--exclude"* ]] && \
   [[ "$*" != *"--include"* ]]; then
  echo "$*" >> "@PLIST_RSYNC_FILE@"
fi)-",
          {{"@DEST_CONTENTS@", dest_contents_path().AsUTF8Unsafe()},
           {"@FAIL_FILE@", fail_path.AsUTF8Unsafe()},
           {"@PLIST_RSYNC_FILE@", plist_rsync_path.AsUTF8Unsafe()}}));
  interceptor.SetUp();

  EXPECT_NO_FATAL_FAILURE(
      RunInstallScript(ProcessExitedWithValue{0}, interceptor.bin_dir()));
  if (base::PathExists(fail_path)) {
    std::string fail_msg;
    if (!base::ReadFileToString(fail_path, &fail_msg)) {
      fail_msg = "<error reading fail.out>";
    }
    std::string_view trimmed =
        base::TrimWhitespaceASCII(fail_msg, base::TRIM_ALL);
    ADD_FAILURE() << "rsync interceptor error: " << trimmed;
  }

  // Verify that an rsync call copying only Info.plist was intercepted.
  ASSERT_TRUE(base::PathExists(plist_rsync_path));
  std::string plist_rsync_args;
  ASSERT_TRUE(base::ReadFileToString(plist_rsync_path, &plist_rsync_args));
  EXPECT_NE(plist_rsync_args.find("Contents/Info.plist"), std::string::npos);

  // Verify all files were copied successfully and Info.plist was updated.
  EXPECT_TRUE(base::PathExists(dest_versioned_path("1")));
  EXPECT_TRUE(base::PathExists(dest_contents_path().AppendUTF8("PkgInfo")));
  EXPECT_TRUE(base::PathExists(dest_info_plist_path()));
  EXPECT_EQ(ReadPlistItem(dest_contents_path().AppendUTF8("Info"), "KSVersion"),
            "2\n");
}

}  // namespace
