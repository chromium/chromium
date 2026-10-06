// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/wof_compression.h"

#include <windows.h>

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/rand_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "chrome/installer/util/work_item.h"
#include "chrome/installer/util/work_item_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace installer {

namespace {

// Large enough that WOF has something to work with. The locale packs this
// targets average a few hundred kilobytes.
constexpr size_t kTestFileSize = 512 * 1024;

// Where Windows records how far it has got through setting up the machine.
constexpr wchar_t kSetupStateKey[] =
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Setup\\State";
constexpr wchar_t kImageStateValue[] = L"ImageState";

constexpr char kDecisionHistogram[] =
    "Setup.Install.WofLocalePakCompressionDecision";
constexpr char kImageStateErrorHistogram[] =
    "Setup.Install.WofLocalePakImageStateError";

// The samples recorded to kDecisionHistogram. Spelled out here rather than
// shared with the code under test, since they are persisted to logs.
enum class Decision {
  kScheduled = 0,
  kWindowsSetupIncomplete = 1,
  kImageStateMissing = 2,
  kImageStateUnreadable = 3,
};

// Writes a file whose contents WOF will find worth compressing.
bool WriteCompressibleFile(const base::FilePath& path) {
  std::string contents;
  contents.reserve(kTestFileSize);
  while (contents.size() < kTestFileSize) {
    contents.append("the quick brown fox jumps over the lazy dog 0123456789\n");
  }
  return base::WriteFile(path, contents);
}

// Writes a file whose contents WOF cannot shrink.
bool WriteIncompressibleFile(const base::FilePath& path) {
  std::vector<uint8_t> contents(kTestFileSize);
  base::RandBytes(contents);
  return base::WriteFile(path, contents);
}

}  // namespace

class WofCompressionTest : public testing::Test {
 protected:
  void SetUp() override {
    // Setup compresses only once Windows has finished setting up the machine,
    // which it records in the registry. Unless a test says otherwise, it has.
    ASSERT_NO_FATAL_FAILURE(
        registry_override_.OverrideRegistry(HKEY_LOCAL_MACHINE));
    ASSERT_NO_FATAL_FAILURE(SetImageState(L"IMAGE_STATE_COMPLETE"));

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    if (!CanCompressInTempDir()) {
      GTEST_SKIP() << "WOF LZX compression is unavailable here.";
    }
    version_dir_ = temp_dir_.GetPath().AppendASCII("1.2.3.4");
    locales_dir_ = version_dir_.Append(FILE_PATH_LITERAL("Locales"));
    ASSERT_TRUE(base::CreateDirectory(locales_dir_));
  }

  // Sets the machine's ImageState, which records how far Windows has got
  // through setting it up.
  static void SetImageState(const wchar_t* image_state) {
    base::win::RegKey key;
    ASSERT_EQ(key.Create(HKEY_LOCAL_MACHINE, kSetupStateKey,
                         KEY_SET_VALUE | KEY_WOW64_64KEY),
              ERROR_SUCCESS);
    ASSERT_EQ(key.WriteValue(kImageStateValue, image_state), ERROR_SUCCESS);
  }

  // Expects that `histograms` saw `decision` made once, and no error reading
  // ImageState.
  static void ExpectDecision(const base::HistogramTester& histograms,
                             Decision decision) {
    histograms.ExpectUniqueSample(kDecisionHistogram, decision, 1);
    histograms.ExpectTotalCount(kImageStateErrorHistogram, 0);
  }

  // Runs the work items that AddWofCompressionWorkItems() produces.
  bool RunWorkItems() {
    std::unique_ptr<WorkItemList> list(WorkItem::CreateWorkItemList());
    AddWofCompressionWorkItems(version_dir_, list.get());
    return list->Do();
  }

  base::FilePath LocalePak(const std::string& name) {
    base::FilePath path = locales_dir_.AppendASCII(name);
    EXPECT_TRUE(WriteCompressibleFile(path));
    return path;
  }

