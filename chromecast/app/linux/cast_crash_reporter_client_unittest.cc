// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fstream>
#include <vector>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/test/scoped_path_override.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chromecast/app/linux/cast_crash_reporter_client.h"
#include "chromecast/base/scoped_temp_file.h"
#include "chromecast/crash/app_state_tracker.h"
#include "chromecast/crash/linux/crash_testing_utils.h"
#include "chromecast/crash/linux/crash_util.h"
#include "chromecast/crash/linux/dump_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace {

const char kFakeDumpstateContents[] = "Dumpstate Contents\nDumpdumpdumpdump\n";
const char kFakeMinidumpContents[] = "Minidump Contents\nLine1\nLine2\n";

int WriteFakeDumpStateFile(const std::string& path) {
  // Append the correct extension and write the data to file.
  base::File dumpstate(base::FilePath(path).AddExtension(".txt.gz"),
                       base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  dumpstate.Write(
      0, kFakeDumpstateContents, sizeof(kFakeDumpstateContents) - 1);
  return 0;
}

}  // namespace

class CastCrashReporterClientTest : public testing::Test {
 protected:
  CastCrashReporterClientTest() {}
  ~CastCrashReporterClientTest() override {}

  static void SetUpTestCase() {
    // Set a callback to be used in place of the |dumpstate| executable.
    CrashUtil::SetDumpStateCbForTest(base::Bind(&WriteFakeDumpStateFile));
  }

  // testing::Test implementation:
  void SetUp() override {
    // Override the $HOME path.
    ASSERT_TRUE(fake_home_dir_.CreateUniqueTempDir());
    home_override_.reset(
        new base::ScopedPathOverride(base::DIR_HOME, home_path()));

    // "Launch" YouTube.
    AppStateTracker::SetLastLaunchedApp("youtube");
    AppStateTracker::SetCurrentApp("youtube");

    // "Launch" and switch to Pandora.
    AppStateTracker::SetLastLaunchedApp("pandora");
    AppStateTracker::SetCurrentApp("pandora");

    // "Launch" Netflix.
    AppStateTracker::SetLastLaunchedApp("netflix");
    // Netflix crashed.

    // A minidump file is written.
    minidump_.Write(kFakeMinidumpContents);
  }

  void TearDown() override {
    // Remove IO restrictions in order to examine the state of the filesystem.
    base::ThreadRestrictions::SetIOAllowed(true);

    // Assert that the original file has been moved.
    ASSERT_FALSE(base::PathExists(minidump_path()));

    // Assert that the file has been moved to "minidumps", with the expected
    // contents.
    std::string contents;
    base::FilePath new_minidump =
        home_path().Append("minidumps").Append(minidump_path().BaseName());
    ASSERT_TRUE(base::PathExists(new_minidump));
    ASSERT_TRUE(base::ReadFileToString(new_minidump, &contents));
    ASSERT_EQ(kFakeMinidumpContents, contents);

    // Assert that the dumpstate file has been written with the expected
    // contents.
    base::FilePath dumpstate = new_minidump.AddExtension(".txt.gz");
    ASSERT_TRUE(base::PathExists(dumpstate));
    ASSERT_TRUE(base::ReadFileToString(dumpstate, &contents));
    ASSERT_EQ(kFakeDumpstateContents, contents);

    // Assert that the lockfile has logged the correct information.
    base::FilePath lockfile =
        home_path().Append("minidumps").Append("lockfile");
    ASSERT_TRUE(base::PathExists(lockfile));
    std::vector<std::unique_ptr<DumpInfo>> dumps;
    ASSERT_TRUE(FetchDumps(lockfile.value(), &dumps));
    ASSERT_EQ(1u, dumps.size());

    const DumpInfo& dump_info = *(dumps[0]);
    ASSERT_TRUE(dump_info.valid());
    EXPECT_EQ(new_minidump.value(), dump_info.crashed_process_dump());
    EXPECT_EQ(dumpstate.value(), dump_info.logfile());
    EXPECT_EQ("youtube", dump_info.params().previous_app_name);
    EXPECT_EQ("pandora", dump_info.params().current_app_name);
    EXPECT_EQ("netflix", dump_info.params().last_app_name);
  }

  base::FilePath minidump_path() { return minidump_.path(); }
  base::FilePath home_path() { return fake_home_dir_.GetPath(); }

 private:
  base::ScopedTempDir fake_home_dir_;
  ScopedTempFile minidump_;
  std::unique_ptr<base::ScopedPathOverride> home_override_;

  DISALLOW_COPY_AND_ASSIGN(CastCrashReporterClientTest);
};

#if DCHECK_IS_ON()
// This test shall only be run when thread restricitons are enabled. Otherwise,
// the thread will not actually be IO-restricted, and the final ASSERT will
// fail.
TEST_F(CastCrashReporterClientTest, EndToEndTestOnIORestrictedThread) {
  // Handle a "crash" on an IO restricted thread.
  base::ThreadRestrictions::SetIOAllowed(false);
  CastCrashReporterClient client;
  ASSERT_TRUE(client.HandleCrashDump(minidump_path().value().c_str(), 0));

  // Assert that the thread is IO restricted when the function exits.
  // Note that SetIOAllowed returns the previous value.
  ASSERT_FALSE(base::ThreadRestrictions::SetIOAllowed(true));
}
#endif  // DCHECK_IS_ON()

TEST_F(CastCrashReporterClientTest, EndToEndTestOnNonIORestrictedThread) {
  // Handle a crash on a non-IO restricted thread.
  base::ThreadRestrictions::SetIOAllowed(true);
  CastCrashReporterClient client;
  ASSERT_TRUE(client.HandleCrashDump(minidump_path().value().c_str(), 0));

  // Assert that the thread is not IO restricted when the function exits.
  // Note that SetIOAllowed returns the previous value.
  ASSERT_TRUE(base::ThreadRestrictions::SetIOAllowed(true));
}

}  // namespace chromecast
