// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/cloud_content_scanning/file_analysis_request_base.h"

#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_service.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/enterprise/obfuscation/core/download_obfuscator.h"
#include "components/file_access/scoped_file_access.h"
#include "components/file_access/test/mock_scoped_file_access_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {

// Helper to cast base::DoNothing.
BinaryUploadRequest::ContentAnalysisCallback DoNothingConnector() {
  return base::DoNothing();
}

// The mime type detected for each file can vary based on the platform/builder,
// so helper functions are used to validate that at least the returned type is
// one of multiple values.
bool IsDocMimeType(const std::string& mime_type) {
  static const base::NoDestructor<std::set<std::string>> set(
      {"application/msword", "text/plain",
       // Large files can result in no mimetype being found.
       ""});
  return set->count(mime_type);
}

class MockFileAnalysisRequestBase : public FileAnalysisRequestBase {
 public:
  using FileAnalysisRequestBase::FileAnalysisRequestBase;

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  MOCK_METHOD1(ProcessZipFile, void(Data));
  MOCK_METHOD1(ProcessRarFile, void(Data));
#endif
};

}  // namespace

class FileAnalysisRequestBaseTest : public testing::Test {
 public:
  std::unique_ptr<FileAnalysisRequestBase> MakeRequest(
      base::FilePath path,
      base::FilePath file_name,
      bool delay_opening_file,
      std::string mime_type = "",
      bool is_obfuscated = false,
      bool force_sync_hash_computation = true) {
    AnalysisSettings settings;
    return std::make_unique<MockFileAnalysisRequestBase>(
        settings, path, file_name, mime_type, delay_opening_file,
        DoNothingConnector(), base::NullCallback(),
        base::SingleThreadTaskRunner::GetCurrentDefault(), base::DoNothing(),
        is_obfuscated, force_sync_hash_computation);
  }

  void GetResultsForFileContents(const std::string& file_contents,
                                 ScanRequestUploadResult* out_result,
                                 BinaryUploadRequest::Data* out_data) {
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    base::FilePath file_path = temp_dir.GetPath().AppendASCII("normal.doc");
    base::WriteFile(file_path, file_contents);

    auto request = MakeRequest(file_path, file_path.BaseName(),
                               /*delay_opening_file*/ false);

    base::test::TestFuture<ScanRequestUploadResult, BinaryUploadRequest::Data>
        future;
    request->GetRequestData(future.GetCallback());

    *out_result = future.Get<ScanRequestUploadResult>();
    *out_data = future.Get<BinaryUploadRequest::Data>();
    EXPECT_EQ(file_path, out_data->path);
    EXPECT_TRUE(out_data->contents.empty());
  }

 protected:
  base::test::TaskEnvironment task_environment;
};

TEST_F(FileAnalysisRequestBaseTest, InvalidFiles) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  {
    // Non-existent files should return UNKNOWN and have no information set.
    base::FilePath path = temp_dir.GetPath().AppendASCII("not_a_real.doc");
    auto request =
        MakeRequest(path, path.BaseName(), /*delay_opening_file*/ false);

    base::test::TestFuture<ScanRequestUploadResult, BinaryUploadRequest::Data>
        future;
    request->GetRequestData(future.GetCallback());

    auto [result, data] = future.Take();
    EXPECT_EQ(result, ScanRequestUploadResult::kUnknown);
    EXPECT_EQ(data.size, 0u);
    EXPECT_TRUE(data.contents.empty());
    EXPECT_TRUE(data.hash.empty());
    EXPECT_TRUE(data.mime_type.empty());
  }

  {
    // Directories should not be used as paths passed to GetFileSHA256Blocking,
    // so they should return UNKNOWN and have no information set.
    base::FilePath path = temp_dir.GetPath();
    auto request =
        MakeRequest(path, path.BaseName(), /*delay_opening_file*/ false);

    base::test::TestFuture<ScanRequestUploadResult, BinaryUploadRequest::Data>
        future;
    request->GetRequestData(future.GetCallback());

    auto [result, data] = future.Take();
    EXPECT_EQ(result, ScanRequestUploadResult::kUnknown);
    EXPECT_EQ(data.size, 0u);
    EXPECT_TRUE(data.contents.empty());
    EXPECT_TRUE(data.hash.empty());
    EXPECT_TRUE(data.mime_type.empty());
  }

  {
    // Empty files should return SUCCESS as they have no content to scan.
    base::FilePath path = temp_dir.GetPath().AppendASCII("empty.doc");
    base::File file(path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    auto request =
        MakeRequest(path, path.BaseName(), /*delay_opening_file*/ false);

    base::test::TestFuture<ScanRequestUploadResult, BinaryUploadRequest::Data>
        future;
    request->GetRequestData(future.GetCallback());

    auto [result, data] = future.Take();
    EXPECT_EQ(result, ScanRequestUploadResult::kSuccess);
    EXPECT_EQ(data.size, 0u);
    EXPECT_TRUE(data.contents.empty());
    EXPECT_TRUE(data.hash.empty());
    EXPECT_TRUE(data.mime_type.empty());
  }
}

