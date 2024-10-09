// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/drive/drive_uploader.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/drive/service/dummy_drive_service.h"
#include "google_apis/common/test_util.h"
#include "google_apis/drive/drive_api_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

using google_apis::ApiErrorCode;
using google_apis::CancelCallbackOnce;
using google_apis::FileResource;
using google_apis::HTTP_CONFLICT;
using google_apis::HTTP_CREATED;
using google_apis::HTTP_NOT_FOUND;
using google_apis::HTTP_PRECONDITION;
using google_apis::HTTP_RESUME_INCOMPLETE;
using google_apis::HTTP_SUCCESS;
using google_apis::InitiateUploadCallback;
using google_apis::NO_CONNECTION;
using google_apis::OTHER_ERROR;
using google_apis::ProgressCallback;
using google_apis::UploadRangeResponse;
using google_apis::drive::UploadRangeCallback;
namespace test_util = google_apis::test_util;

namespace drive {

namespace {

const char kTestDummyMd5[] = "dummy_md5";
const char kTestDocumentTitle[] = "Hello world";
const char kTestInitiateUploadParentResourceId[] = "parent_resource_id";
const char kTestInitiateUploadResourceId[] = "resource_id";
const char kTestMimeType[] = "text/plain";
const char kTestUploadNewFileURL[] = "http://test/upload_location/new_file";
const char kTestUploadExistingFileURL[] =
    "http://test/upload_location/existing_file";
const int64_t kUploadChunkSize = 1024 * 1024 * 1024;
const char kTestETag[] = "test_etag";

CancelCallbackOnce SendMultipartUploadResult(
    ApiErrorCode response_code,
    int64_t content_length,
    google_apis::FileResourceCallback callback,
    google_apis::ProgressCallback progress_callback) {
  // Callback progress
  if (!progress_callback.is_null()) {
    // For the testing purpose, it always notifies the progress at the end of
    // whole file uploading.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(progress_callback, content_length, content_length));
  }

  // MultipartUploadXXXFile is an asynchronous function, so don't callback
  // directly.
  auto entry = std::make_unique<FileResource>();
  entry->set_md5_checksum(kTestDummyMd5);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), response_code, std::move(entry)));
  return CancelCallbackOnce();
}

// Mock DriveService that verifies if the uploaded content matches the preset
// expectation.
class MockDriveServiceWithUploadExpectation : public DummyDriveService {
 public:
  // Sets up an expected upload content. InitiateUpload and ResumeUpload will
  // verify that the specified data is correctly uploaded.
  MockDriveServiceWithUploadExpectation(
      const base::FilePath& expected_upload_file,
      int64_t expected_content_length)
      : expected_upload_file_(expected_upload_file),
        expected_content_length_(expected_content_length),
        received_bytes_(0),
        resume_upload_call_count_(0),
        multipart_upload_call_count_(0) {}

  int64_t received_bytes() const { return received_bytes_; }
  void set_received_bytes(int64_t received_bytes) {
    received_bytes_ = received_bytes;
  }

  int64_t resume_upload_call_count() const { return resume_upload_call_count_; }
  int64_t multipart_upload_call_count() const {
    return multipart_upload_call_count_;
  }

