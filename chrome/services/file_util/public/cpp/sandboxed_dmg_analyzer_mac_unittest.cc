// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/public/cpp/sandboxed_dmg_analyzer_mac.h"

#include <mach-o/loader.h>
#include <stdint.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "chrome/services/file_util/fake_file_util_service.h"
#include "chrome/services/file_util/file_util_service.h"
#include "components/safe_browsing/content/common/file_type_policies.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::_;

class SandboxedDMGAnalyzerTest : public testing::Test {
 public:
  SandboxedDMGAnalyzerTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

  void AnalyzeFile(const base::FilePath& path,
                   safe_browsing::ArchiveAnalyzerResults* results) {
    mojo::PendingRemote<chrome::mojom::FileUtilService> remote;
    FileUtilService service(remote.InitWithNewPipeAndPassReceiver());
    base::RunLoop run_loop;
    ResultsGetter results_getter(run_loop.QuitClosure(), results);
    std::unique_ptr<SandboxedDMGAnalyzer, base::OnTaskRunnerDeleter> analyzer =
        SandboxedDMGAnalyzer::CreateAnalyzer(
            path,
            safe_browsing::FileTypePolicies::GetInstance()
                ->GetMaxFileSizeToAnalyze("dmg"),
            results_getter.GetCallback(), std::move(remote));
    analyzer->Start();
    run_loop.Run();
  }

  base::FilePath GetFilePath(const char* file_name) {
    base::FilePath test_data;
    EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data));
    return test_data.AppendASCII("safe_browsing")
        .AppendASCII("dmg")
        .AppendASCII("data")
        .AppendASCII(file_name);
  }

 private:
  // A helper that provides a SandboxedDMGAnalyzer::ResultCallback that will
  // store a copy of an analyzer's results and then run a closure.
  class ResultsGetter {
   public:
    ResultsGetter(base::OnceClosure next_closure,
                  safe_browsing::ArchiveAnalyzerResults* results)
        : next_closure_(std::move(next_closure)), results_(results) {}

    ResultsGetter(const ResultsGetter&) = delete;
    ResultsGetter& operator=(const ResultsGetter&) = delete;

    SandboxedDMGAnalyzer::ResultCallback GetCallback() {
      return base::BindOnce(&ResultsGetter::ResultsCallback,
                            base::Unretained(this));
    }

   private:
    void ResultsCallback(const safe_browsing::ArchiveAnalyzerResults& results) {
      *results_ = results;
      std::move(next_closure_).Run();
    }

    base::OnceClosure next_closure_;
    raw_ptr<safe_browsing::ArchiveAnalyzerResults> results_;
  };

  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(SandboxedDMGAnalyzerTest, AnalyzeDMG) {
  base::FilePath path;
  ASSERT_NO_FATAL_FAILURE(path = GetFilePath("mach_o_in_dmg.dmg"));

  safe_browsing::ArchiveAnalyzerResults results;
  AnalyzeFile(path, &results);

  EXPECT_TRUE(results.success);
  EXPECT_TRUE(results.has_executable);
  EXPECT_EQ(2, results.archived_binary.size());

  bool got_executable = false, got_dylib = false;
  for (const auto& binary : results.archived_binary) {
    const std::string& file_name = binary.file_path();
    const google::protobuf::RepeatedPtrField<
        safe_browsing::ClientDownloadRequest_MachOHeaders>& headers =
        binary.image_headers().mach_o_headers();

    EXPECT_EQ(safe_browsing::ClientDownloadRequest_DownloadType_MAC_EXECUTABLE,
              binary.download_type());

    if (file_name.find("executablefat") != std::string::npos) {
      got_executable = true;
      ASSERT_EQ(2, headers.size());

      const safe_browsing::ClientDownloadRequest_MachOHeaders& arch32 =
          headers.Get(0);
      EXPECT_EQ(15, arch32.load_commands().size());
      EXPECT_EQ(MH_MAGIC, *reinterpret_cast<const uint32_t*>(
                              arch32.mach_header().c_str()));

      const safe_browsing::ClientDownloadRequest_MachOHeaders& arch64 =
          headers.Get(1);
      EXPECT_EQ(15, arch64.load_commands().size());
      EXPECT_EQ(MH_MAGIC_64, *reinterpret_cast<const uint32_t*>(
                                 arch64.mach_header().c_str()));

      const std::string& sha256_bytes = binary.digests().sha256();
      std::string actual_sha256 = base::HexEncode(sha256_bytes);
      EXPECT_EQ(
          "E462FF752FF9D84E34D843E5D46E2012ADCBD48540A8473FB794B286A389B945",
          actual_sha256);
    } else if (file_name.find("lib64.dylib") != std::string::npos) {
      got_dylib = true;
      ASSERT_EQ(1, headers.size());

      const safe_browsing::ClientDownloadRequest_MachOHeaders& arch =
          headers.Get(0);
      EXPECT_EQ(13, arch.load_commands().size());
      EXPECT_EQ(MH_MAGIC_64,
                *reinterpret_cast<const uint32_t*>(arch.mach_header().c_str()));

      const std::string& sha256_bytes = binary.digests().sha256();
      std::string actual_sha256 = base::HexEncode(sha256_bytes);
      EXPECT_EQ(
          "2012CE4987B0FA4A5D285DF7E810560E841CFAB3054BC19E1AAB345F862A6C4E",
          actual_sha256);
    } else {
      ADD_FAILURE() << "Unexpected result file " << binary.file_path();
    }
  }

  EXPECT_TRUE(got_executable);
  EXPECT_TRUE(got_dylib);

  ASSERT_EQ(1, results.detached_code_signatures.size());
  const safe_browsing::ClientDownloadRequest_DetachedCodeSignature
      detached_signature = results.detached_code_signatures.Get(0);
  EXPECT_EQ(
      "Mach-O in DMG/shell-script.app/Contents/_CodeSignature/CodeSignature",
      detached_signature.file_name());
  EXPECT_EQ(1842u, detached_signature.contents().size());
}

