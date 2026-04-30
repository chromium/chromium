// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sqlite_vfs/sqlite_database_vfs_file_set.h"

#include <stdlib.h>

#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/test/gmock_expected_support.h"
#include "build/blink_buildflags.h"
#include "build/build_config.h"
#include "components/sqlite_vfs/client.h"
#include "components/sqlite_vfs/constants.h"
#include "components/sqlite_vfs/file_type.h"
#include "components/sqlite_vfs/sandboxed_file.h"
#include "components/sqlite_vfs/vfs_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(USE_BLINK) && !BUILDFLAG(IS_ANDROID)
#include "base/check.h"
#include "base/command_line.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "testing/multiprocess_func_list.h"
#endif

namespace sqlite_vfs {

namespace {

// Test is parameterized on the values for `single_connection` and
// `journal_mode_wal`.
class SqliteVfsFileSetTest
    : public testing::TestWithParam<std::tuple<bool, bool>> {
 protected:
  static constexpr base::FilePath::StringViewType kBaseName =
      FILE_PATH_LITERAL("TEST");

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  const base::FilePath& GetTempDir() const { return temp_dir_.GetPath(); }

  std::vector<base::FilePath> GetFilePaths() {
    std::vector<base::FilePath> paths;
    paths.push_back(
        temp_dir_.GetPath().Append(kBaseName).AddExtension(kDbFileExtension));
    paths.push_back(temp_dir_.GetPath().Append(kBaseName).AddExtension(
        kJournalFileExtension));
    if (journal_mode_wal()) {
      paths.push_back(temp_dir_.GetPath().Append(kBaseName).AddExtension(
          kWalJournalFileExtension));
    }
    return paths;
  }

  std::optional<SqliteVfsFileSet> CreateFilesAndBuildVfsFileSet() {
    std::optional<SqliteVfsFileSet> file_set;
    if (auto pending_file_set = MakePendingFileSet(
            Client::kTest, temp_dir_.GetPath(), base::FilePath(kBaseName),
            is_single_connection(), journal_mode_wal());
        !pending_file_set.has_value()) {
      ADD_FAILURE() << "Failed creating pending file_set";
    } else {
      file_set =
          SqliteVfsFileSet::Bind(Client::kTest, *std::move(pending_file_set));
      EXPECT_NE(file_set, std::nullopt) << "Failed creating pending file_set";
    }
    return file_set;
  }

  const base::FilePath& file_set_directory() const {
    return temp_dir_.GetPath();
  }

  static bool is_single_connection() { return std::get<0>(GetParam()); }
  static bool journal_mode_wal() { return std::get<1>(GetParam()); }

 private:
  base::ScopedTempDir temp_dir_;
};

// Tests that creating and destroying a file set doesn't delete the files.
TEST_P(SqliteVfsFileSetTest, FilesAreNotDeleted) {
  auto paths = GetFilePaths();

  {
    ASSERT_OK_AND_ASSIGN(auto file_set, CreateFilesAndBuildVfsFileSet());

    ASSERT_THAT(paths,
                testing::Each(testing::ResultOf(base::PathExists, true)));
  }

  ASSERT_THAT(paths, testing::Each(testing::ResultOf(base::PathExists, true)));
}

// Tests that a file set's files can be deleted while it's in use and are
// absent upon destruction.
TEST_P(SqliteVfsFileSetTest, FilesCanBeDeleted) {
  auto paths = GetFilePaths();

  {
    ASSERT_OK_AND_ASSIGN(auto file_set, CreateFilesAndBuildVfsFileSet());

    ASSERT_THAT(paths,
                testing::Each(testing::ResultOf(base::PathExists, true)));

    ASSERT_THAT(paths,
                testing::Each(testing::ResultOf(base::DeleteFile, true)));
  }

  ASSERT_THAT(paths, testing::Each(testing::ResultOf(base::PathExists, false)));

  // No other files should have been left behind.
  ASSERT_PRED1(base::IsDirectoryEmpty, GetTempDir());
}

// Multiprocess tests are not supported on non-blink platforms (i.e., iOS), and
// they don't work from this test harness on Android.
#if BUILDFLAG(USE_BLINK) && !BUILDFLAG(IS_ANDROID)

static constexpr std::string_view kDirectorySwitch = "directory";
static constexpr std::string_view kBaseNameSwitch = "base-name";
static constexpr std::string_view kSingleConnection = "single-connection";
static constexpr std::string_view kJournalModeWal = "journal-mode-wal";

// The main function for a child process that returns EXIT_SUCCESS if a named
// file set can be opened in a given directory.
MULTIPROCESS_TEST_MAIN(CanOpenConnectionInChild) {
  base::CommandLine& cmd_line = *base::CommandLine::ForCurrentProcess();
  base::FilePath directory = cmd_line.GetSwitchValuePath(kDirectorySwitch);
  base::FilePath base_name = cmd_line.GetSwitchValuePath(kBaseNameSwitch);
  bool single_connection = cmd_line.HasSwitch(kSingleConnection);
  bool journal_mode_wal = cmd_line.HasSwitch(kJournalModeWal);

  auto pending_file_set = MakePendingFileSet(
      Client::kTest, directory, base_name, single_connection, journal_mode_wal);

#if BUILDFLAG(IS_WIN)
  // On Windows, the files cannot even be opened a second time if the parent
  // used single_connection=true.
  if (!pending_file_set.has_value()) {
    return EXIT_FAILURE;
  }
#else
  // Other platforms don't have such protections -- single_connection is
  // checked below when the main database file is opened.
  CHECK(pending_file_set.has_value());
#endif

  auto file_set =
      SqliteVfsFileSet::Bind(Client::kTest, *std::move(pending_file_set));
  CHECK(file_set.has_value());

  SandboxedFile* db_file = file_set->GetSandboxedDbFile();
  if (auto file = db_file->TakeUnderlyingFile(FileType::kMainDb);
      file.IsValid()) {
    // Take care to complete the SandboxedVfs open protocol and close the file
    // if it was opened.
    db_file->OnFileOpened(std::move(file));
    db_file->Close();
    return EXIT_SUCCESS;  // The file was opened.
  }

  return EXIT_FAILURE;  // Could not open the file.
}

// Tests that an open database can/can't be accessed for other connections based
// on the `single_connection` parameter.
TEST_P(SqliteVfsFileSetTest, MultipleConnections) {
  ASSERT_OK_AND_ASSIGN(auto file_set, CreateFilesAndBuildVfsFileSet());

  // Open the file, thereby locking it for exclusive access if it was created
  // for only a single connection.
  SandboxedFile* db_file = file_set.GetSandboxedDbFile();
  db_file->OnFileOpened(db_file->TakeUnderlyingFile(FileType::kMainDb));

  // Attempt to open the file in another process.
  base::CommandLine child_command_line =
      base::GetMultiProcessTestChildBaseCommandLine();
  child_command_line.AppendSwitchPath(kDirectorySwitch, file_set_directory());
  child_command_line.AppendSwitchPath(kBaseNameSwitch,
                                      base::FilePath(kBaseName));
  if (is_single_connection()) {
    child_command_line.AppendSwitch(kSingleConnection);
  }
  if (journal_mode_wal()) {
    child_command_line.AppendSwitch(kJournalModeWal);
  }
  base::LaunchOptions launch_options;
#if BUILDFLAG(IS_WIN)
  launch_options.start_hidden = true;
  launch_options.feedback_cursor_off = true;
#endif
  base::Process process = base::SpawnMultiProcessTestChild(
      "CanOpenConnectionInChild", child_command_line, launch_options);

  int exit_code = -1;
  ASSERT_TRUE(base::WaitForMultiprocessTestChildExit(
      process, TestTimeouts::action_timeout(), &exit_code));
  if (is_single_connection()) {
    ASSERT_EQ(exit_code, EXIT_FAILURE);
  } else {
    ASSERT_EQ(exit_code, EXIT_SUCCESS);
  }

  db_file->Close();
}

#endif  // BUILDFLAG(USE_BLINK) && !BUILDFLAG(IS_ANDROID)

TEST_P(SqliteVfsFileSetTest, AbandonAndReopen) {
  if (is_single_connection()) {
    GTEST_SKIP()
        << "Abandon and Reopen is only supported for multiple connections.";
  }
  ASSERT_OK_AND_ASSIGN(auto file_set, CreateFilesAndBuildVfsFileSet());

  // Share with renderer (read-only).
  std::optional<PendingFileSet> shared_file_set;
  if (!is_single_connection()) {
    shared_file_set =
        ShareConnection(file_set_directory(), base::FilePath(kBaseName),
                        file_set, /*read_write=*/false);
    ASSERT_TRUE(shared_file_set.has_value());
    EXPECT_TRUE(shared_file_set->db_file.IsValid());
    EXPECT_TRUE(shared_file_set->journal_file.IsValid());
    if (journal_mode_wal()) {
      EXPECT_TRUE(shared_file_set->wal_file.IsValid());
      EXPECT_TRUE(shared_file_set->wal_index_file.IsValid());
    }
  }

  // Abandon file set.
  LockState state = LockState::kNotHeld;
  if (!is_single_connection()) {
    state = file_set.Abandon();
    EXPECT_EQ(state, LockState::kNotHeld);
  }

  // Re-open file set (via MakePendingFileSet).
  ASSERT_OK_AND_ASSIGN(
      auto pending_file_set2,
      MakePendingFileSet(Client::kTest, file_set_directory(),
                         base::FilePath(kBaseName), is_single_connection(),
                         journal_mode_wal()));

  // Verify new handles are valid.
  EXPECT_TRUE(pending_file_set2.db_file.IsValid());
  EXPECT_TRUE(pending_file_set2.journal_file.IsValid());
  if (journal_mode_wal() && !is_single_connection()) {
    EXPECT_TRUE(pending_file_set2.wal_file.IsValid());
    EXPECT_TRUE(pending_file_set2.wal_index_file.IsValid());
  }
}

TEST_P(SqliteVfsFileSetTest, ShareReadOnlyTwice) {
  if (is_single_connection()) {
    GTEST_SKIP()
        << "Sharing connections is only supported for multiple connections.";
  }
  ASSERT_OK_AND_ASSIGN(auto file_set, CreateFilesAndBuildVfsFileSet());

  // Share for read-only access.
  ASSERT_OK_AND_ASSIGN(
      auto shared_pending_file_set,
      ShareConnection(file_set_directory(), base::FilePath(kBaseName), file_set,
                      /*read_write=*/false));
  EXPECT_TRUE(shared_pending_file_set.db_file.IsValid());
  EXPECT_TRUE(shared_pending_file_set.journal_file.IsValid());
  if (journal_mode_wal()) {
    EXPECT_TRUE(shared_pending_file_set.wal_file.IsValid());
    EXPECT_TRUE(shared_pending_file_set.wal_index_file.IsValid());
  }

  // Bind the new file set.
  ASSERT_OK_AND_ASSIGN(auto shared_file_set,
                       SqliteVfsFileSet::Bind(
                           Client::kTest, std::move(shared_pending_file_set)));
  EXPECT_TRUE(shared_file_set.read_only());

  // Share the new file set for read-only access.
  ASSERT_OK_AND_ASSIGN(
      auto shared_pending_file_set2,
      ShareConnection(file_set_directory(), base::FilePath(kBaseName),
                      shared_file_set, /*read_write=*/false));
  EXPECT_TRUE(shared_pending_file_set2.db_file.IsValid());
  EXPECT_TRUE(shared_pending_file_set2.journal_file.IsValid());
  if (journal_mode_wal()) {
    EXPECT_TRUE(shared_pending_file_set2.wal_file.IsValid());
    EXPECT_TRUE(shared_pending_file_set2.wal_index_file.IsValid());
  }
  EXPECT_TRUE(shared_file_set.read_only());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    SqliteVfsFileSetTest,
    testing::Combine(testing::Bool(), testing::Bool()),
    [](const testing::TestParamInfo<SqliteVfsFileSetTest::ParamType>& info) {
      bool single_connection = std::get<0>(info.param);
      bool journal_mode_wal = std::get<1>(info.param);
      return base::StrCat(
          {single_connection ? "SingleConnection" : "MultipleConnections",
           journal_mode_wal ? "Wal" : "Rollback"});
    });

TEST(SqliteVfsFileSetStaticsTest, GetVirtualFilePathType) {
  ASSERT_EQ(SqliteVfsFileSet::GetVirtualFileHistogramVariant(
                base::FilePath(FILE_PATH_LITERAL("47/data"))),
            "DbFile");
  ASSERT_EQ(SqliteVfsFileSet::GetVirtualFileHistogramVariant(
                base::FilePath(FILE_PATH_LITERAL("47/data-journal"))),
            "JournalFile");
  ASSERT_EQ(SqliteVfsFileSet::GetVirtualFileHistogramVariant(
                base::FilePath(FILE_PATH_LITERAL("47/data-wal"))),
            "WalJournalFile");
}

}  // namespace

}  // namespace sqlite_vfs
