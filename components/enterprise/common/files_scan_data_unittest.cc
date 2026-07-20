// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/common/files_scan_data.h"

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace enterprise_connectors {

class FilesScanDataTest : public testing::Test {
 public:
  void SetUp() override {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  base::FilePath AddFile(base::FilePath file_name) {
    base::FilePath path = temp_dir_.GetPath().Append(file_name);
    base::File file(path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    return path;
  }

  base::FilePath SubDirPath(base::FilePath dir_name) {
    return temp_dir_.GetPath().Append(dir_name);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(FilesScanDataTest, EmptyFileList) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  FilesScanData files_scan_data;
  base::RunLoop run_loop;

  files_scan_data.ExpandPaths(run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_TRUE(files_scan_data.expanded_paths().empty());

  // Giving an incoherent list of arguments shouldn't cause issues, just return
  // an empty set of indexes.
  base::FilePath path_1 = AddFile(base::FilePath(FILE_PATH_LITERAL("a.txt")));
  base::FilePath path_2 = AddFile(base::FilePath(FILE_PATH_LITERAL("b.txt")));

  ASSERT_TRUE(files_scan_data.IndexesToBlock({true, true}).empty());
  ASSERT_TRUE(files_scan_data.IndexesToBlock({true, false}).empty());
  ASSERT_TRUE(files_scan_data.IndexesToBlock({false, true}).empty());
  ASSERT_TRUE(files_scan_data.IndexesToBlock({false, false}).empty());
  ASSERT_TRUE(files_scan_data.IndexesToBlock({true}).empty());
  ASSERT_TRUE(files_scan_data.IndexesToBlock({false}).empty());
  ASSERT_TRUE(files_scan_data.IndexesToBlock({}).empty());
}

TEST_F(FilesScanDataTest, FlatFileList) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  std::vector<base::FilePath> paths = {
      AddFile(base::FilePath(FILE_PATH_LITERAL("a.txt"))),
      AddFile(base::FilePath(FILE_PATH_LITERAL("b.txt"))),
      AddFile(base::FilePath(FILE_PATH_LITERAL("c.txt"))),
  };
  std::vector<bool> results_cases[] = {
      {true, true, true},     // No failures
      {true, false, true},    // 1 failure
      {false, false, false},  // All failures
  };

  for (const std::vector<bool>& results : results_cases) {
    FilesScanData files_scan_data(paths);

    base::RunLoop run_loop;
    files_scan_data.ExpandPaths(run_loop.QuitClosure());
    run_loop.Run();

    absl::flat_hash_set<size_t> blocked_indexes =
        files_scan_data.IndexesToBlock(results);

    ASSERT_EQ(results.size(), files_scan_data.expanded_paths().size());

    for (size_t i = 0; i < results.size(); ++i) {
      ASSERT_EQ(results[i], !blocked_indexes.count(i));
    }
  }
}

TEST_F(FilesScanDataTest, Directories) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::FilePath folder_1 = base::FilePath(FILE_PATH_LITERAL("folder_1"));
  base::FilePath folder_2 = base::FilePath(FILE_PATH_LITERAL("folder_2"));
  ASSERT_TRUE(base::CreateDirectory(SubDirPath(folder_1)));
  ASSERT_TRUE(base::CreateDirectory(SubDirPath(folder_2)));

  base::FilePath path_1_1 =
      AddFile(folder_1.Append(base::FilePath(FILE_PATH_LITERAL("a.txt"))));
  base::FilePath path_1_2 =
      AddFile(folder_1.Append(base::FilePath(FILE_PATH_LITERAL("b.txt"))));
  base::FilePath path_2_1 =
      AddFile(folder_2.Append(base::FilePath(FILE_PATH_LITERAL("a.txt"))));
  base::FilePath path_2_2 =
      AddFile(folder_2.Append(base::FilePath(FILE_PATH_LITERAL("b.txt"))));

  std::vector<base::FilePath> paths = {path_1_1, path_1_2, path_2_1, path_2_2};

  FilesScanData files_scan_data({SubDirPath(folder_1), SubDirPath(folder_2)});

  base::RunLoop run_loop;
  files_scan_data.ExpandPaths(run_loop.QuitClosure());
  run_loop.Run();

  const auto& expand = files_scan_data.expanded_paths_indexes();
  ASSERT_EQ(expand.size(), 4u);
  ASSERT_EQ(expand.at(path_1_1), 0u);
  ASSERT_EQ(expand.at(path_1_2), 0u);
  ASSERT_EQ(expand.at(path_2_1), 1u);
  ASSERT_EQ(expand.at(path_2_2), 1u);

  // No failure means no top directory gets blocked.
  absl::flat_hash_set<size_t> blocked_indexes =
      files_scan_data.IndexesToBlock({true, true, true, true});
  ASSERT_TRUE(blocked_indexes.empty());

  // One failure means the top matching directory gets blocked.
  blocked_indexes = files_scan_data.IndexesToBlock({false, true, true, true});
  ASSERT_EQ(blocked_indexes.size(), 1u);
  ASSERT_TRUE(blocked_indexes.count(0));
  blocked_indexes = files_scan_data.IndexesToBlock({true, false, true, true});
  ASSERT_EQ(blocked_indexes.size(), 1u);
  ASSERT_TRUE(blocked_indexes.count(0));
  blocked_indexes = files_scan_data.IndexesToBlock({true, true, false, true});
  ASSERT_EQ(blocked_indexes.size(), 1u);
  ASSERT_TRUE(blocked_indexes.count(1));
  blocked_indexes = files_scan_data.IndexesToBlock({true, true, true, false});
  ASSERT_EQ(blocked_indexes.size(), 1u);
  ASSERT_TRUE(blocked_indexes.count(1));

  // Two failures means the top matching directory gets blocked.
  blocked_indexes = files_scan_data.IndexesToBlock({false, false, true, true});
  ASSERT_EQ(blocked_indexes.size(), 1u);
  ASSERT_TRUE(blocked_indexes.count(0));
  blocked_indexes = files_scan_data.IndexesToBlock({false, true, false, true});
  ASSERT_EQ(blocked_indexes.size(), 2u);
  ASSERT_TRUE(blocked_indexes.count(0));
  ASSERT_TRUE(blocked_indexes.count(1));
  blocked_indexes = files_scan_data.IndexesToBlock({true, false, true, false});
  ASSERT_EQ(blocked_indexes.size(), 2u);
  ASSERT_TRUE(blocked_indexes.count(0));
  ASSERT_TRUE(blocked_indexes.count(1));
  blocked_indexes = files_scan_data.IndexesToBlock({true, true, false, false});
  ASSERT_EQ(blocked_indexes.size(), 1u);
  ASSERT_TRUE(blocked_indexes.count(1));
}

// Test case for crbug.com/501437361.
// Verifies that ChromeOS virtual paths are not dropped during expansion,
// ensuring they reach the scanner and are properly evaluated (and reported).
TEST_F(FilesScanDataTest, VirtualPathNoDrop) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Simulate the ChromeOS virtual_path.
  base::FilePath virtual_path(
      FILE_PATH_LITERAL("Downloads-deadbeef0123456789/confidential_ssn.pdf"));
  ASSERT_FALSE(virtual_path.IsAbsolute());

  base::FilePath real_path =
      AddFile(base::FilePath(FILE_PATH_LITERAL("benign.txt")));

  std::vector<base::FilePath> base_paths = {virtual_path, real_path};
  FilesScanData files_scan_data(base_paths);

  base::RunLoop run_loop;
  files_scan_data.ExpandPaths(run_loop.QuitClosure());
  run_loop.Run();

  // The virtual path is NOT dropped anymore. Both files survive expansion.
  ASSERT_EQ(files_scan_data.expanded_paths().size(), 2u);
  EXPECT_EQ(files_scan_data.expanded_paths()[0], virtual_path);
  EXPECT_EQ(files_scan_data.expanded_paths()[1], real_path);
  EXPECT_TRUE(files_scan_data.expanded_paths_indexes().contains(virtual_path));
  EXPECT_TRUE(files_scan_data.expanded_paths_indexes().contains(real_path));

  // base_paths() is round-tripped intact.
  ASSERT_EQ(files_scan_data.base_paths().size(), 2u);

  // Reconciler simulation.
  // Case A: Scanner blocks the virtual file (index 0) and allows the real file
  // (index 1).
  {
    std::vector<bool> scan_verdicts = {false, true};
    absl::flat_hash_set<size_t> blocked =
        files_scan_data.IndexesToBlock(scan_verdicts);
    EXPECT_TRUE(blocked.contains(0u));
    EXPECT_FALSE(blocked.contains(1u));
  }

  // Case B: Scanner allows the virtual file (index 0) and blocks the real file
  // (index 1).
  {
    std::vector<bool> scan_verdicts = {true, false};
    absl::flat_hash_set<size_t> blocked =
        files_scan_data.IndexesToBlock(scan_verdicts);
    EXPECT_FALSE(blocked.contains(0u));
    EXPECT_TRUE(blocked.contains(1u));
  }
}

TEST_F(FilesScanDataTest, BasePaths) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  std::vector<base::FilePath> paths = {
      AddFile(base::FilePath(FILE_PATH_LITERAL("a.txt"))),
      AddFile(base::FilePath(FILE_PATH_LITERAL("b.txt"))),
      AddFile(base::FilePath(FILE_PATH_LITERAL("c.txt"))),
  };

  FilesScanData files_scan_data(paths);
  ASSERT_EQ(files_scan_data.base_paths(), paths);

  base::RunLoop run_loop;
  files_scan_data.ExpandPaths(run_loop.QuitClosure());
  ASSERT_EQ(files_scan_data.base_paths().size(), 0u);
  run_loop.Run();

  ASSERT_EQ(files_scan_data.base_paths(), paths);
  ASSERT_EQ(files_scan_data.take_base_paths(), paths);

  // Base paths is empty after a take.
  ASSERT_EQ(files_scan_data.base_paths().size(), 0u);
  ASSERT_EQ(files_scan_data.take_base_paths().size(), 0u);
}

}  // namespace enterprise_connectors
