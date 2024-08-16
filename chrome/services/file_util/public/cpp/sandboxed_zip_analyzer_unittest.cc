// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/services/file_util/public/cpp/sandboxed_zip_analyzer.h"

#include <stdint.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "chrome/services/file_util/fake_file_util_service.h"
#include "chrome/services/file_util/file_util_service.h"
#include "chrome/services/file_util/public/mojom/safe_archive_analyzer.mojom.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "crypto/sha2.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

#if BUILDFLAG(IS_MAC)
const char kAppInZipHistogramName[] =
    "SBClientDownload.ZipFileContainsAppDirectory";
#endif  // BUILDFLAG(IS_MAC)

using testing::_;

}  // namespace

class SandboxedZipAnalyzerTest : public ::testing::Test {
 protected:
  // Constants for validating the data reported by the analyzer.
  struct BinaryData {
    const char* file_path;
    safe_browsing::ClientDownloadRequest_DownloadType download_type;
    const uint8_t* sha256_digest;
    int64_t length;
    bool is_signed;
  };

  // A helper that provides a SandboxedZipAnalyzer::ResultCallback that will
  // store a copy of an analyzer's results and then run a closure.
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

    SandboxedZipAnalyzer::ResultCallback GetCallback() {
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

  SandboxedZipAnalyzerTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

  void SetUp() override {
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &dir_test_data_));
    dir_test_data_ = dir_test_data_.AppendASCII("safe_browsing");
  }

  // Runs a sandboxed zip analyzer on |file_path|, writing its results into
  // |results|.
  void RunAnalyzer(const base::FilePath& file_path,
                   safe_browsing::ArchiveAnalyzerResults* results) {
    RunAnalyzer(file_path, /*password=*/std::nullopt, results);
  }

  void RunAnalyzer(const base::FilePath& file_path,
                   const std::string& password,
                   safe_browsing::ArchiveAnalyzerResults* results) {
    RunAnalyzer(file_path, base::optional_ref(password), results);
  }

  void RunAnalyzer(const base::FilePath& file_path,
                   base::optional_ref<const std::string> password,
                   safe_browsing::ArchiveAnalyzerResults* results) {
    DCHECK(results);
    mojo::PendingRemote<chrome::mojom::FileUtilService> remote;
    FileUtilService service(remote.InitWithNewPipeAndPassReceiver());
    base::RunLoop run_loop;
    ResultsGetter results_getter(run_loop.QuitClosure(), results);
    std::unique_ptr<SandboxedZipAnalyzer, base::OnTaskRunnerDeleter> analyzer =
        SandboxedZipAnalyzer::CreateAnalyzer(
            file_path, password.CopyAsOptional(), results_getter.GetCallback(),
            std::move(remote));
    analyzer->Start();
    run_loop.Run();
  }

#if BUILDFLAG(IS_WIN)
  void ExpectPEHeaders(
      const BinaryData& data,
      const safe_browsing::ClientDownloadRequest_ArchivedBinary& binary) {
    ASSERT_EQ(data.is_signed, binary.has_signature());
    if (data.is_signed) {
      ASSERT_LT(0, binary.signature().signed_data_size());
      ASSERT_NE(0U, binary.signature().signed_data(0).size());
    }
    ASSERT_TRUE(binary.has_image_headers());
    ASSERT_TRUE(binary.image_headers().has_pe_headers());
    EXPECT_TRUE(binary.image_headers().pe_headers().has_dos_header());
    EXPECT_TRUE(binary.image_headers().pe_headers().has_file_header());
    EXPECT_TRUE(binary.image_headers().pe_headers().has_optional_headers32());
    EXPECT_FALSE(binary.image_headers().pe_headers().has_optional_headers64());
  }
#endif