 private:
  // DriveServiceInterface overrides.
  // Handles a request for obtaining an upload location URL.
  CancelCallbackOnce InitiateUploadNewFile(
      const std::string& content_type,
      int64_t content_length,
      const std::string& parent_resource_id,
      const std::string& title,
      const UploadNewFileOptions& options,
      InitiateUploadCallback callback) override {
    EXPECT_EQ(kTestDocumentTitle, title);
    EXPECT_EQ(kTestMimeType, content_type);
    EXPECT_EQ(expected_content_length_, content_length);
    EXPECT_EQ(kTestInitiateUploadParentResourceId, parent_resource_id);

    // Calls back the upload URL for subsequent ResumeUpload requests.
    // InitiateUpload is an asynchronous function, so don't callback directly.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), HTTP_SUCCESS,
                                  GURL(kTestUploadNewFileURL)));
    return CancelCallbackOnce();
  }

  CancelCallbackOnce InitiateUploadExistingFile(
      const std::string& content_type,
      int64_t content_length,
      const std::string& resource_id,
      const UploadExistingFileOptions& options,
      InitiateUploadCallback callback) override {
    EXPECT_EQ(kTestMimeType, content_type);
    EXPECT_EQ(expected_content_length_, content_length);
    EXPECT_EQ(kTestInitiateUploadResourceId, resource_id);

    if (!options.etag.empty() && options.etag != kTestETag) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback), HTTP_PRECONDITION, GURL()));
      return CancelCallbackOnce();
    }

    // Calls back the upload URL for subsequent ResumeUpload requests.
    // InitiateUpload is an asynchronous function, so don't callback directly.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), HTTP_SUCCESS,
                                  GURL(kTestUploadExistingFileURL)));
    return CancelCallbackOnce();
  }

  // Handles a request for uploading a chunk of bytes.
  CancelCallbackOnce ResumeUpload(const GURL& upload_location,
                                  int64_t start_position,
                                  int64_t end_position,
                                  int64_t content_length,
                                  const std::string& content_type,
                                  const base::FilePath& local_file_path,
                                  UploadRangeCallback callback,
                                  ProgressCallback progress_callback) override {
    // The upload range should start from the current first unreceived byte.
    EXPECT_EQ(received_bytes_, start_position);
    EXPECT_EQ(expected_upload_file_, local_file_path);

    // The upload data must be split into 512KB chunks.
    const int64_t expected_chunk_end =
        std::min(received_bytes_ + kUploadChunkSize, expected_content_length_);
    EXPECT_EQ(expected_chunk_end, end_position);

    // The upload URL returned by InitiateUpload() must be used.
    EXPECT_TRUE(upload_location == kTestUploadNewFileURL ||
                upload_location == kTestUploadExistingFileURL);

    // Other parameters should be the exact values passed to DriveUploader.
    EXPECT_EQ(expected_content_length_, content_length);
    EXPECT_EQ(kTestMimeType, content_type);

    // Update the internal status of the current upload session.
    resume_upload_call_count_++;
    received_bytes_ = end_position;

    // Callback progress
    if (!progress_callback.is_null()) {
      // For the testing purpose, it always notifies the progress at the end of
      // each chunk uploading.
      int64_t chunk_size = end_position - start_position;
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(progress_callback, chunk_size, chunk_size));
    }

    SendUploadRangeResponse(upload_location, std::move(callback));
    return CancelCallbackOnce();
  }

  // Handles a request to fetch the current upload status.
  CancelCallbackOnce GetUploadStatus(const GURL& upload_location,
                                     int64_t content_length,
                                     UploadRangeCallback callback) override {
    EXPECT_EQ(expected_content_length_, content_length);
    // The upload URL returned by InitiateUpload() must be used.
    EXPECT_TRUE(upload_location == kTestUploadNewFileURL ||
                upload_location == kTestUploadExistingFileURL);

    SendUploadRangeResponse(upload_location, std::move(callback));
    return CancelCallbackOnce();
  }

  // Runs |callback| with the current upload status.
  void SendUploadRangeResponse(const GURL& upload_location,
                               UploadRangeCallback callback) {
    // Callback with response.
    UploadRangeResponse response;
    std::unique_ptr<FileResource> entry;
    if (received_bytes_ == expected_content_length_) {
      ApiErrorCode response_code = upload_location == kTestUploadNewFileURL
                                       ? HTTP_CREATED
                                       : HTTP_SUCCESS;
      response = UploadRangeResponse(response_code, -1, -1);

      entry = std::make_unique<FileResource>();
      entry->set_md5_checksum(kTestDummyMd5);
    } else {
      response =
          UploadRangeResponse(HTTP_RESUME_INCOMPLETE, 0, received_bytes_);
    }
    // ResumeUpload is an asynchronous function, so don't callback directly.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), response, std::move(entry)));
  }

  CancelCallbackOnce MultipartUploadNewFile(
      const std::string& content_type,
      std::optional<std::string_view> converted_mime_type,
      int64_t content_length,
      const std::string& parent_resource_id,
      const std::string& title,
      const base::FilePath& local_file_path,
      const UploadNewFileOptions& options,
      google_apis::FileResourceCallback callback,
      google_apis::ProgressCallback progress_callback) override {
    EXPECT_EQ(kTestMimeType, content_type);
    EXPECT_EQ(expected_content_length_, content_length);
    EXPECT_EQ(converted_mime_type, std::nullopt);
    EXPECT_EQ(kTestInitiateUploadParentResourceId, parent_resource_id);
    EXPECT_EQ(kTestDocumentTitle, title);
    EXPECT_EQ(expected_upload_file_, local_file_path);

    received_bytes_ = content_length;
    multipart_upload_call_count_++;
    return SendMultipartUploadResult(HTTP_CREATED, content_length,
                                     std::move(callback), progress_callback);
  }

  CancelCallbackOnce MultipartUploadExistingFile(
      const std::string& content_type,
      int64_t content_length,
      const std::string& resource_id,
      const base::FilePath& local_file_path,
      const UploadExistingFileOptions& options,
      google_apis::FileResourceCallback callback,
      google_apis::ProgressCallback progress_callback) override {
    EXPECT_EQ(kTestMimeType, content_type);
    EXPECT_EQ(expected_content_length_, content_length);
    EXPECT_EQ(kTestInitiateUploadResourceId, resource_id);
    EXPECT_EQ(expected_upload_file_, local_file_path);

    if (!options.etag.empty() && options.etag != kTestETag) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback), HTTP_PRECONDITION, nullptr));
      return CancelCallbackOnce();
    }

    received_bytes_ = content_length;
    multipart_upload_call_count_++;
    return SendMultipartUploadResult(HTTP_SUCCESS, content_length,
                                     std::move(callback), progress_callback);
  }

  const base::FilePath expected_upload_file_;
  const int64_t expected_content_length_;
  int64_t received_bytes_;
  int64_t resume_upload_call_count_;
  int64_t multipart_upload_call_count_;
};

