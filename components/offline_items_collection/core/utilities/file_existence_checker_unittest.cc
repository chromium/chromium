// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_items_collection/core/utilities/file_existence_checker.h"

#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_items_collection {

using FileIdPairVector = FileExistenceChecker::FileWithIdCollection<int64_t>;
using FileIdPairSet = std::set<std::pair<base::FilePath, int64_t>>;
namespace {

FileIdPairSet CheckForMissingFiles(
    const scoped_refptr<base::TestSimpleTaskRunner>& task_runner,
    FileIdPairVector file_paths) {
  FileIdPairSet missing_files;
  FileExistenceChecker::CheckForMissingFiles(
      task_runner, std::move(file_paths),
      base::BindOnce(
          [](FileIdPairSet* set_alias, FileIdPairVector result) {
            set_alias->insert(result.begin(), result.end());
          },
          &missing_files));
  task_runner->RunUntilIdle();
  return missing_files;
}

}  // namespace

class FileExistenceCheckerTest : public testing::Test {
 public:
  FileExistenceCheckerTest();
  ~FileExistenceCheckerTest() override;

  FileIdPairVector CreateTestFiles(int64_t count);

  base::TestSimpleTaskRunner* task_runner() { return task_runner_.get(); }
  scoped_refptr<base::TestSimpleTaskRunner>& task_runner_ref() {
    return task_runner_;
  }

 private:
  base::ScopedTempDir temp_directory_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::SingleThreadTaskRunner::CurrentDefaultHandle
      task_runner_current_default_handle_;
};

FileExistenceCheckerTest::FileExistenceCheckerTest()
    : task_runner_(new base::TestSimpleTaskRunner),
      task_runner_current_default_handle_(task_runner_) {
  EXPECT_TRUE(temp_directory_.CreateUniqueTempDir());
}

FileExistenceCheckerTest::~FileExistenceCheckerTest() {}

FileIdPairVector FileExistenceCheckerTest::CreateTestFiles(int64_t count) {
  base::FilePath file_path;
  FileIdPairVector created_files;
  for (int64_t i = 0; i < count; i++) {
    EXPECT_TRUE(
        base::CreateTemporaryFileInDir(temp_directory_.GetPath(), &file_path));
    created_files.push_back(std::make_pair(file_path, i));
  }

  return created_files;
}

TEST_F(FileExistenceCheckerTest, NoFilesMissing) {
  FileIdPairVector files = CreateTestFiles(2);
  FileIdPairSet missing_files = CheckForMissingFiles(task_runner_ref(), files);
  EXPECT_TRUE(missing_files.empty());
}

TEST_F(FileExistenceCheckerTest, MissingFileFound) {
  FileIdPairVector files = CreateTestFiles(2);

  base::DeleteFile(files[0].first);

  FileIdPairSet missing_files = CheckForMissingFiles(task_runner_ref(), files);

  EXPECT_EQ(1UL, missing_files.size());
  EXPECT_EQ(1UL, missing_files.count(files[0]));
  EXPECT_EQ(0UL, missing_files.count(files[1]));
}

}  // namespace offline_items_collection