#if BUILDFLAG(IS_MAC)
  void ExpectMachOHeaders(
      const BinaryData& data,
      const safe_browsing::ClientDownloadRequest_ArchivedBinary& binary) {
    EXPECT_EQ(data.is_signed, binary.has_signature());
    if (data.is_signed) {
      ASSERT_LT(0, binary.signature().signed_data_size());
      EXPECT_NE(0U, binary.signature().signed_data(0).size());
    }
    EXPECT_TRUE(binary.has_image_headers());
    ASSERT_LT(0, binary.image_headers().mach_o_headers_size());
    EXPECT_LT(0, binary.image_headers().mach_o_headers(0).load_commands_size());
  }
#endif

  // Verifies expectations about a binary found by the analyzer.
  void ExpectBinary(
      const BinaryData& data,
      const safe_browsing::ClientDownloadRequest_ArchivedBinary& binary) {
    ASSERT_TRUE(binary.has_file_path());
    EXPECT_EQ(data.file_path, binary.file_path());
    ASSERT_TRUE(binary.has_download_type());
    EXPECT_EQ(data.download_type, binary.download_type());
    ASSERT_TRUE(binary.has_digests());
    ASSERT_TRUE(binary.digests().has_sha256());
    EXPECT_EQ(std::string(data.sha256_digest,
                          data.sha256_digest + crypto::kSHA256Length),
              binary.digests().sha256());
    EXPECT_FALSE(binary.digests().has_sha1());
    EXPECT_FALSE(binary.digests().has_md5());
    ASSERT_TRUE(binary.has_length());
    EXPECT_EQ(data.length, binary.length());
#if BUILDFLAG(IS_WIN)
    // ExtractImageFeatures for Windows, which only works on PE
    // files.
    if (binary.file_path().find(".exe") != std::string::npos) {
      ExpectPEHeaders(data, binary);
      return;
    }
#endif  // BUILDFLAG(IS_WIN)
#if BUILDFLAG(IS_MAC)
    // ExtractImageFeatures for Mac, which only works on MachO
    // files.
    if (binary.file_path().find("executablefat") != std::string::npos) {
      ExpectMachOHeaders(data, binary);
      return;
    }
#endif  // BUILDFLAG(IS_MAC)
    // No signature/image headers should be extracted on the wrong platform
    // (e.g. analyzing .exe on Mac).
    ASSERT_FALSE(binary.has_signature());
    ASSERT_FALSE(binary.has_image_headers());
  }

  static const uint8_t kUnsignedDigest[];
  static const uint8_t kSignedDigest[];
  static const uint8_t kJSEFileDigest[];
  static const BinaryData kUnsignedExe;
  static const BinaryData kSignedExe;
  static const BinaryData kJSEFile;

#if BUILDFLAG(IS_MAC)
  static const uint8_t kUnsignedMachODigest[];
  static const uint8_t kSignedMachODigest[];
  static const BinaryData kUnsignedMachO;
  static const BinaryData kSignedMachO;
#endif  // BUILDFLAG(IS_MAC)

  base::FilePath dir_test_data_;
  base::test::ScopedFeatureList scoped_feature_list;
  content::BrowserTaskEnvironment task_environment_;
};

// static
const uint8_t SandboxedZipAnalyzerTest::kUnsignedDigest[] = {
    0x1e, 0x95, 0x4d, 0x9c, 0xe0, 0x38, 0x9e, 0x2b, 0xa7, 0x44, 0x72,
    0x16, 0xf2, 0x17, 0x61, 0xf9, 0x8d, 0x1e, 0x65, 0x40, 0xc2, 0xab,
    0xec, 0xdb, 0xec, 0xff, 0x57, 0x0e, 0x36, 0xc4, 0x93, 0xdb};
const uint8_t SandboxedZipAnalyzerTest::kSignedDigest[] = {
    0xe1, 0x1f, 0xfa, 0x0c, 0x9f, 0x25, 0x23, 0x44, 0x53, 0xa9, 0xed,
    0xd1, 0xcb, 0x25, 0x1d, 0x46, 0x10, 0x7f, 0x34, 0xb5, 0x36, 0xad,
    0x74, 0x64, 0x2a, 0x85, 0x84, 0xac, 0xa8, 0xc1, 0xa8, 0xce};
