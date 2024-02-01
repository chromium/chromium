// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/public/cpp/sandboxed_seven_zip_analyzer.h"

#include <string>
#include <utility>

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "chrome/services/file_util/fake_file_util_service.h"
#include "chrome/services/file_util/file_util_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

using ::testing::_;

}  // namespace

class SandboxedSevenZipAnalyzerTest : public ::testing::Test {
 protected:
  // A helper that provides a SandboxedSevenZipAnalyzerTest::ResultCallback that
  // will store a copy of an analyzer's results and then run a closure.
  class ResultsGetter {
   public:
    ResultsGetter(base::OnceClosure quit_closure,
                  safe_browsing::ArchiveAnalyzerResults* results)
        : quit_closure_(std::move(quit_closure)), results_(results) {
      DCHECK(results);
      results->success = false;
    }

    ResultsGetter(const ResultsGetter&) = delete;
    ResultsGetter& operator=(const ResultsGetter&) = delete;

    SandboxedSevenZipAnalyzer::ResultCallback GetCallback() {
      return base::BindOnce(&ResultsGetter::OnZipAnalyzerResults,
                            base::Unretained(this));
    }

   private:
    void OnZipAnalyzerResults(
        const safe_browsing::ArchiveAnalyzerResults& results) {
      *results_ = results;
      std::move(quit_closure_).Run();
    }

    base::OnceClosure quit_closure_;
    raw_ptr<safe_browsing::ArchiveAnalyzerResults> results_;
  };

  SandboxedSevenZipAnalyzerTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

  void SetUp() override {
    ASSERT_TRUE(
        base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &dir_test_data_));
    dir_test_data_ = dir_test_data_.Append(
        FILE_PATH_LITERAL("third_party/lzma_sdk/google/test_data"));
  }

  // Runs a sandboxed zip analyzer on |file_path|, writing its results into
  // |results|.
  void RunAnalyzer(const base::FilePath& file_path,
                   safe_browsing::ArchiveAnalyzerResults* results) {
    DCHECK(results);
    mojo::PendingRemote<chrome::mojom::FileUtilService> remote;
    FileUtilService service(remote.InitWithNewPipeAndPassReceiver());
    base::RunLoop run_loop;
    ResultsGetter results_getter(run_loop.QuitClosure(), results);
    std::unique_ptr<SandboxedSevenZipAnalyzer, base::OnTaskRunnerDeleter>
        analyzer = SandboxedSevenZipAnalyzer::CreateAnalyzer(
            file_path, results_getter.GetCallback(), std::move(remote));
    analyzer->Start();
    run_loop.Run();
  }

  base::FilePath dir_test_data_;
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(SandboxedSevenZipAnalyzerTest, NoBinaries) {
  safe_browsing::ArchiveAnalyzerResults results;
  RunAnalyzer(dir_test_data_.Append(FILE_PATH_LITERAL("empty.7z")), &results);
  ASSERT_TRUE(results.success);
  EXPECT_FALSE(results.has_executable);
  EXPECT_FALSE(results.has_archive);
  EXPECT_EQ(0, results.file_count);
  EXPECT_EQ(0, results.directory_count);
  EXPECT_EQ(0, results.archived_binary.size());
}

TEST_F(SandboxedSevenZipAnalyzerTest, OneBinary) {
  safe_browsing::ArchiveAnalyzerResults results;
  RunAnalyzer(dir_test_data_.Append(FILE_PATH_LITERAL("compressed_exe.7z")),
              &results);
  ASSERT_TRUE(results.success);
  EXPECT_TRUE(results.has_executable);
  EXPECT_FALSE(results.has_archive);
  EXPECT_EQ(1, results.file_count);
  EXPECT_EQ(0, results.directory_count);
  ASSERT_EQ(1, results.archived_binary.size());

  EXPECT_EQ("file.exe", results.archived_binary[0].file_path());
  EXPECT_EQ(ClientDownloadRequest::WIN_EXECUTABLE,
            results.archived_binary[0].download_type());
  EXPECT_EQ("B32E028F9B83C5FFB806CA7DFE7A3ECE5F1AED5A0368B0A140B35A67F5B000B3",
            base::HexEncode(results.archived_binary[0].digests().sha256()));
  EXPECT_EQ(19, results.archived_binary[0].length());
  EXPECT_FALSE(results.archived_binary[0].is_encrypted());
  EXPECT_TRUE(results.archived_binary[0].is_executable());
  EXPECT_FALSE(results.archived_binary[0].is_archive());
}