// Mock DriveService that returns a failure at InitiateUpload().
class MockDriveServiceNoConnectionAtInitiate : public DummyDriveService {
  // Returns error.
  CancelCallbackOnce InitiateUploadNewFile(
      const std::string& content_type,
      int64_t content_length,
      const std::string& parent_resource_id,
      const std::string& title,
      const UploadNewFileOptions& options,
      InitiateUploadCallback callback) override {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), NO_CONNECTION, GURL()));
    return CancelCallbackOnce();
  }

  CancelCallbackOnce InitiateUploadExistingFile(
      const std::string& content_type,
      int64_t content_length,
      const std::string& resource_id,
      const UploadExistingFileOptions& options,
      InitiateUploadCallback callback) override {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), NO_CONNECTION, GURL()));
    return CancelCallbackOnce();
  }

  // Should not be used.
  CancelCallbackOnce ResumeUpload(const GURL& upload_url,
                                  int64_t start_position,
                                  int64_t end_position,
                                  int64_t content_length,
                                  const std::string& content_type,
                                  const base::FilePath& local_file_path,
                                  UploadRangeCallback callback,
                                  ProgressCallback progress_callback) override {
    NOTREACHED_IN_MIGRATION();
    return CancelCallbackOnce();
  }

  CancelCallbackOnce MultipartUploadNewFile(
      const std::string& content_type,
      std::optional<std::string_view> converted_mime_type,
      int64_t content_length,
      const std::string& parent_resource_id,
      const std::string& title,
      const base::FilePath& local_file_path,
      const UploadNewFileOptions& options,
      google_apis::FileResourceCallback callback,
      google_apis::ProgressCallback progress_callback) override {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), NO_CONNECTION, nullptr));
    return CancelCallbackOnce();
  }

  CancelCallbackOnce MultipartUploadExistingFile(
      const std::string& content_type,
      int64_t content_length,
      const std::string& resource_id,
      const base::FilePath& local_file_path,
      const UploadExistingFileOptions& options,
      google_apis::FileResourceCallback callback,
      google_apis::ProgressCallback progress_callback) override {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), NO_CONNECTION, nullptr));
    return CancelCallbackOnce();
  }
};

// Mock DriveService that returns a failure at ResumeUpload().
class MockDriveServiceNoConnectionAtResume : public DummyDriveService {
  // Succeeds and returns an upload location URL.
  CancelCallbackOnce InitiateUploadNewFile(
      const std::string& content_type,
      int64_t content_length,
      const std::string& parent_resource_id,
      const std::string& title,
      const UploadNewFileOptions& options,
      InitiateUploadCallback callback) override {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), HTTP_SUCCESS,
                                  GURL(kTestUploadNewFileURL)));
    return CancelCallbackOnce();
  }

  CancelCallbackOnce InitiateUploadExistingFile(
      const std::string& content_type,
      int64_t content_length,
      const std::string& resource_id,
      const UploadExistingFileOptions& options,
      InitiateUploadCallback callback) override {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), HTTP_SUCCESS,
                                  GURL(kTestUploadExistingFileURL)));
    return CancelCallbackOnce();
  }

  // Returns error.
  CancelCallbackOnce ResumeUpload(const GURL& upload_url,
                                  int64_t start_position,
                                  int64_t end_position,
                                  int64_t content_length,
                                  const std::string& content_type,
                                  const base::FilePath& local_file_path,
                                  UploadRangeCallback callback,
                                  ProgressCallback progress_callback) override {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       UploadRangeResponse(NO_CONNECTION, -1, -1), nullptr));
    return CancelCallbackOnce();
  }
};