const uint8_t SandboxedZipAnalyzerTest::kJSEFileDigest[] = {
    0x58, 0x91, 0xb5, 0xb5, 0x22, 0xd5, 0xdf, 0x08, 0x6d, 0x0f, 0xf0,
    0xb1, 0x10, 0xfb, 0xd9, 0xd2, 0x1b, 0xb4, 0xfc, 0x71, 0x63, 0xaf,
    0x34, 0xd0, 0x82, 0x86, 0xa2, 0xe8, 0x46, 0xf6, 0xbe, 0x03};
const SandboxedZipAnalyzerTest::BinaryData
    SandboxedZipAnalyzerTest::kUnsignedExe = {
        "unsigned.exe",
        safe_browsing::ClientDownloadRequest_DownloadType_WIN_EXECUTABLE,
        &kUnsignedDigest[0],
        36864,
        false,  // !is_signed
};
const SandboxedZipAnalyzerTest::BinaryData
    SandboxedZipAnalyzerTest::kSignedExe = {
        "signed.exe",
        safe_browsing::ClientDownloadRequest_DownloadType_WIN_EXECUTABLE,
        &kSignedDigest[0],
        37768,
        true,  // is_signed
};
const SandboxedZipAnalyzerTest::BinaryData SandboxedZipAnalyzerTest::kJSEFile =
    {
        "hello.jse",
        safe_browsing::ClientDownloadRequest_DownloadType_WIN_EXECUTABLE,
        &kJSEFileDigest[0],
        6,
        false,  // is_signed
};

#if BUILDFLAG(IS_MAC)
const uint8_t SandboxedZipAnalyzerTest::kUnsignedMachODigest[] = {
    0xe4, 0x62, 0xff, 0x75, 0x2f, 0xf9, 0xd8, 0x4e, 0x34, 0xd8, 0x43,
    0xe5, 0xd4, 0x6e, 0x20, 0x12, 0xad, 0xcb, 0xd4, 0x85, 0x40, 0xa8,
    0x47, 0x3f, 0xb7, 0x94, 0xb2, 0x86, 0xa3, 0x89, 0xb9, 0x45};
const SandboxedZipAnalyzerTest::BinaryData
    SandboxedZipAnalyzerTest::kUnsignedMachO = {
        "app-with-executables.app/Contents/MacOS/executablefat",
        safe_browsing::ClientDownloadRequest_DownloadType_WIN_EXECUTABLE,
        &kUnsignedMachODigest[0],
        16640,
        false,  // !is_signed
};
const uint8_t SandboxedZipAnalyzerTest::kSignedMachODigest[] = {
    0x59, 0x0b, 0xc9, 0xc8, 0xee, 0x6c, 0xec, 0x94, 0x46, 0xc1, 0x44,
    0xd8, 0xea, 0x2b, 0x10, 0x85, 0xb1, 0x5b, 0x5c, 0x68, 0x80, 0x9b,
    0x2c, 0x27, 0x48, 0xad, 0x04, 0x0c, 0x2a, 0x1e, 0xf8, 0x29};
const SandboxedZipAnalyzerTest::BinaryData
    SandboxedZipAnalyzerTest::kSignedMachO = {
        "app-with-executables.app/Contents/MacOS/signedexecutablefat",
        safe_browsing::ClientDownloadRequest_DownloadType_WIN_EXECUTABLE,
        &kSignedMachODigest[0],
        34176,
        true,  // !is_signed
};
#endif  // BUILDFLAG(IS_MAC)

TEST_F(SandboxedZipAnalyzerTest, NoBinaries) {
  safe_browsing::ArchiveAnalyzerResults results;
  RunAnalyzer(
      dir_test_data_.AppendASCII("download_protection/zipfile_no_binaries.zip"),
      &results);
  ASSERT_TRUE(results.success);
  EXPECT_FALSE(results.has_executable);
  EXPECT_FALSE(results.has_archive);
  ASSERT_EQ(1, results.archived_binary.size());
  EXPECT_EQ(results.archived_binary[0].file_path(), "simple_exe.txt");
  EXPECT_FALSE(results.archived_binary[0].is_executable());
  EXPECT_FALSE(results.archived_binary[0].is_archive());
}