  const base::FilePath& version_dir() const { return version_dir_; }
  const base::FilePath& locales_dir() const { return locales_dir_; }

 private:
  // Returns true if the volume backing the temporary directory supports WOF
  // LZX. Neither a filesystem other than NTFS nor a volume with legacy NTFS
  // compression enabled does.
  bool CanCompressInTempDir() {
    base::ScopedTempDir probe_dir;
    if (!probe_dir.CreateUniqueTempDirUnderPath(temp_dir_.GetPath())) {
      return false;
    }
    base::FilePath version_dir = probe_dir.GetPath().AppendASCII("1.2.3.4");
    base::FilePath probe =
        version_dir.Append(FILE_PATH_LITERAL("Locales")).AppendASCII("t.pak");
    if (!base::CreateDirectory(probe.DirName()) ||
        !WriteCompressibleFile(probe)) {
      return false;
    }
    std::unique_ptr<WorkItemList> list(WorkItem::CreateWorkItemList());
    AddWofCompressionWorkItems(version_dir, list.get());
    return list->Do() && IsFileWofCompressed(probe);
  }

  registry_util::RegistryOverrideManager registry_override_;
  base::ScopedTempDir temp_dir_;
  base::FilePath version_dir_;
  base::FilePath locales_dir_;
};

TEST_F(WofCompressionTest, IsFileWofCompressedIsFalseForPlainFile) {
  base::FilePath pak = LocalePak("en-US.pak");
  EXPECT_FALSE(IsFileWofCompressed(pak));
}

TEST_F(WofCompressionTest, IsFileWofCompressedIsFalseForMissingFile) {
  EXPECT_FALSE(IsFileWofCompressed(locales_dir().AppendASCII("absent.pak")));
}

TEST_F(WofCompressionTest, IsFileWofCompressedIsFalseForDirectory) {
  EXPECT_FALSE(IsFileWofCompressed(locales_dir()));
}

TEST_F(WofCompressionTest, CompressesLocalePaks) {
  base::FilePath en = LocalePak("en-US.pak");
  base::FilePath fr = LocalePak("fr.pak");

  EXPECT_TRUE(RunWorkItems());

  EXPECT_TRUE(IsFileWofCompressed(en));
  EXPECT_TRUE(IsFileWofCompressed(fr));
}

TEST_F(WofCompressionTest, PreservesContentsAndLogicalSize) {
  base::FilePath pak = LocalePak("en-US.pak");
  std::string before;
  ASSERT_TRUE(base::ReadFileToString(pak, &before));

  ASSERT_TRUE(RunWorkItems());
  ASSERT_TRUE(IsFileWofCompressed(pak));

  std::string after;
  ASSERT_TRUE(base::ReadFileToString(pak, &after));
  EXPECT_EQ(before, after);
  EXPECT_EQ(base::GetFileSize(pak), before.size());
}

TEST_F(WofCompressionTest, LeavesNonPakFilesAlone) {
  base::FilePath other = locales_dir().AppendASCII("en-US.pak.info");
  ASSERT_TRUE(WriteCompressibleFile(other));

  EXPECT_TRUE(RunWorkItems());

  EXPECT_FALSE(IsFileWofCompressed(other));
}

TEST_F(WofCompressionTest, LeavesFilesOutsideLocalesAlone) {
  base::FilePath pak = version_dir().AppendASCII("resources.pak");
  ASSERT_TRUE(WriteCompressibleFile(pak));

  EXPECT_TRUE(RunWorkItems());

  EXPECT_FALSE(IsFileWofCompressed(pak));
}

TEST_F(WofCompressionTest, DoesNotRecurseIntoSubdirectories) {
  base::FilePath nested = locales_dir().AppendASCII("nested");
  ASSERT_TRUE(base::CreateDirectory(nested));
  base::FilePath pak = nested.AppendASCII("en-US.pak");
  ASSERT_TRUE(WriteCompressibleFile(pak));

  EXPECT_TRUE(RunWorkItems());

  EXPECT_FALSE(IsFileWofCompressed(pak));
}