// Mock DriveService that returns a failure at GetUploadStatus().
class MockDriveServiceNoConnectionAtGetUploadStatus : public DummyDriveService {
  // Returns error.
  CancelCallbackOnce GetUploadStatus(const GURL& upload_url,
                                     int64_t content_length,
                                     UploadRangeCallback callback) override {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       UploadRangeResponse(NO_CONNECTION, -1, -1), nullptr));
    return CancelCallbackOnce();
  }
};

class DriveUploaderTest : public testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
};

}  // namespace

TEST_F(DriveUploaderTest, UploadExisting0KB) {
  base::FilePath local_path;
  std::string data;
  ASSERT_TRUE(test_util::CreateFileOfSpecifiedSize(temp_dir_.GetPath(), 0,
                                                   &local_path, &data));

  ApiErrorCode error = OTHER_ERROR;
  GURL upload_location;
  std::unique_ptr<FileResource> entry;

  MockDriveServiceWithUploadExpectation mock_service(local_path, data.size());
  DriveUploader uploader(
      &mock_service, base::SingleThreadTaskRunner::GetCurrentDefault().get(),
      mojo::NullRemote());
  std::vector<test_util::ProgressInfo> upload_progress_values;
  uploader.UploadExistingFile(
      kTestInitiateUploadResourceId, local_path, kTestMimeType,
      UploadExistingFileOptions(),
      test_util::CreateCopyResultCallback(&error, &upload_location, &entry),
      base::BindRepeating(&test_util::AppendProgressCallbackResult,
                          &upload_progress_values));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, mock_service.resume_upload_call_count());
  EXPECT_EQ(1, mock_service.multipart_upload_call_count());
  EXPECT_EQ(0, mock_service.received_bytes());
  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_TRUE(upload_location.is_empty());
  ASSERT_TRUE(entry);
  EXPECT_EQ(kTestDummyMd5, entry->md5_checksum());
  ASSERT_EQ(1U, upload_progress_values.size());
  EXPECT_EQ(test_util::ProgressInfo(0, 0), upload_progress_values[0]);
}

TEST_F(DriveUploaderTest, UploadExisting512KB) {
  base::FilePath local_path;
  std::string data;
  ASSERT_TRUE(test_util::CreateFileOfSpecifiedSize(
      temp_dir_.GetPath(), 512 * 1024, &local_path, &data));

  ApiErrorCode error = OTHER_ERROR;
  GURL upload_location;
  std::unique_ptr<FileResource> entry;

  MockDriveServiceWithUploadExpectation mock_service(local_path, data.size());
  DriveUploader uploader(
      &mock_service, base::SingleThreadTaskRunner::GetCurrentDefault().get(),
      mojo::NullRemote());
  std::vector<test_util::ProgressInfo> upload_progress_values;
  uploader.UploadExistingFile(
      kTestInitiateUploadResourceId, local_path, kTestMimeType,
      UploadExistingFileOptions(),
      test_util::CreateCopyResultCallback(&error, &upload_location, &entry),
      base::BindRepeating(&test_util::AppendProgressCallbackResult,
                          &upload_progress_values));
  base::RunLoop().RunUntilIdle();

  // 512KB upload should be uploaded as multipart body.
  EXPECT_EQ(0, mock_service.resume_upload_call_count());
  EXPECT_EQ(1, mock_service.multipart_upload_call_count());
  EXPECT_EQ(512 * 1024, mock_service.received_bytes());
  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_TRUE(upload_location.is_empty());
  ASSERT_TRUE(entry);
  EXPECT_EQ(kTestDummyMd5, entry->md5_checksum());
  ASSERT_EQ(1U, upload_progress_values.size());
  EXPECT_EQ(test_util::ProgressInfo(512 * 1024, 512 * 1024),
            upload_progress_values[0]);
}

