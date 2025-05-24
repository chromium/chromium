// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/drive/service/drive_api_service.h"

#include <optional>
#include <utility>

#include "base/test/test_simple_task_runner.h"
#include "google_apis/common/dummy_auth_service.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/common/test_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace {
const char kTestUserAgent[] = "test-user-agent";
}

class TestAuthService : public google_apis::DummyAuthService {
 public:
  void StartAuthentication(google_apis::AuthStatusCallback callback) override {
    callback_ = std::move(callback);
  }

  bool HasAccessToken() const override { return false; }

  void SendHttpError() {
    ASSERT_FALSE(callback_.is_null());
    std::move(callback_).Run(google_apis::HTTP_UNAUTHORIZED, "");
  }

 private:
  google_apis::AuthStatusCallback callback_;
};

TEST(DriveAPIServiceTest, BatchRequestConfiguratorWithAuthFailure) {
  const GURL test_base_url("http://localhost/");
  google_apis::DriveApiUrlGenerator url_generator(test_base_url, test_base_url);
  scoped_refptr<base::TestSimpleTaskRunner> task_runner =
      new base::TestSimpleTaskRunner();
  network::TestURLLoaderFactory test_url_loader_factory;
  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
      test_shared_loader_factory =
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              &test_url_loader_factory);
  google_apis::RequestSender sender(
      std::make_unique<TestAuthService>(), test_shared_loader_factory,
      task_runner.get(), kTestUserAgent, TRAFFIC_ANNOTATION_FOR_TESTS);
  std::unique_ptr<google_apis::drive::BatchUploadRequest> request =
      std::make_unique<google_apis::drive::BatchUploadRequest>(&sender,
                                                               url_generator);
  google_apis::drive::BatchUploadRequest* request_ptr = request.get();
  sender.StartRequestWithAuthRetry(std::move(request));
  BatchRequestConfigurator configurator(
      request_ptr->GetWeakPtrAsBatchUploadRequest(), task_runner.get(),
      url_generator, google_apis::CancelCallbackRepeating());
  static_cast<TestAuthService*>(sender.auth_service())->SendHttpError();

  {
    google_apis::ApiErrorCode error = google_apis::HTTP_SUCCESS;
    std::unique_ptr<google_apis::FileResource> file_resource;
    configurator.MultipartUploadNewFile(
        "text/plain", /*converted_mime_type=*/std::nullopt, 10, "", "title",
        base::FilePath(FILE_PATH_LITERAL("/file")), UploadNewFileOptions(),
        google_apis::test_util::CreateCopyResultCallback(&error,
                                                         &file_resource),
        google_apis::ProgressCallback());
    EXPECT_EQ(google_apis::OTHER_ERROR, error);
  }
  {
    google_apis::ApiErrorCode error = google_apis::HTTP_SUCCESS;
    std::unique_ptr<google_apis::FileResource> file_resource;
    configurator.MultipartUploadExistingFile(
        "text/plain", 10, "resource_id",
        base::FilePath(FILE_PATH_LITERAL("/file")), UploadExistingFileOptions(),
        google_apis::test_util::CreateCopyResultCallback(&error,
                                                         &file_resource),
        google_apis::ProgressCallback());
    EXPECT_EQ(google_apis::OTHER_ERROR, error);
  }
}

}  // namespace drive