TEST_F(SandboxedDMGAnalyzerTest, AnalyzeDMGNoPartitionName) {
  base::FilePath path;
  ASSERT_NO_FATAL_FAILURE(
      path = GetFilePath("mach_o_in_dmg_no_partition_name.dmg"));

  safe_browsing::ArchiveAnalyzerResults results;
  AnalyzeFile(path, &results);

  EXPECT_TRUE(results.success);
  EXPECT_TRUE(results.has_executable);
  EXPECT_EQ(2, results.archived_binary.size());

  bool got_executable = false, got_dylib = false;
  for (const auto& binary : results.archived_binary) {
    const std::string& file_name = binary.file_path();
    const google::protobuf::RepeatedPtrField<
        safe_browsing::ClientDownloadRequest_MachOHeaders>& headers =
        binary.image_headers().mach_o_headers();

    EXPECT_EQ(safe_browsing::ClientDownloadRequest_DownloadType_MAC_EXECUTABLE,
              binary.download_type());

    if (file_name.find("executablefat") != std::string::npos) {
      got_executable = true;
      ASSERT_EQ(2, headers.size());

      const safe_browsing::ClientDownloadRequest_MachOHeaders& arch32 =
          headers.Get(0);
      EXPECT_EQ(15, arch32.load_commands().size());
      EXPECT_EQ(MH_MAGIC, *reinterpret_cast<const uint32_t*>(
                              arch32.mach_header().c_str()));

      const safe_browsing::ClientDownloadRequest_MachOHeaders& arch64 =
          headers.Get(1);
      EXPECT_EQ(15, arch64.load_commands().size());
      EXPECT_EQ(MH_MAGIC_64, *reinterpret_cast<const uint32_t*>(
                                 arch64.mach_header().c_str()));

      const std::string& sha256_bytes = binary.digests().sha256();
      std::string actual_sha256 = base::HexEncode(sha256_bytes);
      EXPECT_EQ(
          "E462FF752FF9D84E34D843E5D46E2012ADCBD48540A8473FB794B286A389B945",
          actual_sha256);
    } else if (file_name.find("lib64.dylib") != std::string::npos) {
      got_dylib = true;
      ASSERT_EQ(1, headers.size());

      const safe_browsing::ClientDownloadRequest_MachOHeaders& arch =
          headers.Get(0);
      EXPECT_EQ(13, arch.load_commands().size());
      EXPECT_EQ(MH_MAGIC_64,
                *reinterpret_cast<const uint32_t*>(arch.mach_header().c_str()));

      const std::string& sha256_bytes = binary.digests().sha256();
      std::string actual_sha256 = base::HexEncode(sha256_bytes);
      EXPECT_EQ(
          "2012CE4987B0FA4A5D285DF7E810560E841CFAB3054BC19E1AAB345F862A6C4E",
          actual_sha256);
    } else {
      ADD_FAILURE() << "Unexpected result file " << binary.file_path();
    }
  }

  EXPECT_TRUE(got_executable);
  EXPECT_TRUE(got_dylib);

  ASSERT_EQ(1, results.detached_code_signatures.size());
  const safe_browsing::ClientDownloadRequest_DetachedCodeSignature
      detached_signature = results.detached_code_signatures.Get(0);
  EXPECT_EQ(
      "Mach-O in DMG/shell-script.app/Contents/_CodeSignature/CodeSignature",
      detached_signature.file_name());
  EXPECT_EQ(1842u, detached_signature.contents().size());
}

