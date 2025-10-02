// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <poll.h>

#include <cstdlib>
#include <string>

#include "base/apple/foundation_util.h"
#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class KeystoneInstallTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_.CreateUniqueTempDir());

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

    // Create the destination directory.
    app_install_dir_ = temp_.GetPath().AppendUTF8("dest");
    base::CreateDirectory(app_install_dir_);

    // Create a fake ksadmin.
    std::string ksadmin =
        R"(
        #!/bin/bash
        if [ "${1}" = "--ksadmin-version" ] ; then
          # version check
          echo "137.0.0.0"
          exit 0
        fi
        if [ "${1}" = "-pP" ] ; then
          # finding app to update
          echo " xc=<KSPathExistenceChecker:0x45 path=__DEST__>"
          exit 0
        fi
        # otherwise, just save args
        while (( "$#" )) ; do
          echo -n "${1} " >> "__OUTFILE__"
          shift
        done
        echo "\n" >> "__OUTFILE__"
        exit 0
        )";
    base::ReplaceSubstringsAfterOffset(&ksadmin, 0, "__DEST__",
                                       app_install_dir_.AsUTF8Unsafe());
    base::ReplaceSubstringsAfterOffset(
        &ksadmin, 0, "__OUTFILE__",
        temp_.GetPath().AppendUTF8("ksadmin_out").AsUTF8Unsafe());
    base::CreateDirectory(temp_.GetPath().AppendUTF8("ksadmin"));
    ASSERT_TRUE(base::WriteFile(
        temp_.GetPath().AppendUTF8("ksadmin").AppendUTF8("ksadmin"), ksadmin));
    ASSERT_TRUE(base::SetPosixFilePermissions(
        temp_.GetPath().AppendUTF8("ksadmin").AppendUTF8("ksadmin"),
        base::FILE_PERMISSION_READ_BY_USER |
            base::FILE_PERMISSION_WRITE_BY_USER |
            base::FILE_PERMISSION_EXECUTE_BY_USER));
  }

  void TearDown() override {
    base::DeleteFile(base::FilePath(getenv("HOME"))
                         .AppendUTF8("Library")
                         .AppendUTF8("Google")
                         .AppendUTF8("Google Chrome Brand"));
  }

  // There is no simple API in base/launch.h for providing environment
  // variables and capturing all of stdout, stderr, and the exit code.
  void RunExecutable(const base::CommandLine& cmd,
                     const base::EnvironmentMap& env,
                     std::string* output,
                     int* exit_code) {
    base::ScopedFD read_fd, write_fd;
    {
      int pipefds[2] = {};
      ASSERT_EQ(pipe(pipefds), 0);
      read_fd.reset(pipefds[0]);
      write_fd.reset(pipefds[1]);
    }

    base::LaunchOptions options;
    options.fds_to_remap.emplace_back(write_fd.get(), STDOUT_FILENO);
    options.fds_to_remap.emplace_back(write_fd.get(), STDERR_FILENO);
    options.current_directory = mount_dir_;
    options.clear_environment = true;
    options.environment = env;
    const base::Process proc = base::LaunchProcess(cmd, options);
    ASSERT_TRUE(proc.IsValid());
    write_fd.reset();

    base::Time deadline = base::Time::Now() + base::Seconds(60);

    static constexpr size_t kBufferSize = 1024;
    base::CheckedNumeric<size_t> total_bytes_read = 0;
    ssize_t read_this_pass = 0;
    do {
      struct pollfd fds[1] = {{.fd = read_fd.get(), .events = POLLIN}};
      int timeout_remaining_ms =
          static_cast<int>((deadline - base::Time::Now()).InMilliseconds());
      if (timeout_remaining_ms < 0 || poll(fds, 1, timeout_remaining_ms) != 1) {
        break;
      }
      base::CheckedNumeric<size_t> new_size =
          base::CheckedNumeric<size_t>(output->size()) +
          base::CheckedNumeric<size_t>(kBufferSize);
      if (!new_size.IsValid() || !total_bytes_read.IsValid()) {
        // Ignore the rest of the output.
        break;
      }
      output->resize(new_size.ValueOrDie());
      read_this_pass = HANDLE_EINTR(
          read(read_fd.get(), &(*output)[total_bytes_read.ValueOrDie()],
               kBufferSize));
      if (read_this_pass >= 0) {
        total_bytes_read += base::CheckedNumeric<size_t>(read_this_pass);
        if (!total_bytes_read.IsValid()) {
          // Ignore the rest of the output.
          break;
        }
        output->resize(total_bytes_read.ValueOrDie());
      }
    } while (read_this_pass > 0);

    ASSERT_TRUE(proc.WaitForExitWithTimeout(
        std::max(deadline - base::Time::Now(), base::TimeDelta()), exit_code));
  }

  void RunInstallScript(int exit_code) {
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

    int actual_exit_code = 0;
    std::string output;
    RunExecutable(
        cmd,
        {
            {"KS_TICKET_AP", "ap"},
            {"KS_TICKET_SERVER_URL", "https://exampleserverurl"},
            {"KS_TICKET_XC_PATH", app_install_dir_.AsUTF8Unsafe()},
            {"PATH",
             base::StrCat(
                 {"/bin:/usr/bin:",
                  temp_.GetPath().AppendUTF8("ksadmin").AsUTF8Unsafe()})},
            {"PREVIOUS_VERSION", "1.0.0.0"},
            {"SERVER_ARGS", ""},
            {"UPDATE_IS_MACHINE", "0"},
            {"UNPACK_DIR", mount_dir_.AsUTF8Unsafe()},
            {"GOOGLE_USAGE_STATS_ENABLED", "0"},
            {"GOOGLE_CHROME_UPDATER_DEBUG", "1"},
            {"GOOGLE_CHROME_UPDATER_TEST_ENROLLMENT_PATH",
             temp_.GetPath().AppendUTF8("Enrollment").AsUTF8Unsafe()},
        },
        &output, &actual_exit_code);
    ASSERT_EQ(actual_exit_code, exit_code) << output;
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
    SetPlistItem(base::FilePath(getenv("HOME"))
                     .AppendUTF8("Library")
                     .AppendUTF8("Google")
                     .AppendUTF8("Google Chrome Brand"),
                 "KSBrandID", brand);
  }

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

  std::string ReadLibraryBrand() {
    base::CommandLine cmd(base::FilePath("defaults"));
    cmd.AppendArg("read");
    cmd.AppendArgPath(base::FilePath(getenv("HOME"))
                          .AppendUTF8("Library")
                          .AppendUTF8("Google")
                          .AppendUTF8("Google Chrome Brand"));
    cmd.AppendArg("KSBrandID");
    std::string output;
    int exit_code = 0;
    RunExecutable(cmd, {{"__CFPREFERENCES_AVOID_DAEMON", "1"}}, &output,
                  &exit_code);
    EXPECT_EQ(exit_code, 0) << output;
    return output;
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
  base::FilePath mount_dir_;
  base::FilePath app_install_dir_;
};