TEST_F(DriveUploaderTest, UploadExisting2MB) {
  base::FilePath local_path;
  std::string data;
  ASSERT_TRUE(test_util::CreateFileOfSpecifiedSize(
      temp_dir_.GetPath(), 2 * 1024 * 1024, &local_path, &data));

  ApiErrorCode error = OTHER_ERROR;
  GURL upload_location;
  std::unique_ptr<FileResource> entry;

  MockDriveServiceWithUploadExpectation mock_service(local_path, data.size());
  DriveUploader uploader(
      &mock_service, base::SingleThreadTaskRunner::GetCurrentDefault().get(),
      mojo::NullRemote());
  std::vector<test_util::ProgressInfo> upload_progress_values;
  uploader.UploadExistingFile(
      kTestInitiateUploadResourceId, local_path, kTestMimeType,
      UploadExistingFileOptions(),
      test_util::CreateCopyResultCallback(&error, &upload_location, &entry),
      base::BindRepeating(&test_util::AppendProgressCallbackResult,
                          &upload_progress_values));
  base::RunLoop().RunUntilIdle();

  // 2MB upload should not be split into multiple chunks.
  EXPECT_EQ(1, mock_service.resume_upload_call_count());
  EXPECT_EQ(0, mock_service.multipart_upload_call_count());
  EXPECT_EQ(2 * 1024 * 1024, mock_service.received_bytes());
  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_TRUE(upload_location.is_empty());
  ASSERT_TRUE(entry);
  EXPECT_EQ(kTestDummyMd5, entry->md5_checksum());
  ASSERT_EQ(1U, upload_progress_values.size());
  EXPECT_EQ(test_util::ProgressInfo(2 * 1024 * 1024, 2 * 1024 * 1024),
            upload_progress_values[0]);
}

TEST_F(DriveUploaderTest, InitiateUploadFail) {
  base::FilePath local_path;
  std::string data;
  ASSERT_TRUE(test_util::CreateFileOfSpecifiedSize(
      temp_dir_.GetPath(), 2 * 1024 * 1024, &local_path, &data));

  ApiErrorCode error = HTTP_SUCCESS;
  GURL upload_location;
  std::unique_ptr<FileResource> entry;

  MockDriveServiceNoConnectionAtInitiate mock_service;
  DriveUploader uploader(
      &mock_service, base::SingleThreadTaskRunner::GetCurrentDefault().get(),
      mojo::NullRemote());
  uploader.UploadExistingFile(
      kTestInitiateUploadResourceId, local_path, kTestMimeType,
      UploadExistingFileOptions(),
      test_util::CreateCopyResultCallback(&error, &upload_location, &entry),
      google_apis::ProgressCallback());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(NO_CONNECTION, error);
  EXPECT_TRUE(upload_location.is_empty());
  EXPECT_FALSE(entry);
}

TEST_F(DriveUploaderTest, MultipartUploadFail) {
  base::FilePath local_path;
  std::string data;
  ASSERT_TRUE(test_util::CreateFileOfSpecifiedSize(
      temp_dir_.GetPath(), 512 * 1024, &local_path, &data));

  ApiErrorCode error = HTTP_SUCCESS;
  GURL upload_location;
  std::unique_ptr<FileResource> entry;

  MockDriveServiceNoConnectionAtInitiate mock_service;
  DriveUploader uploader(
      &mock_service, base::SingleThreadTaskRunner::GetCurrentDefault().get(),
      mojo::NullRemote());
  uploader.UploadExistingFile(
      kTestInitiateUploadResourceId, local_path, kTestMimeType,
      UploadExistingFileOptions(),
      test_util::CreateCopyResultCallback(&error, &upload_location, &entry),
      google_apis::ProgressCallback());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(NO_CONNECTION, error);
  EXPECT_TRUE(upload_location.is_empty());
  EXPECT_FALSE(entry);
}

TEST_F(DriveUploaderTest, InitiateUploadNoConflict) {
  base::FilePath local_path;
  std::string data;
  ASSERT_TRUE(test_util::CreateFileOfSpecifiedSize(
      temp_dir_.GetPath(), 512 * 1024, &local_path, &data));

  ApiErrorCode error = OTHER_ERROR;
  GURL upload_location;
  std::unique_ptr<FileResource> entry;

  MockDriveServiceWithUploadExpectation mock_service(local_path, data.size());
  DriveUploader uploader(
      &mock_service, base::SingleThreadTaskRunner::GetCurrentDefault().get(),
      mojo::NullRemote());
  UploadExistingFileOptions options;
  options.etag = kTestETag;
  uploader.UploadExistingFile(
      kTestInitiateUploadResourceId, local_path, kTestMimeType, options,
      test_util::CreateCopyResultCallback(&error, &upload_location, &entry),
      google_apis::ProgressCallback());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_TRUE(upload_location.is_empty());
}

TEST_F(DriveUploaderTest, MultipartUploadConflict) {
  base::FilePath local_path;
  std::string data;
  ASSERT_TRUE(test_util::CreateFileOfSpecifiedSize(
      temp_dir_.GetPath(), 512 * 1024, &local_path, &data));
  const std::string kDestinationETag("destination_etag");

  ApiErrorCode error = OTHER_ERROR;
  GURL upload_location;
  std::unique_ptr<FileResource> entry;

  MockDriveServiceWithUploadExpectation mock_service(local_path, data.size());
  DriveUploader uploader(
      &mock_service, base::SingleThreadTaskRunner::GetCurrentDefault().get(),
      mojo::NullRemote());
  UploadExistingFileOptions options;
  options.etag = kDestinationETag;
  uploader.UploadExistingFile(
      kTestInitiateUploadResourceId, local_path, kTestMimeType, options,
      test_util::CreateCopyResultCallback(&error, &upload_location, &entry),
      google_apis::ProgressCallback());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_CONFLICT, error);
  EXPECT_TRUE(upload_location.is_empty());
}