TEST_F(WofCompressionTest, SucceedsWithNoLocalesDirectory) {
  ASSERT_TRUE(base::DeletePathRecursively(locales_dir()));

  EXPECT_TRUE(RunWorkItems());
}

TEST_F(WofCompressionTest, SucceedsWithEmptyLocalesDirectory) {
  EXPECT_TRUE(RunWorkItems());
}

TEST_F(WofCompressionTest, SucceedsOnAlreadyCompressedPaks) {
  base::FilePath pak = LocalePak("en-US.pak");
  ASSERT_TRUE(RunWorkItems());
  ASSERT_TRUE(IsFileWofCompressed(pak));

  EXPECT_TRUE(RunWorkItems());

  EXPECT_TRUE(IsFileWofCompressed(pak));
}

TEST_F(WofCompressionTest, SucceedsOnIncompressiblePaks) {
  base::FilePath pak = locales_dir().AppendASCII("en-US.pak");
  ASSERT_TRUE(WriteIncompressibleFile(pak));

  EXPECT_TRUE(RunWorkItems());

  // WOF declines to compress data it cannot shrink. That is not a failure.
  EXPECT_FALSE(IsFileWofCompressed(pak));
}

TEST_F(WofCompressionTest, CompressesPaksHeldOpenByAnotherHandle) {
  base::FilePath pak = LocalePak("en-US.pak");
  // Something else on the machine, an antivirus scanner for instance, can be
  // holding a pack open when the installer gets to it. Compression asks for no
  // access rights, so it is not blocked by that.
  base::File hold(pak, base::File::FLAG_OPEN | base::File::FLAG_READ |
                           base::File::FLAG_WIN_EXCLUSIVE_READ |
                           base::File::FLAG_WIN_EXCLUSIVE_WRITE);
  ASSERT_TRUE(hold.IsValid());

  EXPECT_TRUE(RunWorkItems());
  hold.Close();

  EXPECT_TRUE(IsFileWofCompressed(pak));
}

TEST_F(WofCompressionTest, WorkItemIsBestEffortAndNotRolledBack) {
  base::FilePath pak = LocalePak("en-US.pak");

  std::unique_ptr<WorkItemList> list(WorkItem::CreateWorkItemList());
  AddWofCompressionWorkItems(version_dir(), list.get());
  ASSERT_TRUE(list->Do());
  ASSERT_TRUE(IsFileWofCompressed(pak));

  // Rolling the list back must leave the packs compressed: there is nothing to
  // undo, and expanding them again would only cost disk space.
  list->Rollback();

  EXPECT_TRUE(IsFileWofCompressed(pak));
}

TEST_F(WofCompressionTest, RecordsThatCompressionWasScheduled) {
  base::FilePath pak = LocalePak("en-US.pak");
  base::HistogramTester histograms;

  EXPECT_TRUE(RunWorkItems());

  EXPECT_TRUE(IsFileWofCompressed(pak));
  ExpectDecision(histograms, Decision::kScheduled);
}

TEST_F(WofCompressionTest, ImageStateIsCaseInsensitive) {
  base::FilePath pak = LocalePak("en-US.pak");
  ASSERT_NO_FATAL_FAILURE(SetImageState(L"Image_State_Complete"));

  EXPECT_TRUE(RunWorkItems());

  EXPECT_TRUE(IsFileWofCompressed(pak));
}

TEST_F(WofCompressionTest, LeavesPaksAloneWithoutSetupStateKey) {
  base::FilePath pak = LocalePak("en-US.pak");
  ASSERT_EQ(
      base::win::RegKey(HKEY_LOCAL_MACHINE, L"", KEY_READ | KEY_WOW64_64KEY)
          .DeleteKey(kSetupStateKey),
      ERROR_SUCCESS);
  base::HistogramTester histograms;

  EXPECT_TRUE(RunWorkItems());

  EXPECT_FALSE(IsFileWofCompressed(pak));
  // Without the key there is nothing to say that Windows setup has finished,
  // which is not an error. It is reported apart from a machine that says setup
  // is still running, so that a configuration that never records ImageState
  // can be told from one genuinely in audit mode or OOBE.
  ExpectDecision(histograms, Decision::kImageStateMissing);
}

