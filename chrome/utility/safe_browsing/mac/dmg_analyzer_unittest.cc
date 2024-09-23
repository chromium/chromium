// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/safe_browsing/mac/dmg_analyzer.h"

#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "chrome/utility/safe_browsing/mac/dmg_iterator.h"
#include "chrome/utility/safe_browsing/mac/read_stream.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {
namespace dmg {
namespace {

class MockDMGIterator : public DMGIterator {
 public:
  struct Entry {
    std::string path;
    std::vector<uint8_t> data;
  };

  using FileList = std::vector<Entry>;

  MockDMGIterator(bool open_ok, const FileList& file_entries)
      : MockDMGIterator(open_ok, file_entries, std::vector<uint8_t>()) {}
  MockDMGIterator(bool open_ok,
                  const FileList& file_entries,
                  const std::vector<uint8_t>& code_signature)
      : DMGIterator(nullptr),
        open_ok_(open_ok),
        entries_(file_entries),
        index_(-1),
        code_signature_(code_signature) {}

  bool Open() override { return open_ok_; }

  const std::vector<uint8_t>& GetCodeSignature() override {
    return code_signature_;
  }

  bool Next() override { return ++index_ < entries_.size(); }

  std::u16string GetPath() override {
    EXPECT_LT(index_, entries_.size());
    return base::UTF8ToUTF16(entries_[index_].path);
  }

  std::unique_ptr<ReadStream> GetReadStream() override {
    EXPECT_LT(index_, entries_.size());
    const std::vector<uint8_t>& data = entries_[index_].data;
    return std::make_unique<MemoryReadStream>(data);
  }

  bool IsEmpty() override { return entries_.empty(); }

 private:
  bool open_ok_;
  FileList entries_;
  size_t index_;
  std::vector<uint8_t> code_signature_;
};

TEST(DMGAnalyzerTest, FailToOpen) {
  base::test::TaskEnvironment task_environment;
  DMGAnalyzer analyzer_;
  base::FilePath temp_path;
  base::File temp_file;
  base::CreateTemporaryFile(&temp_path);
  temp_file.Initialize(
      temp_path, (base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_READ |
                  base::File::FLAG_WRITE | base::File::FLAG_WIN_TEMPORARY |
                  base::File::FLAG_DELETE_ON_CLOSE));
  std::unique_ptr<MockDMGIterator> iterator =
      std::make_unique<MockDMGIterator>(false, MockDMGIterator::FileList());
  safe_browsing::ArchiveAnalyzerResults results;
  base::RunLoop run_loop;
  analyzer_.AnalyzeDMGFileForTesting(std::move(iterator), &results,
                                     std::move(temp_file),
                                     run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(results.success);
  EXPECT_FALSE(results.has_archive);
  EXPECT_FALSE(results.has_executable);
  EXPECT_TRUE(results.archived_binary.empty());
}

TEST(DMGAnalyzerTest, EmptyDMG) {
  base::test::TaskEnvironment task_environment;
  DMGAnalyzer analyzer_;
  base::FilePath temp_path;
  base::File temp_file;
  base::CreateTemporaryFile(&temp_path);
  temp_file.Initialize(
      temp_path, (base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_READ |
                  base::File::FLAG_WRITE | base::File::FLAG_WIN_TEMPORARY |
                  base::File::FLAG_DELETE_ON_CLOSE));
  std::unique_ptr<MockDMGIterator> iterator =
      std::make_unique<MockDMGIterator>(true, MockDMGIterator::FileList());
  safe_browsing::ArchiveAnalyzerResults results;
  base::RunLoop run_loop;
  analyzer_.AnalyzeDMGFileForTesting(std::move(iterator), &results,
                                     std::move(temp_file),
                                     run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(results.success);
  EXPECT_FALSE(results.has_archive);
  EXPECT_FALSE(results.has_executable);
  EXPECT_TRUE(results.archived_binary.empty());
}

TEST(DMGAnalyzerTest, DetachedCodeSignature) {
  base::test::TaskEnvironment task_environment;
  DMGAnalyzer analyzer_;
  base::FilePath temp_path;
  base::File temp_file;
  base::CreateTemporaryFile(&temp_path);
  temp_file.Initialize(
      temp_path, (base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_READ |
                  base::File::FLAG_WRITE | base::File::FLAG_WIN_TEMPORARY |
                  base::File::FLAG_DELETE_ON_CLOSE));
  base::FilePath real_code_signature_file;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT,
                                     &real_code_signature_file));
  real_code_signature_file = real_code_signature_file.AppendASCII("chrome")
                                 .AppendASCII("test")
                                 .AppendASCII("data")
                                 .AppendASCII("safe_browsing")
                                 .AppendASCII("mach_o")
                                 .AppendASCII("shell-script.app")
                                 .AppendASCII("Contents")
                                 .AppendASCII("_CodeSignature")
                                 .AppendASCII("CodeSignature");

  std::string real_code_signature;
  ASSERT_TRUE(
      base::ReadFileToString(real_code_signature_file, &real_code_signature));

  MockDMGIterator::FileList file_list{
      {"DMG/App.app/Contents/_CodeSignature/CodeSignature",
       {real_code_signature.begin(), real_code_signature.end()}},
  };

  std::unique_ptr<MockDMGIterator> iterator =
      std::make_unique<MockDMGIterator>(true, file_list);
  safe_browsing::ArchiveAnalyzerResults results;
  base::RunLoop run_loop;
  analyzer_.AnalyzeDMGFileForTesting(std::move(iterator), &results,
                                     std::move(temp_file),
                                     run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_TRUE(results.success);
  EXPECT_TRUE(results.has_executable);
  EXPECT_TRUE(results.archived_binary.empty());
  ASSERT_EQ(1, results.detached_code_signatures.size());
  EXPECT_EQ(real_code_signature,
            results.detached_code_signatures.Get(0).contents());
}

TEST(DMGAnalyzerTest, InvalidDetachedCodeSignature) {
  base::test::TaskEnvironment task_environment;
  DMGAnalyzer analyzer_;
  base::FilePath temp_path;
  base::File temp_file;
  base::CreateTemporaryFile(&temp_path);
  temp_file.Initialize(
      temp_path, (base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_READ |
                  base::File::FLAG_WRITE | base::File::FLAG_WIN_TEMPORARY |
                  base::File::FLAG_DELETE_ON_CLOSE));
  MockDMGIterator::FileList file_list{
      {"DMG/App.app/Contents/_CodeSignature/CodeSignature", {0x30, 0x80}},
  };

  std::unique_ptr<MockDMGIterator> iterator =
      std::make_unique<MockDMGIterator>(true, file_list);
  safe_browsing::ArchiveAnalyzerResults results;
  base::RunLoop run_loop;
  analyzer_.AnalyzeDMGFileForTesting(std::move(iterator), &results,
                                     std::move(temp_file),
                                     run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_TRUE(results.success);
  EXPECT_TRUE(results.has_executable);
  EXPECT_TRUE(results.archived_binary.empty());
  EXPECT_EQ(0, results.detached_code_signatures.size());
}

}  // namespace
}  // namespace dmg
}  // namespace safe_browsing