TEST_F(DriveUploaderTest, InitiateUploadConflict) {
  base::FilePath local_path;
  std::string data;
  ASSERT_TRUE(test_util::CreateFileOfSpecifiedSize(
      temp_dir_.GetPath(), 2 * 1024 * 1024, &local_path, &data));
  const std::string kDestinationETag("destination_etag");

  ApiErrorCode error = OTHER_ERROR;
  GURL upload_location;
  std::unique_ptr<FileResource> entry;

  MockDriveServiceWithUploadExpectation mock_service(local_path, data.size());
  DriveUploader uploader(
      &mock_service, base::SingleThreadTaskRunner::GetCurrentDefault().get(),
      mojo::NullRemote());
  UploadExistingFileOptions options;
  options.etag = kDestinationETag;
  uploader.UploadExistingFile(
      kTestInitiateUploadResourceId, local_path, kTestMimeType, options,
      test_util::CreateCopyResultCallback(&error, &upload_location, &entry),
      google_apis::ProgressCallback());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(HTTP_CONFLICT, error);
  EXPECT_TRUE(upload_location.is_empty());
}

TEST_F(DriveUploaderTest, ResumeUploadFail) {
  base::FilePath local_path;
  std::string data;
  ASSERT_TRUE(test_util::CreateFileOfSpecifiedSize(
      temp_dir_.GetPath(), 2 * 1024 * 1024, &local_path, &data));

  ApiErrorCode error = HTTP_SUCCESS;
  GURL upload_location;
  std::unique_ptr<FileResource> entry;

  MockDriveServiceNoConnectionAtResume mock_service;
  DriveUploader uploader(
      &mock_service, base::SingleThreadTaskRunner::GetCurrentDefault().get(),
      mojo::NullRemote());
  uploader.UploadExistingFile(
      kTestInitiateUploadResourceId, local_path, kTestMimeType,
      UploadExistingFileOptions(),
      test_util::CreateCopyResultCallback(&error, &upload_location, &entry),
      google_apis::ProgressCallback());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(NO_CONNECTION, error);
  EXPECT_EQ(GURL(kTestUploadExistingFileURL), upload_location);
}

TEST_F(DriveUploaderTest, GetUploadStatusFail) {
  base::FilePath local_path;
  std::string data;
  ASSERT_TRUE(test_util::CreateFileOfSpecifiedSize(
      temp_dir_.GetPath(), 2 * 1024 * 1024, &local_path, &data));

  ApiErrorCode error = HTTP_SUCCESS;
  GURL upload_location;
  std::unique_ptr<FileResource> entry;

  MockDriveServiceNoConnectionAtGetUploadStatus mock_service;
  DriveUploader uploader(
      &mock_service, base::SingleThreadTaskRunner::GetCurrentDefault().get(),
      mojo::NullRemote());
  uploader.ResumeUploadFile(
      GURL(kTestUploadExistingFileURL), local_path, kTestMimeType,
      test_util::CreateCopyResultCallback(&error, &upload_location, &entry),
      google_apis::ProgressCallback());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(NO_CONNECTION, error);
  EXPECT_TRUE(upload_location.is_empty());
}

TEST_F(DriveUploaderTest, NonExistingSourceFile) {
  ApiErrorCode error = OTHER_ERROR;
  GURL upload_location;
  std::unique_ptr<FileResource> entry;

  DriveUploader uploader(
      nullptr,  // nullptr, the service won't be used.
      base::SingleThreadTaskRunner::GetCurrentDefault().get(),
      mojo::NullRemote());
  uploader.UploadExistingFile(
      kTestInitiateUploadResourceId,
      temp_dir_.GetPath().AppendASCII("_this_path_should_not_exist_"),
      kTestMimeType, UploadExistingFileOptions(),
      test_util::CreateCopyResultCallback(&error, &upload_location, &entry),
      google_apis::ProgressCallback());
  base::RunLoop().RunUntilIdle();

  // Should return failure without doing any attempt to connect to the server.
  EXPECT_EQ(HTTP_NOT_FOUND, error);
  EXPECT_TRUE(upload_location.is_empty());
}