TEST_F(FileAnalysisRequestBaseTest, NormalFiles) {
  ScanRequestUploadResult result;
  BinaryUploadRequest::Data data;

  std::string normal_contents = "Normal file contents";
  GetResultsForFileContents(normal_contents, &result, &data);
  EXPECT_EQ(result, ScanRequestUploadResult::kSuccess);
  EXPECT_EQ(data.size, normal_contents.size());
  EXPECT_TRUE(data.contents.empty());
  // printf "Normal file contents" | sha256sum |  tr '[:lower:]' '[:upper:]'
  EXPECT_EQ(data.hash,
            "29644C10BD036866FCFD2BDACFF340DB5DE47A90002D6AB0C42DE6A22C26158B");
  EXPECT_TRUE(IsDocMimeType(data.mime_type))
      << data.mime_type << " is not an expected mimetype";

  std::string long_contents =
      std::string(BinaryUploadService::kMaxUploadSizeBytes, 'a');
  GetResultsForFileContents(long_contents, &result, &data);
  EXPECT_EQ(result, ScanRequestUploadResult::kSuccess);
  EXPECT_EQ(data.size, long_contents.size());
  EXPECT_TRUE(data.contents.empty());
  // python3 -c "print('a' * (50 * 1024 * 1024), end='')" | sha256sum | tr
  // '[:lower:]' '[:upper:]'
  EXPECT_EQ(data.hash,
            "4F0E9C6A1A9A90F35B884D0F0E7343459C21060EEFEC6C0F2FA9DC1118DBE5BE");
  EXPECT_TRUE(IsDocMimeType(data.mime_type))
      << data.mime_type << " is not an expected mimetype";
}

TEST_F(FileAnalysisRequestBaseTest, NormalFilesDataControls) {
  ScanRequestUploadResult result;
  BinaryUploadRequest::Data data;

  file_access::MockScopedFileAccessDelegate scoped_files_access_delegate;

  EXPECT_CALL(scoped_files_access_delegate, RequestFilesAccessForSystem)
      .WillOnce(base::test::RunOnceCallback<1>(
          file_access::ScopedFileAccess::Allowed()))
      .WillOnce(base::test::RunOnceCallback<1>(
          file_access::ScopedFileAccess::Allowed()));

  std::string normal_contents = "Normal file contents";
  GetResultsForFileContents(normal_contents, &result, &data);
  EXPECT_EQ(result, ScanRequestUploadResult::kSuccess);
  EXPECT_EQ(data.size, normal_contents.size());
  EXPECT_TRUE(data.contents.empty());
  // printf "Normal file contents" | sha256sum |  tr '[:lower:]' '[:upper:]'
  EXPECT_EQ(data.hash,
            "29644C10BD036866FCFD2BDACFF340DB5DE47A90002D6AB0C42DE6A22C26158B");
  EXPECT_TRUE(IsDocMimeType(data.mime_type))
      << data.mime_type << " is not an expected mimetype";

  std::string long_contents =
      std::string(BinaryUploadService::kMaxUploadSizeBytes, 'a');
  GetResultsForFileContents(long_contents, &result, &data);
  EXPECT_EQ(result, ScanRequestUploadResult::kSuccess);
  EXPECT_EQ(data.size, long_contents.size());
  EXPECT_TRUE(data.contents.empty());
  // python3 -c "print('a' * (50 * 1024 * 1024), end='')" | sha256sum | tr
  // '[:lower:]' '[:upper:]'
  EXPECT_EQ(data.hash,
            "4F0E9C6A1A9A90F35B884D0F0E7343459C21060EEFEC6C0F2FA9DC1118DBE5BE");
  EXPECT_TRUE(IsDocMimeType(data.mime_type))
      << data.mime_type << " is not an expected mimetype";
}

