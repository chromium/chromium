// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/services/file_util/public/cpp/sandboxed_rar_analyzer.h"

#include <string>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "chrome/services/file_util/fake_file_util_service.h"
#include "chrome/services/file_util/file_util_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "crypto/sha2.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {
namespace {

#define CDRDT(x) safe_browsing::ClientDownloadRequest_DownloadType_##x

using ::testing::_;
using ::testing::UnorderedElementsAre;

class SandboxedRarAnalyzerTest : public testing::Test {
 protected:
  // Constants for validating the data reported by the analyzer.
  struct BinaryData {
    const char* file_path;
    safe_browsing::ClientDownloadRequest_DownloadType download_type;
    const uint8_t* sha256_digest;
    bool has_signature;
    bool has_image_headers;
    int64_t length;
  };

 public:
  SandboxedRarAnalyzerTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

  void AnalyzeFile(const base::FilePath& path,
                   safe_browsing::ArchiveAnalyzerResults* results) {
    AnalyzeFile(path, /*password=*/std::nullopt, results);
  }

  void AnalyzeFile(const base::FilePath& path,
                   std::optional<const std::string> password,
                   safe_browsing::ArchiveAnalyzerResults* results) {
    mojo::PendingRemote<chrome::mojom::FileUtilService> remote;
    FileUtilService service(remote.InitWithNewPipeAndPassReceiver());
    base::RunLoop run_loop;
    ResultsGetter results_getter(run_loop.QuitClosure(), results);
    std::unique_ptr<SandboxedRarAnalyzer, base::OnTaskRunnerDeleter> analyzer =
        SandboxedRarAnalyzer::CreateAnalyzer(path, /*password=*/password,
                                             results_getter.GetCallback(),
                                             std::move(remote));
    analyzer->Start();
    run_loop.Run();
  }

  base::FilePath GetFilePath(const char* file_name) {
    base::FilePath test_data;
    EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data));
    return test_data.AppendASCII("safe_browsing")
        .AppendASCII("rar")
        .AppendASCII(file_name);
  }
  // Verifies expectations about a binary found by the analyzer.
  void ExpectBinary(
      const BinaryData& data,
      const safe_browsing::ClientDownloadRequest_ArchivedBinary& binary) {
    ASSERT_TRUE(binary.has_file_path());
    EXPECT_EQ(data.file_path, binary.file_path());
    ASSERT_TRUE(binary.has_download_type());
    EXPECT_EQ(data.download_type, binary.download_type());
    ASSERT_TRUE(binary.has_digests());
    EXPECT_EQ(std::string(data.sha256_digest,
                          data.sha256_digest + crypto::kSHA256Length),
              binary.digests().sha256());
    ASSERT_TRUE(binary.has_length());
    EXPECT_EQ(data.length, binary.length());

    ASSERT_EQ(data.has_signature, binary.has_signature());
    ASSERT_EQ(data.has_image_headers, binary.has_image_headers());
  }

  static const uint8_t kNotARarSignature[];
  static const uint8_t kSignedExeSignature[];

  static const BinaryData kNotARar;
  static const BinaryData kSignedExe;

 private:
  // A helper that provides a SandboxedRarAnalyzer::ResultCallback that will
  // store a copy of an analyzer's results and then run a closure.
  class ResultsGetter {
   public:
    ResultsGetter(const base::RepeatingClosure& next_closure,
                  safe_browsing::ArchiveAnalyzerResults* results)
        : next_closure_(next_closure), results_(results) {}

    ResultsGetter(const ResultsGetter&) = delete;
    ResultsGetter& operator=(const ResultsGetter&) = delete;

    SandboxedRarAnalyzer::ResultCallback GetCallback() {
      return base::BindOnce(&ResultsGetter::ResultsCallback,
                            base::Unretained(this));
    }

   private:
    void ResultsCallback(const safe_browsing::ArchiveAnalyzerResults& results) {
      *results_ = results;
      next_closure_.Run();
    }

    base::RepeatingClosure next_closure_;
    raw_ptr<safe_browsing::ArchiveAnalyzerResults> results_;
  };
  content::BrowserTaskEnvironment task_environment_;
};

// static
const SandboxedRarAnalyzerTest::BinaryData SandboxedRarAnalyzerTest::kNotARar =
    {
        "not_a_rar.rar", CDRDT(ARCHIVE), kNotARarSignature, false, false, 18,
};