TEST_F(DriveUploaderTest, ResumeUpload) {
  base::FilePath local_path;
  std::string data;
  ASSERT_TRUE(test_util::CreateFileOfSpecifiedSize(
      temp_dir_.GetPath(), 1024 * 1024, &local_path, &data));

  ApiErrorCode error = OTHER_ERROR;
  GURL upload_location;
  std::unique_ptr<FileResource> entry;

  MockDriveServiceWithUploadExpectation mock_service(local_path, data.size());
  DriveUploader uploader(
      &mock_service, base::SingleThreadTaskRunner::GetCurrentDefault().get(),
      mojo::NullRemote());
  // Emulate the situation that the only first part is successfully uploaded,
  // but not the latter half.
  mock_service.set_received_bytes(512 * 1024);

  std::vector<test_util::ProgressInfo> upload_progress_values;
  uploader.ResumeUploadFile(
      GURL(kTestUploadExistingFileURL), local_path, kTestMimeType,
      test_util::CreateCopyResultCallback(&error, &upload_location, &entry),
      base::BindRepeating(&test_util::AppendProgressCallbackResult,
                          &upload_progress_values));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, mock_service.resume_upload_call_count());
  EXPECT_EQ(1024 * 1024, mock_service.received_bytes());
  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_TRUE(upload_location.is_empty());
  ASSERT_TRUE(entry);
  EXPECT_EQ(kTestDummyMd5, entry->md5_checksum());
  ASSERT_EQ(1U, upload_progress_values.size());
  EXPECT_EQ(test_util::ProgressInfo(1024 * 1024, 1024 * 1024),
            upload_progress_values[0]);
}

class MockDriveServiceForBatchProcessing : public DummyDriveService {
 public:
  struct UploadFileInfo {
    enum { NEW_FILE, EXISTING_FILE } type;
    std::string content_type;
    std::optional<std::string> converted_mime_type;
    uint64_t content_length;
    std::string parent_resource_id;
    std::string resource_id;
    std::string title;
    base::FilePath local_file_path;
    google_apis::FileResourceCallback callback;
    google_apis::ProgressCallback progress_callback;
  };

  class BatchRequestConfigurator : public BatchRequestConfiguratorInterface {
   public:
    explicit BatchRequestConfigurator(
        MockDriveServiceForBatchProcessing* service)
        : service(service) {}

    CancelCallbackOnce MultipartUploadNewFile(
        const std::string& content_type,
        std::optional<std::string_view> converted_mime_type,
        int64_t content_length,
        const std::string& parent_resource_id,
        const std::string& title,
        const base::FilePath& local_file_path,
        const UploadNewFileOptions& options,
        google_apis::FileResourceCallback callback,
        google_apis::ProgressCallback progress_callback) override {
      UploadFileInfo info;
      info.type = UploadFileInfo::NEW_FILE;
      info.content_type = content_type;
      info.converted_mime_type = converted_mime_type;
      info.content_length = content_length;
      info.parent_resource_id = parent_resource_id;
      info.title = title;
      info.local_file_path = local_file_path;
      info.callback = std::move(callback);
      info.progress_callback = progress_callback;
      service->files.push_back(std::move(info));
      return CancelCallbackOnce();
    }

    CancelCallbackOnce MultipartUploadExistingFile(
        const std::string& content_type,
        int64_t content_length,
        const std::string& resource_id,
        const base::FilePath& local_file_path,
        const UploadExistingFileOptions& options,
        google_apis::FileResourceCallback callback,
        google_apis::ProgressCallback progress_callback) override {
      UploadFileInfo info;
      info.type = UploadFileInfo::EXISTING_FILE;
      info.content_type = content_type;
      info.content_length = content_length;
      info.resource_id = resource_id;
      info.local_file_path = local_file_path;
      info.callback = std::move(callback);
      info.progress_callback = progress_callback;
      service->files.push_back(std::move(info));
      return CancelCallbackOnce();
    }

    void Commit() override {
      ASSERT_FALSE(service->committed);
      service->committed = true;
      for (auto& file : service->files) {
        SendMultipartUploadResult(HTTP_SUCCESS, file.content_length,
                                  std::move(file.callback),
                                  file.progress_callback);
      }
    }

   private:
    raw_ptr<MockDriveServiceForBatchProcessing> service;
  };

 public:
  std::unique_ptr<BatchRequestConfiguratorInterface> StartBatchRequest()
      override {
    committed = false;
    return std::unique_ptr<BatchRequestConfiguratorInterface>(
        new BatchRequestConfigurator(this));
  }

