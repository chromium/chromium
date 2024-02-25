// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/browser_context_helper/file_flusher.h"

#include <map>
#include <memory>
#include <string>
#include <string_view>

#include "base/barrier_closure.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

void WriteStringToFile(const base::FilePath& path, std::string_view data) {
  ASSERT_TRUE(base::CreateDirectory(path.DirName()))
      << "Failed to create directory " << path.DirName().value();
  ASSERT_TRUE(base::WriteFile(path, data))
      << "Failed to write " << path.value();
}

}  // namespace

// Provide basic sanity test of the FileFlusher. Note it only tests that
// flush is called for the expected files but not testing the underlying
// file system for actually persisting the data.
class FileFlusherTest : public testing::Test {
 public:
  FileFlusherTest() = default;
  FileFlusherTest(const FileFlusherTest&) = delete;
  FileFlusherTest& operator=(const FileFlusherTest&) = delete;
  ~FileFlusherTest() override = default;

  // testing::Test
  void SetUp() override {
    // Create test files under a temp dir.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    const size_t kNumDirs = 2;
    const size_t kNumFiles = 3;
    for (size_t i = 1; i <= kNumDirs; ++i) {
      for (size_t j = 1; j <= kNumFiles; ++j) {
        const std::string path = base::StringPrintf("dir%zu/file%zu", i, j);
        const std::string content = base::StringPrintf("content %zu %zu", i, j);
        WriteStringToFile(temp_dir_.GetPath().AppendASCII(path), content);
      }
    }
  }

  std::unique_ptr<FileFlusher> CreateFileFlusher() {
    auto flusher = std::make_unique<FileFlusher>();
    flusher->set_on_flush_callback_for_test(
        base::BindRepeating(&FileFlusherTest::OnFlush, base::Unretained(this)));
    return flusher;
  }

  base::FilePath GetTestFilePath(const std::string& path_string) const {
    const base::FilePath path = base::FilePath::FromUTF8Unsafe(path_string);
    if (path.IsAbsolute()) {
      return path;
    }

    return temp_dir_.GetPath().Append(path);
  }

  void OnFlush(const base::FilePath& path) { ++flush_counts_[path]; }

  int GetFlushCount(const std::string& path_string) const {
    const base::FilePath path(GetTestFilePath(path_string));
    const auto& it = flush_counts_.find(path);
    return it == flush_counts_.end() ? 0 : it->second;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::map<base::FilePath, int> flush_counts_;
};

TEST_F(FileFlusherTest, Flush) {
  std::unique_ptr<FileFlusher> flusher(CreateFileFlusher());
  base::RunLoop run_loop;
  auto completion_callback = base::BarrierClosure(2, run_loop.QuitClosure());
  flusher->RequestFlush(GetTestFilePath("dir1"), /*recursive=*/false,
                        completion_callback);
  flusher->RequestFlush(GetTestFilePath("dir2"), /*recursive=*/false,
                        completion_callback);
  run_loop.Run();

  EXPECT_EQ(1, GetFlushCount("dir1/file1"));
  EXPECT_EQ(1, GetFlushCount("dir1/file2"));
  EXPECT_EQ(1, GetFlushCount("dir1/file3"));

  EXPECT_EQ(1, GetFlushCount("dir2/file1"));
  EXPECT_EQ(1, GetFlushCount("dir2/file2"));
  EXPECT_EQ(1, GetFlushCount("dir2/file3"));
}

TEST_F(FileFlusherTest, DuplicateRequests) {
  std::unique_ptr<FileFlusher> flusher(CreateFileFlusher());
  base::RunLoop run_loop;
  flusher->PauseForTest();
  auto completion_callback = base::BarrierClosure(2, run_loop.QuitClosure());
  flusher->RequestFlush(GetTestFilePath("dir1"), /*recursive=*/false,
                        completion_callback);
  flusher->RequestFlush(GetTestFilePath("dir1"), /*recursive=*/false,
                        completion_callback);
  flusher->ResumeForTest();
  run_loop.Run();

  EXPECT_EQ(1, GetFlushCount("dir1/file1"));
  EXPECT_EQ(1, GetFlushCount("dir1/file2"));
  EXPECT_EQ(1, GetFlushCount("dir1/file3"));
}

}  // namespace ash