TEST_F(SandboxedDMGAnalyzerTest, AnalyzeDmgNoSignature) {
  base::FilePath unsigned_dmg;
  ASSERT_NO_FATAL_FAILURE(unsigned_dmg = GetFilePath("mach_o_in_dmg.dmg"));

  safe_browsing::ArchiveAnalyzerResults results;
  AnalyzeFile(unsigned_dmg, &results);

  EXPECT_TRUE(results.success);
  EXPECT_EQ(0u, results.signature_blob.size());
  EXPECT_EQ(nullptr, results.signature_blob.data());
}

TEST_F(SandboxedDMGAnalyzerTest, AnalyzeDmgWithSignature) {
  base::FilePath signed_dmg;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &signed_dmg));
  signed_dmg = signed_dmg.AppendASCII("safe_browsing")
                   .AppendASCII("mach_o")
                   .AppendASCII("signed-archive.dmg");

  safe_browsing::ArchiveAnalyzerResults results;
  AnalyzeFile(signed_dmg, &results);

  EXPECT_TRUE(results.success);
  EXPECT_EQ(2215u, results.signature_blob.size());

  base::FilePath signed_dmg_signature;
  EXPECT_TRUE(
      base::PathService::Get(chrome::DIR_TEST_DATA, &signed_dmg_signature));
  signed_dmg_signature = signed_dmg_signature.AppendASCII("safe_browsing")
                             .AppendASCII("mach_o")
                             .AppendASCII("signed-archive-signature.data");

  std::string from_file;
  base::ReadFileToString(signed_dmg_signature, &from_file);
  EXPECT_EQ(2215u, from_file.length());
  std::string signature(results.signature_blob.begin(),
                        results.signature_blob.end());
  EXPECT_EQ(from_file, signature);
}

TEST_F(SandboxedDMGAnalyzerTest, CanDeleteDuringExecution) {
  base::FilePath file_path;
  ASSERT_NO_FATAL_FAILURE(file_path = GetFilePath("mach_o_in_dmg.dmg"));
  base::FilePath temp_path;
  ASSERT_TRUE(base::CreateTemporaryFile(&temp_path));
  ASSERT_TRUE(base::CopyFile(file_path, temp_path));

  mojo::PendingRemote<chrome::mojom::FileUtilService> remote;
  base::RunLoop run_loop;

  FakeFileUtilService service(remote.InitWithNewPipeAndPassReceiver());
  EXPECT_CALL(service.GetSafeArchiveAnalyzer(), AnalyzeDmgFile(_, _, _))
      .WillOnce([&](base::File dmg_file,
                    mojo::PendingRemote<chrome::mojom::TemporaryFileGetter>,
                    chrome::mojom::SafeArchiveAnalyzer::AnalyzeDmgFileCallback
                        callback) {
        EXPECT_TRUE(base::DeleteFile(temp_path));
        std::move(callback).Run(safe_browsing::ArchiveAnalyzerResults());
        run_loop.Quit();
      });
  std::unique_ptr<SandboxedDMGAnalyzer, base::OnTaskRunnerDeleter> analyzer =
      SandboxedDMGAnalyzer::CreateAnalyzer(
          temp_path,
          safe_browsing::FileTypePolicies::GetInstance()
              ->GetMaxFileSizeToAnalyze("dmg"),
          base::DoNothing(), std::move(remote));
  analyzer->Start();
  run_loop.Run();
}

TEST_F(SandboxedDMGAnalyzerTest, InvalidPath) {
  base::FilePath file_path;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &file_path));
  file_path = file_path.AppendASCII("does_not_exist");

  safe_browsing::ArchiveAnalyzerResults results;
  AnalyzeFile(file_path, &results);

  EXPECT_FALSE(results.success);
  EXPECT_EQ(results.analysis_result,
            safe_browsing::ArchiveAnalysisResult::kFailedToOpen);
}

}  // namespace