TEST_F(SandboxedSevenZipAnalyzerTest, TwoBinariesAndFolder) {
  safe_browsing::ArchiveAnalyzerResults results;
  RunAnalyzer(dir_test_data_.Append(FILE_PATH_LITERAL("file_folder_file.7z")),
              &results);
  ASSERT_TRUE(results.success);
  EXPECT_TRUE(results.has_executable);
  EXPECT_FALSE(results.has_archive);
  EXPECT_EQ(2, results.file_count);
  EXPECT_EQ(1, results.directory_count);
  ASSERT_EQ(3, results.archived_binary.size());

  EXPECT_EQ("folder", results.archived_binary[0].file_path());
  EXPECT_EQ(ClientDownloadRequest::WIN_EXECUTABLE,
            results.archived_binary[0].download_type());
  EXPECT_EQ("E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855",
            base::HexEncode(results.archived_binary[0].digests().sha256()));
  EXPECT_EQ(0, results.archived_binary[0].length());
  EXPECT_FALSE(results.archived_binary[0].is_encrypted());
  EXPECT_FALSE(results.archived_binary[0].is_executable());
  EXPECT_FALSE(results.archived_binary[0].is_archive());

  EXPECT_EQ("file.exe", results.archived_binary[1].file_path());
  EXPECT_EQ(ClientDownloadRequest::WIN_EXECUTABLE,
            results.archived_binary[1].download_type());
  EXPECT_EQ("B32E028F9B83C5FFB806CA7DFE7A3ECE5F1AED5A0368B0A140B35A67F5B000B3",
            base::HexEncode(results.archived_binary[1].digests().sha256()));
  EXPECT_EQ(19, results.archived_binary[1].length());
  EXPECT_FALSE(results.archived_binary[1].is_encrypted());
  EXPECT_TRUE(results.archived_binary[1].is_executable());
  EXPECT_FALSE(results.archived_binary[1].is_archive());

  EXPECT_EQ("file2.exe", results.archived_binary[2].file_path());
  EXPECT_EQ(ClientDownloadRequest::WIN_EXECUTABLE,
            results.archived_binary[2].download_type());
  EXPECT_EQ("B32E028F9B83C5FFB806CA7DFE7A3ECE5F1AED5A0368B0A140B35A67F5B000B3",
            base::HexEncode(results.archived_binary[2].digests().sha256()));
  EXPECT_EQ(19, results.archived_binary[2].length());
  EXPECT_FALSE(results.archived_binary[2].is_encrypted());
  EXPECT_TRUE(results.archived_binary[2].is_executable());
  EXPECT_FALSE(results.archived_binary[2].is_archive());
}

TEST_F(SandboxedSevenZipAnalyzerTest, NestedArchive) {
  safe_browsing::ArchiveAnalyzerResults results;
  RunAnalyzer(dir_test_data_.Append(FILE_PATH_LITERAL("inner_archive.7z")),
              &results);
  EXPECT_FALSE(results.success);
  EXPECT_FALSE(results.has_executable);
  EXPECT_TRUE(results.has_archive);
  EXPECT_EQ(1, results.file_count);
  EXPECT_EQ(0, results.directory_count);
  ASSERT_EQ(1, results.archived_binary.size());

  EXPECT_EQ("fake.zip", results.archived_binary[0].file_path());
  EXPECT_EQ(ClientDownloadRequest::ARCHIVE,
            results.archived_binary[0].download_type());
  EXPECT_EQ("DFD138681A2BE04D4E97A4CF839C08042A1A9F7541B4DE0EDEC4422A4D881045",
            base::HexEncode(results.archived_binary[0].digests().sha256()));
  EXPECT_EQ(10, results.archived_binary[0].length());
  EXPECT_FALSE(results.archived_binary[0].is_encrypted());
  EXPECT_FALSE(results.archived_binary[0].is_executable());
  EXPECT_TRUE(results.archived_binary[0].is_archive());
}