TEST_F(SandboxedZipAnalyzerTest, OneUnsignedBinary) {
  safe_browsing::ArchiveAnalyzerResults results;
  RunAnalyzer(dir_test_data_.AppendASCII(
                  "download_protection/zipfile_one_unsigned_binary.zip"),
              &results);
  ASSERT_TRUE(results.success);
  EXPECT_TRUE(results.has_executable);
  EXPECT_FALSE(results.has_archive);
  ASSERT_EQ(1, results.archived_binary.size());
  ExpectBinary(kUnsignedExe, results.archived_binary.Get(0));
}

TEST_F(SandboxedZipAnalyzerTest, TwoBinariesOneSigned) {
  safe_browsing::ArchiveAnalyzerResults results;
  RunAnalyzer(dir_test_data_.AppendASCII(
                  "download_protection/zipfile_two_binaries_one_signed.zip"),
              &results);
  ASSERT_TRUE(results.success);
  EXPECT_TRUE(results.has_executable);
  EXPECT_FALSE(results.has_archive);
  ASSERT_EQ(2, results.archived_binary.size());
  ExpectBinary(kUnsignedExe, results.archived_binary.Get(0));
  ExpectBinary(kSignedExe, results.archived_binary.Get(1));
}

TEST_F(SandboxedZipAnalyzerTest, ZippedArchiveNoBinaries) {
  safe_browsing::ArchiveAnalyzerResults results;
  RunAnalyzer(dir_test_data_.AppendASCII(
                  "download_protection/zipfile_archive_no_binaries.zip"),
              &results);
  ASSERT_TRUE(results.success);
  EXPECT_TRUE(results.has_executable);
  EXPECT_FALSE(results.has_archive);
  EXPECT_EQ(1, results.archived_binary.size());
  ASSERT_EQ(0u, results.archived_archive_filenames.size());
  EXPECT_GT(results.archived_binary[0].length(), 0);
  EXPECT_FALSE(results.archived_binary[0].digests().sha256().empty());
}

TEST_F(SandboxedZipAnalyzerTest, ZippedNestedArchive) {
  safe_browsing::ArchiveAnalyzerResults results;
  RunAnalyzer(dir_test_data_.AppendASCII(
                  "download_protection/zipfile_nested_archives.zip"),
              &results);
  ASSERT_TRUE(results.success);
  EXPECT_TRUE(results.has_executable);
  EXPECT_FALSE(results.has_archive);
  EXPECT_EQ(6, results.archived_binary.size());
  ASSERT_EQ(0u, results.archived_archive_filenames.size());
  EXPECT_FALSE(results.archived_binary[0].digests().sha256().empty());
}

TEST_F(SandboxedZipAnalyzerTest, ZippedTooManyNestedArchive) {
  safe_browsing::ArchiveAnalyzerResults results;
  RunAnalyzer(dir_test_data_.AppendASCII(
                  "download_protection/zipfile_too_many_nested_archives.zip"),
              &results);
  ASSERT_TRUE(results.success);
  EXPECT_FALSE(results.has_executable);
  EXPECT_TRUE(results.has_archive);
  EXPECT_EQ(14, results.archived_binary.size());
  ASSERT_EQ(3u, results.archived_archive_filenames.size());
  EXPECT_FALSE(results.archived_binary[0].digests().sha256().empty());
}

TEST_F(SandboxedZipAnalyzerTest, ZippedRarArchiveNoBinaries) {
  safe_browsing::ArchiveAnalyzerResults results;
  RunAnalyzer(dir_test_data_.AppendASCII(
                  "download_protection/zipfile_rar_archive_no_binaries.zip"),
              &results);
  ASSERT_TRUE(results.success);
  EXPECT_FALSE(results.has_executable);
  EXPECT_TRUE(results.has_archive);
  EXPECT_EQ(1, results.archived_binary.size());
  ASSERT_EQ(1u, results.archived_archive_filenames.size());
  EXPECT_EQ(FILE_PATH_LITERAL("hello.rar"),
            results.archived_archive_filenames[0].value());
}