class FileAnalysisRequestBaseHashInFinalInvariantTest
    : public FileAnalysisRequestBaseTest,
      public testing::WithParamInterface<bool> {
 public:
  bool is_content_hash_in_final_call_enabled() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(,
                         FileAnalysisRequestBaseHashInFinalInvariantTest,
                         testing::Bool());

TEST_P(FileAnalysisRequestBaseHashInFinalInvariantTest,
       LargeFileAlwaysHasHashWhenNotDelayOpen) {
  enterprise_connectors::ScanRequestUploadResult result;
  BinaryUploadRequest::Data data;
  base::test::ScopedFeatureList scoped_feature_list;

  if (is_content_hash_in_final_call_enabled()) {
    scoped_feature_list.InitAndEnableFeature(
        enterprise_connectors::kContentHashInFileUploadFinalCall);
  } else {
    scoped_feature_list.InitAndDisableFeature(
        enterprise_connectors::kContentHashInFileUploadFinalCall);
  }

  std::string large_file_contents(BinaryUploadService::kMaxUploadSizeBytes + 1,
                                  'a');
  GetResultsForFileContents(large_file_contents, &result, &data);
  EXPECT_EQ(result, ScanRequestUploadResult::kFileTooLarge);
  EXPECT_EQ(data.size, large_file_contents.size());
  EXPECT_TRUE(data.contents.empty());
  // python3 -c "print('a' * (50 * 1024 * 1024 + 1), end='')" | sha256sum | tr
  // '[:lower:]' '[:upper:]'
  EXPECT_EQ(data.hash,
            "9EB56DB30C49E131459FE735BA6B9D38327376224EC8D5A1233F43A5B4A25942");
  EXPECT_TRUE(IsDocMimeType(data.mime_type))
      << data.mime_type << " is not an expected mimetype";

  std::string very_large_file_contents(
      2 * BinaryUploadService::kMaxUploadSizeBytes, 'a');
  GetResultsForFileContents(very_large_file_contents, &result, &data);
  EXPECT_EQ(result, ScanRequestUploadResult::kFileTooLarge);
  EXPECT_EQ(data.size, very_large_file_contents.size());
  EXPECT_TRUE(data.contents.empty());
  // python3 -c "print('a' * (100 * 1024 * 1024), end='')" | sha256sum | tr
  // '[:lower:]' '[:upper:]'
  EXPECT_EQ(data.hash,
            "CEE41E98D0A6AD65CC0EC77A2BA50BF26D64DC9007F7F1C7D7DF68B8B71291A6");
  EXPECT_TRUE(IsDocMimeType(data.mime_type))
      << data.mime_type << " is not an expected mimetype";
}

TEST_F(FileAnalysisRequestBaseTest, NewFileLimitSet) {
  base::test::ScopedFeatureList scoped_feature_list;

  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kEnableNewUploadSizeLimit, {{"max_file_size_mb", "100"}});

  ScanRequestUploadResult result;
  BinaryUploadRequest::Data data;

  // Lower than the new limit of 100MB.
  std::string small_file_contents(100 * 1024 * 1024 - 1, 'a');
  GetResultsForFileContents(small_file_contents, &result, &data);
  EXPECT_EQ(result, ScanRequestUploadResult::kSuccess);
  EXPECT_EQ(data.size, small_file_contents.size());
  EXPECT_TRUE(data.contents.empty());

  // Above the new limit of 100MB.
  std::string large_file_contents(100 * 1024 * 1024 + 1, 'a');
  GetResultsForFileContents(large_file_contents, &result, &data);
  EXPECT_EQ(result, ScanRequestUploadResult::kFileTooLarge);
  EXPECT_EQ(data.size, large_file_contents.size());
  EXPECT_TRUE(data.contents.empty());
  // python3 -c "print('a' * (100 * 1024 * 1024 + 1), end='')" | sha256sum | tr
  // '[:lower:]' '[:upper:]'
  EXPECT_EQ(data.hash,
            "700A2A19FF7AE59E77BAE4E504371B6E5FF0F1698F02CF50F99AF3F20B02A6FB");
  EXPECT_TRUE(IsDocMimeType(data.mime_type))
      << data.mime_type << " is not an expected mimetype";
}