const SandboxedRarAnalyzerTest::BinaryData
    SandboxedRarAnalyzerTest::kSignedExe = {
        "signed.exe",
        CDRDT(WIN_EXECUTABLE),
        kSignedExeSignature,
#if BUILDFLAG(IS_WIN)
        true,
        true,
#else
        false,
        false,
#endif
        37768,
};

// static
const uint8_t SandboxedRarAnalyzerTest::kNotARarSignature[] = {
    0x11, 0x76, 0x44, 0x5c, 0x05, 0x7b, 0x65, 0xb7, 0x06, 0x90, 0xa1,
    0xc1, 0xa7, 0xdf, 0x08, 0x46, 0x96, 0x10, 0xfe, 0xb5, 0x59, 0xfe,
    0x9c, 0x7d, 0xe3, 0x0a, 0x7d, 0xc3, 0xde, 0xdb, 0xba, 0xb3};

const uint8_t SandboxedRarAnalyzerTest::kSignedExeSignature[] = {
    0xe1, 0x1f, 0xfa, 0x0c, 0x9f, 0x25, 0x23, 0x44, 0x53, 0xa9, 0xed,
    0xd1, 0xcb, 0x25, 0x1d, 0x46, 0x10, 0x7f, 0x34, 0xb5, 0x36, 0xad,
    0x74, 0x64, 0x2a, 0x85, 0x84, 0xac, 0xa8, 0xc1, 0xa8, 0xce};

TEST_F(SandboxedRarAnalyzerTest, AnalyzeBenignRar) {
  base::FilePath path;
  ASSERT_NO_FATAL_FAILURE(path = GetFilePath("small_archive.rar"));

  safe_browsing::ArchiveAnalyzerResults results;
  AnalyzeFile(path, &results);

  ASSERT_TRUE(results.success);
  EXPECT_FALSE(results.has_executable);
  EXPECT_EQ(results.archived_binary.size(), 1);
  EXPECT_EQ(results.archived_binary[0].file_path(), "limerick.txt");
  EXPECT_FALSE(results.archived_binary[0].is_executable());
  EXPECT_FALSE(results.archived_binary[0].is_archive());
  EXPECT_TRUE(results.archived_archive_filenames.empty());
}

TEST_F(SandboxedRarAnalyzerTest, AnalyzeEncryptedRar) {
  // Can list files inside an archive that has password protected data.
  // passwd1234.rar contains 1 file: signed.exe
  base::FilePath path;
  ASSERT_NO_FATAL_FAILURE(path = GetFilePath("passwd1234.rar"));

  safe_browsing::ArchiveAnalyzerResults results;
  AnalyzeFile(path, &results);

  ASSERT_TRUE(results.success);
  EXPECT_TRUE(results.has_executable);
  ASSERT_EQ(results.archived_binary.size(), 1);
  EXPECT_EQ(results.archived_binary[0].file_path(), "signed.exe");
  EXPECT_TRUE(results.archived_binary[0].is_executable());
  EXPECT_FALSE(results.archived_binary[0].is_archive());
  EXPECT_TRUE(results.archived_archive_filenames.empty());
}

TEST_F(SandboxedRarAnalyzerTest, AnalyzeEncryptedRarWithCorrectPassword) {
  // Can list files inside an archive that has password protected data.
  // passwd1234.rar contains 1 file: signed.exe
  base::FilePath path;
  ASSERT_NO_FATAL_FAILURE(path = GetFilePath("passwd1234.rar"));

  safe_browsing::ArchiveAnalyzerResults results;
  AnalyzeFile(path, /*password=*/"1234", &results);

  ASSERT_TRUE(results.success);
  EXPECT_TRUE(results.has_executable);
  ASSERT_EQ(results.archived_binary.size(), 1);
  EXPECT_EQ(results.archived_binary[0].file_path(), "signed.exe");
  ExpectBinary(kSignedExe, results.archived_binary.Get(0));
  EXPECT_TRUE(results.archived_binary[0].is_executable());
  EXPECT_FALSE(results.archived_binary[0].is_archive());
  EXPECT_TRUE(results.archived_archive_filenames.empty());

  EXPECT_TRUE(results.encryption_info.is_encrypted);
  EXPECT_TRUE(results.encryption_info.is_top_level_encrypted);
  EXPECT_EQ(results.encryption_info.password_status,
            EncryptionInfo::kKnownCorrect);
}