TEST_F(KeystoneInstallTest, RunScript) {
  ASSERT_NO_FATAL_FAILURE(RunInstallScript(0));
}

TEST_F(KeystoneInstallTest, CBCMBrandSubstitution) {
  SetEnrolled(true);

  ASSERT_NO_FATAL_FAILURE(SetLibraryBrand("GCEA"));
  ASSERT_NO_FATAL_FAILURE(RunInstallScript(0));
  ASSERT_EQ(ReadLibraryBrand(), "GCCA\n");

  ASSERT_NO_FATAL_FAILURE(SetLibraryBrand("FPAZ"));
  ASSERT_NO_FATAL_FAILURE(RunInstallScript(0));
  ASSERT_EQ(ReadLibraryBrand(), "FPJZ\n");
}

TEST_F(KeystoneInstallTest, CBCMReverseBrandSubstitution) {
  SetEnrolled(false);

  ASSERT_NO_FATAL_FAILURE(SetLibraryBrand("GCCA"));
  ASSERT_NO_FATAL_FAILURE(RunInstallScript(0));
  ASSERT_EQ(ReadLibraryBrand(), "GCEA\n");

  ASSERT_NO_FATAL_FAILURE(SetLibraryBrand("FPJZ"));
  ASSERT_NO_FATAL_FAILURE(RunInstallScript(0));
  ASSERT_EQ(ReadLibraryBrand(), "FPAZ\n");
}

TEST_F(KeystoneInstallTest, CBCMBrandSubstitutionNoOpUnenrolled) {
  SetEnrolled(false);

  ASSERT_NO_FATAL_FAILURE(SetLibraryBrand("GCEA"));
  ASSERT_NO_FATAL_FAILURE(RunInstallScript(0));
  ASSERT_EQ(ReadLibraryBrand(), "GCEA\n");

  ASSERT_NO_FATAL_FAILURE(SetLibraryBrand("FPAZ"));
  ASSERT_NO_FATAL_FAILURE(RunInstallScript(0));
  ASSERT_EQ(ReadLibraryBrand(), "FPAZ\n");
}

TEST_F(KeystoneInstallTest, CBCMBrandSubstitutionNoOpEnrolled) {
  SetEnrolled(true);

  ASSERT_NO_FATAL_FAILURE(SetLibraryBrand("GCCA"));
  ASSERT_NO_FATAL_FAILURE(RunInstallScript(0));
  ASSERT_EQ(ReadLibraryBrand(), "GCCA\n");

  ASSERT_NO_FATAL_FAILURE(SetLibraryBrand("FPJZ"));
  ASSERT_NO_FATAL_FAILURE(RunInstallScript(0));
  ASSERT_EQ(ReadLibraryBrand(), "FPJZ\n");

  ASSERT_NO_FATAL_FAILURE(SetLibraryBrand("GGLS"));
  ASSERT_NO_FATAL_FAILURE(RunInstallScript(0));
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

}  // namespace