TEST_F(SandboxedZipAnalyzerTest, ZippedArchiveAndBinaries) {
  safe_browsing::ArchiveAnalyzerResults results;
  RunAnalyzer(dir_test_data_.AppendASCII(
                  "download_protection/zipfile_archive_and_binaries.zip"),
              &results);
  ASSERT_TRUE(results.success);
  EXPECT_TRUE(results.has_executable);
  EXPECT_TRUE(results.has_archive);
  ASSERT_EQ(2, results.archived_binary.size());
  ExpectBinary(kSignedExe, results.archived_binary.Get(0));
  ASSERT_EQ(1u, results.archived_archive_filenames.size());
  EXPECT_EQ(FILE_PATH_LITERAL("hello.7z"),
            results.archived_archive_filenames[0].value());
}

TEST_F(SandboxedZipAnalyzerTest,
       ZippedArchiveAndBinariesWithTrailingSpaceAndPeriodChars) {
  safe_browsing::ArchiveAnalyzerResults results;
  RunAnalyzer(dir_test_data_.AppendASCII(
                  "download_protection/zipfile_two_binaries_one_archive_"
                  "trailing_space_and_period_chars.zip"),
              &results);
  ASSERT_TRUE(results.success);
  EXPECT_TRUE(results.has_executable);
  EXPECT_FALSE(results.has_archive);
  ASSERT_EQ(3, results.archived_binary.size());

  BinaryData SignedExe = kSignedExe;
  SignedExe.file_path = "signed.exe ";
  BinaryData UnsignedExe = kUnsignedExe;
  UnsignedExe.file_path = "unsigned.exe.";
  ExpectBinary(SignedExe, results.archived_binary.Get(0));
  ExpectBinary(UnsignedExe, results.archived_binary.Get(1));
  ASSERT_EQ(0u, results.archived_archive_filenames.size());
}

TEST_F(SandboxedZipAnalyzerTest, ZippedJSEFile) {
  safe_browsing::ArchiveAnalyzerResults results;
  RunAnalyzer(dir_test_data_.AppendASCII(
                  "download_protection/zipfile_one_jse_file.zip"),
              &results);
  ASSERT_TRUE(results.success);
  EXPECT_TRUE(results.has_executable);
  EXPECT_FALSE(results.has_archive);
  ASSERT_EQ(1, results.archived_binary.size());
  ExpectBinary(kJSEFile, results.archived_binary.Get(0));
  EXPECT_TRUE(results.archived_archive_filenames.empty());
}

TEST_F(SandboxedZipAnalyzerTest, EncryptedZip) {
  safe_browsing::ArchiveAnalyzerResults results;
  RunAnalyzer(dir_test_data_.AppendASCII("download_protection/encrypted.zip"),
              /*password=*/"12345", &results);
  ASSERT_TRUE(results.success);
  EXPECT_TRUE(results.has_executable);
  EXPECT_FALSE(results.has_archive);
  ASSERT_EQ(1, results.archived_binary.size());
  ExpectBinary(kSignedExe, results.archived_binary.Get(0));

  EXPECT_TRUE(results.encryption_info.is_encrypted);
  EXPECT_TRUE(results.encryption_info.is_top_level_encrypted);
  EXPECT_EQ(results.encryption_info.password_status,
            safe_browsing::EncryptionInfo::kKnownCorrect);
}