  std::vector<UploadFileInfo> files;
  bool committed;
};

TEST_F(DriveUploaderTest, BatchProcessing) {
  // Preapre test file.
  const size_t kTestFileSize = 1024 * 512;
  base::FilePath local_path;
  std::string data;
  ASSERT_TRUE(test_util::CreateFileOfSpecifiedSize(
      temp_dir_.GetPath(), kTestFileSize, &local_path, &data));

  // Prepare test target.
  MockDriveServiceForBatchProcessing service;
  DriveUploader uploader(
      &service, base::SingleThreadTaskRunner::GetCurrentDefault().get(),
      mojo::NullRemote());

  struct {
    ApiErrorCode error;
    GURL resume_url;
    std::unique_ptr<FileResource> file;
    UploadCompletionCallback callback() {
      return test_util::CreateCopyResultCallback(&error, &resume_url, &file);
    }
  } results[2];

  uploader.StartBatchProcessing();
  uploader.UploadNewFile("parent_resource_id", local_path, "title",
                         kTestMimeType, UploadNewFileOptions(),
                         results[0].callback(),
                         google_apis::ProgressCallback());
  uploader.UploadExistingFile(
      "resource_id", local_path, kTestMimeType, UploadExistingFileOptions(),
      results[1].callback(), google_apis::ProgressCallback());
  uploader.StopBatchProcessing();
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(2u, service.files.size());
  EXPECT_TRUE(service.committed);

  EXPECT_EQ(MockDriveServiceForBatchProcessing::UploadFileInfo::NEW_FILE,
            service.files[0].type);
  EXPECT_EQ(kTestMimeType, service.files[0].content_type);
  EXPECT_EQ(kTestFileSize, service.files[0].content_length);
  EXPECT_EQ("parent_resource_id", service.files[0].parent_resource_id);
  EXPECT_EQ("", service.files[0].resource_id);
  EXPECT_EQ("title", service.files[0].title);
  EXPECT_EQ(local_path.value(), service.files[0].local_file_path.value());

  EXPECT_EQ(MockDriveServiceForBatchProcessing::UploadFileInfo::EXISTING_FILE,
            service.files[1].type);
  EXPECT_EQ(kTestMimeType, service.files[1].content_type);
  EXPECT_EQ(kTestFileSize, service.files[1].content_length);
  EXPECT_EQ("", service.files[1].parent_resource_id);
  EXPECT_EQ("resource_id", service.files[1].resource_id);
  EXPECT_EQ("", service.files[1].title);
  EXPECT_EQ(local_path.value(), service.files[1].local_file_path.value());

  EXPECT_EQ(HTTP_SUCCESS, results[0].error);
  EXPECT_TRUE(results[0].resume_url.is_empty());
  EXPECT_TRUE(results[0].file);

  EXPECT_EQ(HTTP_SUCCESS, results[1].error);
  EXPECT_TRUE(results[1].resume_url.is_empty());
  EXPECT_TRUE(results[1].file);
}

TEST_F(DriveUploaderTest, BatchProcessingWithError) {
  // Prepare test target.
  MockDriveServiceForBatchProcessing service;
  DriveUploader uploader(
      &service, base::SingleThreadTaskRunner::GetCurrentDefault().get(),
      mojo::NullRemote());

  struct {
    ApiErrorCode error;
    GURL resume_url;
    std::unique_ptr<FileResource> file;
    UploadCompletionCallback callback() {
      return test_util::CreateCopyResultCallback(&error, &resume_url, &file);
    }
  } results[2];

  uploader.StartBatchProcessing();
  uploader.UploadNewFile("parent_resource_id",
                         base::FilePath(FILE_PATH_LITERAL("/path/non_exists")),
                         "title", kTestMimeType, UploadNewFileOptions(),
                         results[0].callback(),
                         google_apis::ProgressCallback());
  uploader.UploadExistingFile(
      "resource_id", base::FilePath(FILE_PATH_LITERAL("/path/non_exists")),
      kTestMimeType, UploadExistingFileOptions(), results[1].callback(),
      google_apis::ProgressCallback());
  uploader.StopBatchProcessing();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, service.files.size());
  EXPECT_TRUE(service.committed);

  EXPECT_EQ(HTTP_NOT_FOUND, results[0].error);
  EXPECT_TRUE(results[0].resume_url.is_empty());
  EXPECT_FALSE(results[0].file);

  EXPECT_EQ(HTTP_NOT_FOUND, results[1].error);
  EXPECT_TRUE(results[1].resume_url.is_empty());
  EXPECT_FALSE(results[1].file);
}
}  // namespace drive