TEST_F(SandboxedRarAnalyzerTest, AnalyzeEncryptedRarWithIncorrectPassword) {
  // Can list files inside an archive that has password protected data.
  // passwd1234.rar contains 1 file: signed.exe
  base::FilePath path;
  ASSERT_NO_FATAL_FAILURE(path = GetFilePath("passwd1234.rar"));

  safe_browsing::ArchiveAnalyzerResults results;
  AnalyzeFile(path, /*password=*/"5678", &results);

  ASSERT_TRUE(results.success);
  EXPECT_TRUE(results.has_executable);
  ASSERT_EQ(results.archived_binary.size(), 1);
  EXPECT_EQ(results.archived_binary[0].file_path(), "signed.exe");
  EXPECT_TRUE(results.archived_binary[0].is_executable());
  EXPECT_FALSE(results.archived_binary[0].is_archive());
  EXPECT_TRUE(results.archived_archive_filenames.empty());

  EXPECT_TRUE(results.encryption_info.is_encrypted);
  EXPECT_TRUE(results.encryption_info.is_top_level_encrypted);
  EXPECT_EQ(results.encryption_info.password_status,
            EncryptionInfo::kKnownIncorrect);
}

TEST_F(SandboxedRarAnalyzerTest, AnalyzeRarWithPasswordMultipleFiles) {
  // Can list files inside an archive that has password protected data.
  // passwd1234_two_files.rar contains 2 files: signed.exe and text.txt
  base::FilePath path;
  ASSERT_NO_FATAL_FAILURE(path = GetFilePath("passwd1234_two_files.rar"));

  safe_browsing::ArchiveAnalyzerResults results;
  AnalyzeFile(path, &results);

  ASSERT_TRUE(results.success);
  EXPECT_TRUE(results.has_executable);
  ASSERT_EQ(results.archived_binary.size(), 2);
  EXPECT_EQ(results.archived_binary[0].file_path(), "signed.exe");
  EXPECT_TRUE(results.archived_binary[0].is_executable());
  EXPECT_FALSE(results.archived_binary[0].is_archive());
  EXPECT_EQ(results.archived_binary[1].file_path(), "text.txt");
  EXPECT_FALSE(results.archived_binary[1].is_executable());
  EXPECT_FALSE(results.archived_binary[1].is_archive());
  EXPECT_TRUE(results.archived_archive_filenames.empty());
}

TEST_F(SandboxedRarAnalyzerTest, AnalyzeRarContainingExecutable) {
  // Can detect when .rar contains executable files.
  // has_exe.rar contains 1 file: signed.exe
  base::FilePath path;
  ASSERT_NO_FATAL_FAILURE(path = GetFilePath("has_exe.rar"));

  safe_browsing::ArchiveAnalyzerResults results;
  AnalyzeFile(path, &results);

  ASSERT_TRUE(results.success);
  EXPECT_TRUE(results.has_executable);
  EXPECT_EQ(1, results.archived_binary.size());
  EXPECT_TRUE(results.archived_archive_filenames.empty());
  ExpectBinary(kSignedExe, results.archived_binary.Get(0));
}

TEST_F(SandboxedRarAnalyzerTest, AnalyzeTextAsRar) {
  // Catches when a file isn't a a valid RAR file.
  base::FilePath path;
  ASSERT_NO_FATAL_FAILURE(path = GetFilePath(kNotARar.file_path));

  safe_browsing::ArchiveAnalyzerResults results;
  AnalyzeFile(path, &results);

  ASSERT_FALSE(results.success);
  EXPECT_FALSE(results.has_executable);
  EXPECT_TRUE(results.archived_binary.empty());
  EXPECT_TRUE(results.archived_archive_filenames.empty());
}

TEST_F(SandboxedRarAnalyzerTest, AnalyzeRarContainingArchive) {
  // Can detect when .rar contains other archive files.
  // has_archive.rar contains 1 file: empty.zip
  base::FilePath path;
  ASSERT_NO_FATAL_FAILURE(path = GetFilePath("has_archive.rar"));

  safe_browsing::ArchiveAnalyzerResults results;
  AnalyzeFile(path, &results);

  ASSERT_TRUE(results.success);
  EXPECT_FALSE(results.has_executable);
  EXPECT_EQ(0, results.archived_binary.size());
  EXPECT_EQ(0u, results.archived_archive_filenames.size());
}