TEST_F(SandboxedZipAnalyzerTest, EncryptedZipWrongPassword) {
  safe_browsing::ArchiveAnalyzerResults results;
  RunAnalyzer(dir_test_data_.AppendASCII("download_protection/encrypted.zip"),
              /*password=*/"67890", &results);
  ASSERT_TRUE(results.success);
  EXPECT_TRUE(results.has_executable);
  EXPECT_FALSE(results.has_archive);
  ASSERT_EQ(1, results.archived_binary.size());

  const safe_browsing::ClientDownloadRequest_ArchivedBinary& binary =
      results.archived_binary.Get(0);
  EXPECT_EQ("signed.exe", binary.file_path());
  EXPECT_EQ(safe_browsing::ClientDownloadRequest_DownloadType_WIN_EXECUTABLE,
            binary.download_type());
  EXPECT_FALSE(binary.has_digests());
  EXPECT_FALSE(binary.has_length());

  EXPECT_TRUE(results.encryption_info.is_encrypted);
  EXPECT_TRUE(results.encryption_info.is_top_level_encrypted);
  EXPECT_EQ(results.encryption_info.password_status,
            safe_browsing::EncryptionInfo::kKnownIncorrect);
}

TEST_F(SandboxedZipAnalyzerTest, EncryptedZipAes) {
  safe_browsing::ArchiveAnalyzerResults results;
  RunAnalyzer(
      dir_test_data_.AppendASCII("download_protection/encrypted_aes.zip"),
      /*password=*/"67890", &results);
  ASSERT_TRUE(results.success);
  EXPECT_TRUE(results.has_executable);
  EXPECT_FALSE(results.has_archive);
  ASSERT_EQ(1, results.archived_binary.size());

  const safe_browsing::ClientDownloadRequest_ArchivedBinary& binary =
      results.archived_binary.Get(0);
  EXPECT_EQ("signed.exe", binary.file_path());
  EXPECT_EQ(safe_browsing::ClientDownloadRequest_DownloadType_WIN_EXECUTABLE,
            binary.download_type());
  EXPECT_FALSE(binary.has_digests());
  EXPECT_FALSE(binary.has_length());

  EXPECT_TRUE(results.encryption_info.is_encrypted);
  EXPECT_TRUE(results.encryption_info.is_top_level_encrypted);
  EXPECT_EQ(results.encryption_info.password_status,
            safe_browsing::EncryptionInfo::kUnknown);
}

TEST_F(SandboxedZipAnalyzerTest, EncryptedZipAesNoPassword) {
  safe_browsing::ArchiveAnalyzerResults results;
  RunAnalyzer(
      dir_test_data_.AppendASCII("download_protection/encrypted_aes.zip"),
      &results);
  ASSERT_TRUE(results.success);
  EXPECT_TRUE(results.has_executable);
  EXPECT_FALSE(results.has_archive);
  ASSERT_EQ(1, results.archived_binary.size());

  const safe_browsing::ClientDownloadRequest_ArchivedBinary& binary =
      results.archived_binary.Get(0);
  EXPECT_EQ("signed.exe", binary.file_path());
  EXPECT_EQ(safe_browsing::ClientDownloadRequest_DownloadType_WIN_EXECUTABLE,
            binary.download_type());
  EXPECT_FALSE(binary.has_digests());
  EXPECT_FALSE(binary.has_length());

  EXPECT_TRUE(results.encryption_info.is_encrypted);
  EXPECT_TRUE(results.encryption_info.is_top_level_encrypted);
  EXPECT_EQ(results.encryption_info.password_status,
            safe_browsing::EncryptionInfo::kKnownIncorrect);
}

