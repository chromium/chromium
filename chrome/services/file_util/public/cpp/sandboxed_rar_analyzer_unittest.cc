// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/public/cpp/sandboxed_rar_analyzer.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "chrome/services/file_util/file_util_service.h"
#include "components/safe_browsing/features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "crypto/sha2.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {
namespace {

#define CDRDT(x) safe_browsing::ClientDownloadRequest_DownloadType_##x

using ::testing::UnorderedElementsAre;

class SandboxedRarAnalyzerTest : public testing::Test {
 protected:
  // Constants for validating the data reported by the analyzer.
  struct BinaryData {
    const char* file_basename;
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
    mojo::PendingRemote<chrome::mojom::FileUtilService> remote;
    FileUtilService service(remote.InitWithNewPipeAndPassReceiver());
    base::RunLoop run_loop;
    ResultsGetter results_getter(run_loop.QuitClosure(), results);
    scoped_refptr<SandboxedRarAnalyzer> analyzer(new SandboxedRarAnalyzer(
        path, results_getter.GetCallback(), std::move(remote)));
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
    ASSERT_TRUE(binary.has_file_basename());
    EXPECT_EQ(data.file_basename, binary.file_basename());
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

  static const uint8_t kEmptyZipSignature[];
  static const uint8_t kNotARarSignature[];
  static const uint8_t kSignedExeSignature[];

  static const BinaryData kEmptyZip;
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

    SandboxedRarAnalyzer::ResultCallback GetCallback() {
      return base::BindRepeating(&ResultsGetter::ResultsCallback,
                                 base::Unretained(this));
    }

   private:
    void ResultsCallback(const safe_browsing::ArchiveAnalyzerResults& results) {
      *results_ = results;
      next_closure_.Run();
    }

    base::RepeatingClosure next_closure_;
    safe_browsing::ArchiveAnalyzerResults* results_;

    DISALLOW_COPY_AND_ASSIGN(ResultsGetter);
  };

  content::BrowserTaskEnvironment task_environment_;
};

// static
const SandboxedRarAnalyzerTest::BinaryData SandboxedRarAnalyzerTest::kEmptyZip =
    {
        "empty.zip", CDRDT(ARCHIVE), kEmptyZipSignature, false, false, 22,
};

const SandboxedRarAnalyzerTest::BinaryData SandboxedRarAnalyzerTest::kNotARar =
    {
        "not_a_rar.rar", CDRDT(ARCHIVE), kNotARarSignature, false, false, 18,
};

const SandboxedRarAnalyzerTest::BinaryData
    SandboxedRarAnalyzerTest::kSignedExe = {
        "signed.exe",
        CDRDT(WIN_EXECUTABLE),
        kSignedExeSignature,
#if defined(OS_WIN)
        true,
        true,
#else
        false,
        false,
#endif
        37768,
};

// static
const uint8_t SandboxedRarAnalyzerTest::kEmptyZipSignature[] = {
    0x87, 0x39, 0xc7, 0x6e, 0x68, 0x1f, 0x90, 0x09, 0x23, 0xb9, 0x00,
    0xc9, 0xdf, 0x0e, 0xf7, 0x5c, 0xf4, 0x21, 0xd3, 0x9c, 0xab, 0xb5,
    0x46, 0x50, 0xc4, 0xb9, 0xad, 0x19, 0xb6, 0xa7, 0x6d, 0x85};

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
  EXPECT_TRUE(results.archived_binary.empty());
  EXPECT_TRUE(results.archived_archive_filenames.empty());
}

TEST_F(SandboxedRarAnalyzerTest, AnalyzeRarWithPassword) {
  // Can list files inside an archive that has password protected data.
  // passwd.rar contains 1 file: file1.txt
  base::FilePath path;
  ASSERT_NO_FATAL_FAILURE(path = GetFilePath("passwd.rar"));

  safe_browsing::ArchiveAnalyzerResults results;
  AnalyzeFile(path, &results);

  ASSERT_TRUE(results.success);
  EXPECT_FALSE(results.has_executable);
  EXPECT_TRUE(results.archived_binary.empty());
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
  ASSERT_NO_FATAL_FAILURE(path = GetFilePath(kNotARar.file_basename));

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
  EXPECT_EQ(1, results.archived_binary.size());
  EXPECT_EQ(1u, results.archived_archive_filenames.size());
  ExpectBinary(kEmptyZip, results.archived_binary.Get(0));
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
  ExpectBinary(kEmptyZip, results.archived_binary.Get(2));
  EXPECT_EQ(2u, results.archived_archive_filenames.size());

  EXPECT_THAT(
      results.archived_archive_filenames,
      UnorderedElementsAre(base::FilePath(FILE_PATH_LITERAL("not_a_rar.rar")),
                           base::FilePath(FILE_PATH_LITERAL("empty.zip"))));
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
  ASSERT_TRUE(binary.has_file_basename());
  EXPECT_EQ(kSignedExe.file_basename, binary.file_basename());
  ASSERT_TRUE(binary.has_download_type());
  EXPECT_EQ(kSignedExe.download_type, binary.download_type());
  // If we're doing content inspection, we expect to have digests.
  ASSERT_TRUE(binary.has_digests());
  ASSERT_TRUE(binary.has_length());
  EXPECT_EQ(kSignedExe.length, binary.length());

#if defined(OS_WIN)
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

}  // namespace
}  // namespace safe_browsing