TEST_F(SandboxedRarAnalyzerTest, AnalyzeRarContainingAssortmentOfFiles) {
  // Can detect when .rar contains a mix of different intereting types.
  // has_exe_rar_text_zip.rar contains: signed.exe, not_a_rar.rar, text.txt,
  // empty.zip
  base::FilePath path;
  ASSERT_NO_FATAL_FAILURE(path = GetFilePath("has_exe_rar_text_zip.rar"));

  safe_browsing::ArchiveAnalyzerResults results;
  AnalyzeFile(path, &results);

  ASSERT_TRUE(results.success);
  EXPECT_TRUE(results.has_executable);
  EXPECT_EQ(3, results.archived_binary.size());
  ExpectBinary(kSignedExe, results.archived_binary.Get(0));
  ExpectBinary(kNotARar, results.archived_binary.Get(1));
  EXPECT_EQ(results.archived_binary[2].file_path(), "text.txt");
  EXPECT_FALSE(results.archived_binary[2].is_executable());
  EXPECT_FALSE(results.archived_binary[2].is_archive());
  EXPECT_EQ(1u, results.archived_archive_filenames.size());

  EXPECT_THAT(
      results.archived_archive_filenames,
      UnorderedElementsAre(base::FilePath(FILE_PATH_LITERAL("not_a_rar.rar"))));
}

TEST_F(SandboxedRarAnalyzerTest,
       AnalyzeRarContainingExecutableWithContentInspection) {
  // Can detect when .rar contains executable files.
  // has_exe.rar contains 1 file: signed.exe
  base::FilePath path;
  ASSERT_NO_FATAL_FAILURE(path = GetFilePath("has_exe.rar"));

  safe_browsing::ArchiveAnalyzerResults results;
  AnalyzeFile(path, &results);

  ASSERT_TRUE(results.success);
  EXPECT_TRUE(results.has_executable);
  EXPECT_EQ(1, results.archived_binary.size());
  EXPECT_TRUE(results.archived_archive_filenames.empty());

  const safe_browsing::ClientDownloadRequest_ArchivedBinary& binary =
      results.archived_binary.Get(0);
  ASSERT_TRUE(binary.has_file_path());
  EXPECT_EQ(kSignedExe.file_path, binary.file_path());
  ASSERT_TRUE(binary.has_download_type());
  EXPECT_EQ(kSignedExe.download_type, binary.download_type());
  // If we're doing content inspection, we expect to have digests.
  ASSERT_TRUE(binary.has_digests());
  ASSERT_TRUE(binary.has_length());
  EXPECT_EQ(kSignedExe.length, binary.length());

#if BUILDFLAG(IS_WIN)
  // On windows, we should also have a signature and image header
  ASSERT_TRUE(binary.has_signature());
  ASSERT_TRUE(binary.has_image_headers());
#else
  ASSERT_FALSE(binary.has_signature());
  ASSERT_FALSE(binary.has_image_headers());
#endif
}

TEST_F(SandboxedRarAnalyzerTest, AnalyzeMultipartRarContainingExecutable) {
  base::FilePath path;
  // Contains one part of an exe file.
  ASSERT_NO_FATAL_FAILURE(path = GetFilePath("multipart.part0001.rar"));

  safe_browsing::ArchiveAnalyzerResults results;
  AnalyzeFile(path, &results);

  ASSERT_TRUE(results.success);
  ASSERT_TRUE(results.has_executable);
  EXPECT_EQ(1, results.archived_binary.size());
  EXPECT_TRUE(results.archived_archive_filenames.empty());
}

TEST_F(SandboxedRarAnalyzerTest,
       AnalyzeMultipartRarContainingMultipleExecutables) {
  base::FilePath path;
  // Contains one part of two different exe files.
  ASSERT_NO_FATAL_FAILURE(
      path = GetFilePath("multipart_multiple_file.part0002.rar"));

  safe_browsing::ArchiveAnalyzerResults results;
  AnalyzeFile(path, &results);

  ASSERT_TRUE(results.success);
  ASSERT_TRUE(results.has_executable);
  EXPECT_EQ(2, results.archived_binary.size());
  EXPECT_TRUE(results.archived_archive_filenames.empty());
}