TEST_F(FileAnalysisRequestBaseTest, PopulatesDigest) {
  std::string file_contents = "Normal file contents";
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("foo.doc");

  // Create the file.
  base::File file(file_path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  file.WriteAtCurrentPos(base::as_byte_span(file_contents));

  auto request = MakeRequest(file_path, file_path.BaseName(),
                             /*delay_opening_file*/ false);

  base::RunLoop run_loop;
  request->GetRequestData(
      base::IgnoreArgs<ScanRequestUploadResult, BinaryUploadRequest::Data>(
          run_loop.QuitClosure()));
  run_loop.Run();

  // printf "Normal file contents" | sha256sum |  tr '[:lower:]' '[:upper:]'
  EXPECT_EQ(request->digest(),
            "29644C10BD036866FCFD2BDACFF340DB5DE47A90002D6AB0C42DE6A22C26158B");
}

TEST_F(FileAnalysisRequestBaseTest, PopulatesFilename) {
  std::string file_contents = "contents";
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("foo.doc");

  // Create the file.
  base::File file(file_path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  file.WriteAtCurrentPos(base::as_byte_span(file_contents));

  auto request = MakeRequest(file_path, file_path.BaseName(),
                             /*delay_opening_file*/ false);

  base::RunLoop run_loop;
  request->GetRequestData(
      base::IgnoreArgs<ScanRequestUploadResult, BinaryUploadRequest::Data>(
          run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_EQ(request->filename(), file_path.AsUTF8Unsafe());
}

TEST_F(FileAnalysisRequestBaseTest, CachesResults) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  std::string normal_contents = "Normal file contents";
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("normal.doc");
  base::WriteFile(file_path, normal_contents);

  auto request = MakeRequest(file_path, file_path.BaseName(),
                             /*delay_opening_file*/ false);

  base::test::TestFuture<ScanRequestUploadResult, BinaryUploadRequest::Data>
      future;
  request->GetRequestData(future.GetCallback());

  auto [async_result, async_data] = future.Take();

  request->GetRequestData(future.GetCallback());

  auto [sync_result, sync_data] = future.Take();

  EXPECT_EQ(sync_result, async_result);
  EXPECT_EQ(sync_data.contents, async_data.contents);
  EXPECT_EQ(sync_data.size, async_data.size);
  EXPECT_EQ(sync_data.hash, async_data.hash);
  EXPECT_EQ(sync_data.mime_type, async_data.mime_type);
}

TEST_F(FileAnalysisRequestBaseTest, CachesResultsWithKnownMimetype) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  std::string normal_contents = "Normal file contents";
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("normal.doc");
  base::WriteFile(file_path, normal_contents);

  auto request = MakeRequest(file_path, file_path.BaseName(),
                             /*delay_opening_file*/ false, "fake/mimetype");

  base::test::TestFuture<ScanRequestUploadResult, BinaryUploadRequest::Data>
      future;
  request->GetRequestData(future.GetCallback());

  auto [result, data] = future.Take();

  EXPECT_EQ(result, ScanRequestUploadResult::kSuccess);
  EXPECT_EQ(data.size, normal_contents.size());
  EXPECT_TRUE(data.contents.empty());
  // printf "Normal file contents" | sha256sum | tr '[:lower:]' '[:upper:]'
  EXPECT_EQ(data.hash,
            "29644C10BD036866FCFD2BDACFF340DB5DE47A90002D6AB0C42DE6A22C26158B");
  EXPECT_EQ(request->digest(), data.hash);
  EXPECT_EQ(request->content_type(), "fake/mimetype");
  EXPECT_EQ(data.size, 20UL);  // printf "Normal file contents" | wc -c
  EXPECT_EQ(request->file_size(), data.size);
}

TEST_F(FileAnalysisRequestBaseTest, DelayedFileOpening) {
  std::string file_contents = "Normal file contents";
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("foo.doc");

  // Create the file.
  base::File file(file_path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  file.WriteAtCurrentPos(base::as_byte_span(file_contents));

  auto request =
      MakeRequest(file_path, file_path.BaseName(), /*delay_opening_file*/ true);

  base::RunLoop run_loop;
  request->GetRequestData(base::BindLambdaForTesting(
      [&run_loop, &file_contents](ScanRequestUploadResult result,
                                  BinaryUploadRequest::Data data) {
        run_loop.Quit();

        EXPECT_EQ(result, ScanRequestUploadResult::kSuccess);
        EXPECT_EQ(data.size, file_contents.size());
        EXPECT_TRUE(data.contents.empty());
        // printf "Normal file contents" | sha256sum |\
        // tr '[:lower:]' '[:upper:]'
        EXPECT_EQ(
            data.hash,
            "29644C10BD036866FCFD2BDACFF340DB5DE47A90002D6AB0C42DE6A22C26158B");
        EXPECT_TRUE(IsDocMimeType(data.mime_type))
            << data.mime_type << " is not an expected mimetype";
      }));

  EXPECT_FALSE(run_loop.AnyQuitCalled());
  request->OpenFile();
  run_loop.Run();

  EXPECT_TRUE(run_loop.AnyQuitCalled());
}

TEST_F(FileAnalysisRequestBaseTest, ObfuscatedFile) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      enterprise_obfuscation::kEnterpriseFileObfuscation);

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Create original file contents.
  std::vector<uint8_t> original_contents(5000, 'a');
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("obfuscated");

  // Obfuscate the file contents and write to file.
  enterprise_obfuscation::DownloadObfuscator obfuscator;
  auto obfuscation_result =
      obfuscator.ObfuscateChunk(base::span(original_contents), true);

  ASSERT_TRUE(obfuscation_result.has_value());
  ASSERT_TRUE(base::WriteFile(file_path, obfuscation_result.value()));

  auto obfuscated_request = MakeRequest(file_path, file_path.BaseName(),
                                        /*delay_opening_file=*/false,
                                        /*mime_type=*/"",
                                        /*is_obfuscated=*/true);
  base::test::TestFuture<ScanRequestUploadResult, BinaryUploadRequest::Data>
      future;
  obfuscated_request->GetRequestData(future.GetCallback());
  auto [result, data] = future.Take();

  EXPECT_EQ(result, ScanRequestUploadResult::kSuccess);

  // Check if size has been updated to use the calculated unobfuscated content
  // size.
  EXPECT_EQ(data.size, original_contents.size());
}

TEST_F(FileAnalysisRequestBaseTest, FileHashComputesAsyncWhenEnabled) {
  base::test::ScopedFeatureList scoped_feature_list(
      enterprise_connectors::kContentHashInFileUploadFinalCall);

  std::string large_file_contents(BinaryUploadService::kMaxUploadSizeBytes + 1,
                                  'a');
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("foo.doc");

  // Create the file.
  base::File file(file_path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  file.WriteAtCurrentPos(base::as_byte_span(large_file_contents));

  auto request =
      MakeRequest(file_path, file_path.BaseName(), /*delay_opening_file*/ true,
                  "", false, /*force_sync_hash_computation=*/false);

  base::RunLoop run_loop;

  request->GetRequestData(base::BindLambdaForTesting(
      [&request, &run_loop, &large_file_contents](
          enterprise_connectors::ScanRequestUploadResult result,
          BinaryUploadRequest::Data data) {
        EXPECT_TRUE(request->register_on_got_hash_callback_);
        request->register_on_got_hash_callback_.Run(
            false, base::BindLambdaForTesting([&run_loop](std::string hash) {
              // python3 -c "print('a' * (50 * 1024 * 1024 + 1), end='')" |
              // sha256sum | tr '[:lower:]' '[:upper:]'
              EXPECT_EQ(hash,
                        "9EB56DB30C49E131459FE735BA6B9D38327376224EC8D5A1233F43"
                        "A5B4A25942");
              run_loop.Quit();
            }));

        EXPECT_EQ(
            result,
            enterprise_connectors::ScanRequestUploadResult::kFileTooLarge);
        EXPECT_EQ(data.size, large_file_contents.size());
        EXPECT_TRUE(data.contents.empty());
        EXPECT_EQ(data.hash, "");
        EXPECT_TRUE(IsDocMimeType(data.mime_type))
            << data.mime_type << " is not an expected mimetype";
      }));

  request->OpenFile();

  run_loop.Run();
  EXPECT_TRUE(run_loop.AnyQuitCalled());
}

}  // namespace enterprise_connectors