#if BUILDFLAG(IS_MAC)
TEST_F(SandboxedZipAnalyzerTest, ZippedAppWithUnsignedAndSignedExecutable) {
  base::HistogramTester histograms;
  histograms.ExpectTotalCount(kAppInZipHistogramName, 0);

  safe_browsing::ArchiveAnalyzerResults results;
  RunAnalyzer(dir_test_data_.AppendASCII(
                  "mach_o/zipped-app-two-executables-one-signed.zip"),
              &results);

  EXPECT_TRUE(results.success);
  EXPECT_TRUE(results.has_executable);
  EXPECT_FALSE(results.has_archive);

  // Many of the files within the app have no extension, and are therefore
  // flagged by Safe Browsing. So search for the two executables.
  bool found_unsigned = false;
  bool found_signed = false;
  for (const auto& binary : results.archived_binary) {
    if (kSignedMachO.file_path == binary.file_path()) {
      found_signed = true;
      ExpectBinary(kSignedMachO, binary);
    }

    if (kUnsignedMachO.file_path == binary.file_path()) {
      found_unsigned = true;
      ExpectBinary(kUnsignedMachO, binary);
    }
  }

  if (!found_unsigned || !found_signed) {
    LOG(ERROR) << "Expected to find: " << kSignedMachO.file_path << " and "
               << kUnsignedMachO.file_path;
    for (const auto& binary : results.archived_binary) {
      LOG(ERROR) << "Found " << binary.file_path();
    }
  }
  EXPECT_TRUE(found_unsigned);
  EXPECT_TRUE(found_signed);
}
#endif  // BUILDFLAG(IS_MAC)

TEST_F(SandboxedZipAnalyzerTest, CanDeleteDuringExecution) {
  base::FilePath file_path =
      dir_test_data_.AppendASCII("download_protection/zipfile_no_binaries.zip");
  base::FilePath temp_path;
  ASSERT_TRUE(base::CreateTemporaryFile(&temp_path));
  ASSERT_TRUE(base::CopyFile(file_path, temp_path));

  mojo::PendingRemote<chrome::mojom::FileUtilService> remote;
  base::RunLoop run_loop;

  FakeFileUtilService service(remote.InitWithNewPipeAndPassReceiver());
  EXPECT_CALL(service.GetSafeArchiveAnalyzer(), AnalyzeZipFile(_, _, _, _))
      .WillOnce([&](base::File zip_file,
                    const std::optional<std::string>& password,
                    mojo::PendingRemote<chrome::mojom::TemporaryFileGetter>
                        temp_file_getter,
                    chrome::mojom::SafeArchiveAnalyzer::AnalyzeZipFileCallback
                        callback) {
        EXPECT_TRUE(base::DeleteFile(temp_path));
        std::move(callback).Run(safe_browsing::ArchiveAnalyzerResults());
        run_loop.Quit();
      });
  std::unique_ptr<SandboxedZipAnalyzer, base::OnTaskRunnerDeleter> analyzer =
      SandboxedZipAnalyzer::CreateAnalyzer(temp_path, /*password=*/std::nullopt,
                                           base::DoNothing(),
                                           std::move(remote));
  analyzer->Start();
  run_loop.Run();
}

TEST_F(SandboxedZipAnalyzerTest, InvalidPath) {
  safe_browsing::ArchiveAnalyzerResults results;
  RunAnalyzer(dir_test_data_.AppendASCII("download_protection/does_not_exist"),
              &results);
  EXPECT_FALSE(results.success);
  EXPECT_EQ(results.analysis_result,
            safe_browsing::ArchiveAnalysisResult::kFailedToOpen);
}

TEST_F(SandboxedZipAnalyzerTest, NestedEncryptedZip) {
  safe_browsing::ArchiveAnalyzerResults results;
  RunAnalyzer(dir_test_data_.AppendASCII(
                  "download_protection/zipfile_nested_encrypted_zip.zip"),
              &results);
  EXPECT_TRUE(results.success);
  EXPECT_TRUE(results.encryption_info.is_encrypted);
  EXPECT_FALSE(results.encryption_info.is_top_level_encrypted);
}

TEST_F(SandboxedZipAnalyzerTest, NestedEncryptedRar) {
  safe_browsing::ArchiveAnalyzerResults results;
  RunAnalyzer(dir_test_data_.AppendASCII(
                  "download_protection/zipfile_nested_encrypted_archives.zip"),
              &results);
  EXPECT_TRUE(results.success);
  EXPECT_TRUE(results.encryption_info.is_encrypted);
  EXPECT_FALSE(results.encryption_info.is_top_level_encrypted);
}