TEST_F(SandboxedSevenZipAnalyzerTest, Encrypted) {
  safe_browsing::ArchiveAnalyzerResults results;
  RunAnalyzer(dir_test_data_.Append(FILE_PATH_LITERAL("encrypted.7z")),
              &results);
  ASSERT_TRUE(results.success);
  EXPECT_TRUE(results.has_executable);
  EXPECT_FALSE(results.has_archive);
  EXPECT_EQ(1, results.file_count);
  EXPECT_EQ(0, results.directory_count);
  ASSERT_EQ(1, results.archived_binary.size());

  EXPECT_EQ("file.exe", results.archived_binary[0].file_path());
  EXPECT_EQ(ClientDownloadRequest::WIN_EXECUTABLE,
            results.archived_binary[0].download_type());
  EXPECT_TRUE(results.archived_binary[0].digests().sha256().empty());
  EXPECT_EQ(0, results.archived_binary[0].length());
  EXPECT_TRUE(results.archived_binary[0].is_encrypted());
  EXPECT_TRUE(results.archived_binary[0].is_executable());
  EXPECT_FALSE(results.archived_binary[0].is_archive());
}

TEST_F(SandboxedSevenZipAnalyzerTest, NotASevenZip) {
  safe_browsing::ArchiveAnalyzerResults results;
  RunAnalyzer(dir_test_data_.Append(FILE_PATH_LITERAL("not_a_seven_zip.7z")),
              &results);
  EXPECT_FALSE(results.success);
}

TEST_F(SandboxedSevenZipAnalyzerTest, CanDeleteDuringExecution) {
  base::FilePath file_path =
      dir_test_data_.Append(FILE_PATH_LITERAL("empty.7z"));
  base::FilePath temp_path;
  ASSERT_TRUE(base::CreateTemporaryFile(&temp_path));
  ASSERT_TRUE(base::CopyFile(file_path, temp_path));

  mojo::PendingRemote<chrome::mojom::FileUtilService> remote;
  base::RunLoop run_loop;

  FakeFileUtilService service(remote.InitWithNewPipeAndPassReceiver());
  EXPECT_CALL(service.GetSafeArchiveAnalyzer(), AnalyzeSevenZipFile(_, _, _))
      .WillOnce(
          [&](base::File zip_file,
              mojo::PendingRemote<chrome::mojom::TemporaryFileGetter>
                  temp_file_getter,
              chrome::mojom::SafeArchiveAnalyzer::AnalyzeSevenZipFileCallback
                  callback) {
            EXPECT_TRUE(base::DeleteFile(temp_path));
            std::move(callback).Run(safe_browsing::ArchiveAnalyzerResults());
            run_loop.Quit();
          });
  std::unique_ptr<SandboxedSevenZipAnalyzer, base::OnTaskRunnerDeleter>
      analyzer = SandboxedSevenZipAnalyzer::CreateAnalyzer(
          temp_path, base::DoNothing(), std::move(remote));
  analyzer->Start();
  run_loop.Run();
}

TEST_F(SandboxedSevenZipAnalyzerTest, InvalidPath) {
  safe_browsing::ArchiveAnalyzerResults results;
  RunAnalyzer(dir_test_data_.Append(FILE_PATH_LITERAL("does_not_exit")),
              &results);
  EXPECT_FALSE(results.success);
  EXPECT_EQ(results.analysis_result,
            safe_browsing::ArchiveAnalysisResult::kFailedToOpen);
}

}  // namespace safe_browsing