TEST_F(SandboxedRarAnalyzerTest, CanDeleteDuringExecution) {
  base::FilePath file_path;
  ASSERT_NO_FATAL_FAILURE(file_path = GetFilePath("small_archive.rar"));
  base::FilePath temp_path;
  ASSERT_TRUE(base::CreateTemporaryFile(&temp_path));
  ASSERT_TRUE(base::CopyFile(file_path, temp_path));

  mojo::PendingRemote<chrome::mojom::FileUtilService> remote;
  base::RunLoop run_loop;

  FakeFileUtilService service(remote.InitWithNewPipeAndPassReceiver());
  EXPECT_CALL(service.GetSafeArchiveAnalyzer(), AnalyzeRarFile(_, _, _, _))
      .WillOnce([&](base::File rar_file,
                    const std::optional<std::string>& password,
                    mojo::PendingRemote<chrome::mojom::TemporaryFileGetter>
                        temp_file_getter,
                    chrome::mojom::SafeArchiveAnalyzer::AnalyzeRarFileCallback
                        callback) {
        EXPECT_TRUE(base::DeleteFile(temp_path));
        std::move(callback).Run(safe_browsing::ArchiveAnalyzerResults());
        run_loop.Quit();
      });
  std::unique_ptr<SandboxedRarAnalyzer, base::OnTaskRunnerDeleter> analyzer =
      SandboxedRarAnalyzer::CreateAnalyzer(temp_path, /*password=*/std::nullopt,
                                           base::DoNothing(),
                                           std::move(remote));
  analyzer->Start();
  run_loop.Run();
}

TEST_F(SandboxedRarAnalyzerTest, InvalidPath) {
  base::FilePath path;
  ASSERT_NO_FATAL_FAILURE(path = GetFilePath("does_not_exist"));
  safe_browsing::ArchiveAnalyzerResults results;
  AnalyzeFile(path, &results);
  EXPECT_FALSE(results.success);
  EXPECT_EQ(results.analysis_result,
            safe_browsing::ArchiveAnalysisResult::kFailedToOpen);
}

TEST_F(SandboxedRarAnalyzerTest, HeaderEncryptionCorrectPassword) {
  base::FilePath path;
  ASSERT_NO_FATAL_FAILURE(path =
                              GetFilePath("header_encryption_passwd1234.rar"));

  safe_browsing::ArchiveAnalyzerResults results;
  AnalyzeFile(path, /*password=*/"1234", &results);

  ASSERT_TRUE(results.success);
  EXPECT_TRUE(results.has_executable);
  ASSERT_EQ(results.archived_binary.size(), 1);
  EXPECT_EQ(results.archived_binary[0].file_path(), "signed.exe");
  ExpectBinary(kSignedExe, results.archived_binary.Get(0));
  EXPECT_TRUE(results.archived_binary[0].is_executable());
  EXPECT_FALSE(results.archived_binary[0].is_archive());
  EXPECT_TRUE(results.archived_archive_filenames.empty());

  EXPECT_TRUE(results.encryption_info.is_encrypted);
  EXPECT_TRUE(results.encryption_info.is_top_level_encrypted);
  EXPECT_EQ(results.encryption_info.password_status,
            EncryptionInfo::kKnownCorrect);
}

TEST_F(SandboxedRarAnalyzerTest, HeaderEncryptionIncorrectPassword) {
  base::FilePath path;
  ASSERT_NO_FATAL_FAILURE(path =
                              GetFilePath("header_encryption_passwd1234.rar"));

  safe_browsing::ArchiveAnalyzerResults results;
  AnalyzeFile(path, /*password=*/"5678", &results);

  ASSERT_FALSE(results.success);
  EXPECT_FALSE(results.has_executable);
  ASSERT_EQ(results.archived_binary.size(), 0);

  EXPECT_TRUE(results.encryption_info.is_encrypted);
  EXPECT_TRUE(results.encryption_info.is_top_level_encrypted);
  EXPECT_EQ(results.encryption_info.password_status,
            EncryptionInfo::kKnownIncorrect);
}

TEST_F(SandboxedRarAnalyzerTest, HeaderEncryptionNoPassword) {
  base::FilePath path;
  ASSERT_NO_FATAL_FAILURE(path =
                              GetFilePath("header_encryption_passwd1234.rar"));

  safe_browsing::ArchiveAnalyzerResults results;
  AnalyzeFile(path, /*password=*/std::nullopt, &results);

  ASSERT_FALSE(results.success);
  EXPECT_FALSE(results.has_executable);
  ASSERT_EQ(results.archived_binary.size(), 0);

  EXPECT_TRUE(results.encryption_info.is_encrypted);
  EXPECT_TRUE(results.encryption_info.is_top_level_encrypted);
  EXPECT_EQ(results.encryption_info.password_status,
            EncryptionInfo::kKnownIncorrect);
}

}  // namespace
}  // namespace safe_browsing