TEST_F(WofCompressionTest, LeavesPaksAloneWithoutImageState) {
  base::FilePath pak = LocalePak("en-US.pak");
  ASSERT_EQ(base::win::RegKey(HKEY_LOCAL_MACHINE, kSetupStateKey,
                              KEY_SET_VALUE | KEY_WOW64_64KEY)
                .DeleteValue(kImageStateValue),
            ERROR_SUCCESS);
  base::HistogramTester histograms;

  EXPECT_TRUE(RunWorkItems());

  EXPECT_FALSE(IsFileWofCompressed(pak));
  ExpectDecision(histograms, Decision::kImageStateMissing);
}

TEST_F(WofCompressionTest, LeavesPaksAloneWhenImageStateIsUnreadable) {
  base::FilePath pak = LocalePak("en-US.pak");
  // A value of the wrong type stands in for anything other than a missing key
  // or value that stops setup reading ImageState.
  ASSERT_EQ(base::win::RegKey(HKEY_LOCAL_MACHINE, kSetupStateKey,
                              KEY_SET_VALUE | KEY_WOW64_64KEY)
                .WriteValue(kImageStateValue, DWORD{1}),
            ERROR_SUCCESS);
  base::HistogramTester histograms;

  EXPECT_TRUE(RunWorkItems());

  EXPECT_FALSE(IsFileWofCompressed(pak));
  // No machine should hit this, so it is reported apart from an image that is
  // still being built, along with the error that caused it.
  histograms.ExpectUniqueSample(kDecisionHistogram,
                                Decision::kImageStateUnreadable, 1);
  histograms.ExpectUniqueSample(kImageStateErrorHistogram, ERROR_CANTREAD, 1);
}

TEST_F(WofCompressionTest, CompressesOnceWindowsSetupFinishes) {
  base::FilePath pak = LocalePak("en-US.pak");
  ASSERT_NO_FATAL_FAILURE(
      SetImageState(L"IMAGE_STATE_SPECIALIZE_RESEAL_TO_AUDIT"));
  ASSERT_TRUE(RunWorkItems());
  ASSERT_FALSE(IsFileWofCompressed(pak));

  ASSERT_NO_FATAL_FAILURE(SetImageState(L"IMAGE_STATE_COMPLETE"));

  EXPECT_TRUE(RunWorkItems());

  EXPECT_TRUE(IsFileWofCompressed(pak));
}

// Every ImageState that Windows reports before its setup, OOBE included, has
// finished.
class WofCompressionImageStateTest
    : public WofCompressionTest,
      public testing::WithParamInterface<const wchar_t*> {};

TEST_P(WofCompressionImageStateTest, LeavesPaksAlone) {
  base::FilePath pak = LocalePak("en-US.pak");
  ASSERT_NO_FATAL_FAILURE(SetImageState(GetParam()));
  base::HistogramTester histograms;

  EXPECT_TRUE(RunWorkItems());

  EXPECT_FALSE(IsFileWofCompressed(pak));
  ExpectDecision(histograms, Decision::kWindowsSetupIncomplete);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WofCompressionImageStateTest,
    testing::Values(L"IMAGE_STATE_UNDEPLOYABLE",
                    L"IMAGE_STATE_GENERALIZE_RESEAL_TO_OOBE",
                    L"IMAGE_STATE_GENERALIZE_RESEAL_TO_AUDIT",
                    L"IMAGE_STATE_SPECIALIZE_RESEAL_TO_OOBE",
                    L"IMAGE_STATE_SPECIALIZE_RESEAL_TO_AUDIT",
                    L""));

}  // namespace installer
