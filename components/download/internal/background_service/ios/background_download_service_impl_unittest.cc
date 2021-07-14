// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/ios/background_download_service_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/download/internal/background_service/client_set.h"
#include "components/download/internal/background_service/ios/background_download_task_helper.h"
#include "components/download/internal/background_service/test/test_store.h"
#include "components/download/public/background_service/test/mock_client.h"
#include "testing/platform_test.h"

using ::base::test::RunCallback;
using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::NiceMock;
using ServiceStatus = download::BackgroundDownloadService::ServiceStatus;
using StartResult = download::DownloadParams::StartResult;

const char kURL[] = "https://www.example.com/test";
const char kGuid[] = "1234";
const base::FilePath::CharType kFilePath[] =
    FILE_PATH_LITERAL("downloaded_file.zip");

namespace download {
namespace {

MATCHER_P(CompletionInfoIs, file_path, "") {
  return arg.path == file_path;
}

class MockBackgroundDownloadTaskHelper : public BackgroundDownloadTaskHelper {
 public:
  MockBackgroundDownloadTaskHelper() = default;
  ~MockBackgroundDownloadTaskHelper() override = default;
  MOCK_METHOD(void,
              StartDownload,
              (const std::string& guid,
               const RequestParams&,
               const SchedulingParams&,
               CompletionCallback,
               UpdateCallback),
              (override));
};

// Test fixture for BackgroundDownloadServiceImpl.
class BackgroundDownloadServiceImplTest : public PlatformTest {
 protected:
  BackgroundDownloadServiceImplTest() {}
  ~BackgroundDownloadServiceImplTest() override = default;

  void SetUp() override {
    auto store = std::make_unique<test::TestStore>();
    store_ = store.get();
    auto model = std::make_unique<ModelImpl>(std::move(store));
    auto client = std::make_unique<NiceMock<test::MockClient>>();
    client_ = client.get();
    auto clients = std::make_unique<DownloadClientMap>();
    clients->insert(std::make_pair(DownloadClient::TEST, std::move(client)));
    auto client_set = std::make_unique<ClientSet>(std::move(clients));
    auto download_helper = std::make_unique<MockBackgroundDownloadTaskHelper>();
    download_helper_ = download_helper.get();
    service_ = std::make_unique<BackgroundDownloadServiceImpl>(
        std::move(client_set), std::move(model), std::move(download_helper));
  }

  BackgroundDownloadService* service() { return service_.get(); }
  std::unique_ptr<std::vector<Entry>> empty_entries() {
    return std::make_unique<std::vector<Entry>>();
  }
  DownloadParams CreateDownloadParams(const std::string& url) {
    DownloadParams download_params;
    download_params.client = DownloadClient::TEST;
    download_params.guid = kGuid;
    download_params.callback = start_callback_.Get();
    download_params.request_params.url = GURL(url);
    return download_params;
  }

  base::test::TaskEnvironment task_environment_;
  MockBackgroundDownloadTaskHelper* download_helper_;
  test::TestStore* store_;
  test::MockClient* client_;
  base::MockCallback<DownloadParams::StartCallback> start_callback_;

 private:
  std::unique_ptr<BackgroundDownloadServiceImpl> service_;
};

TEST_F(BackgroundDownloadServiceImplTest, InitSuccess) {
  EXPECT_EQ(ServiceStatus::STARTING_UP, service()->GetStatus());
  EXPECT_CALL(*client_, OnServiceInitialized(false, _));
  store_->TriggerInit(/*success=*/true, empty_entries());
  EXPECT_EQ(ServiceStatus::READY, service()->GetStatus());
}

TEST_F(BackgroundDownloadServiceImplTest, InitFailure) {
  EXPECT_EQ(ServiceStatus::STARTING_UP, service()->GetStatus());
  EXPECT_CALL(*client_, OnServiceUnavailable());
  store_->TriggerInit(/*success=*/false, empty_entries());
  EXPECT_EQ(ServiceStatus::UNAVAILABLE, service()->GetStatus());
}

TEST_F(BackgroundDownloadServiceImplTest, StartDownloadDbFailure) {
  store_->TriggerInit(/*success=*/true, empty_entries());
  EXPECT_CALL(start_callback_, Run(kGuid, StartResult::INTERNAL_ERROR));
  auto download_params = CreateDownloadParams(kURL);
  service()->StartDownload(std::move(download_params));
  store_->TriggerUpdate(/*success=*/false);
  EXPECT_EQ(kGuid, store_->LastUpdatedEntry()->guid);
  task_environment_.RunUntilIdle();
}

TEST_F(BackgroundDownloadServiceImplTest, StartDownloadHelperFailure) {
  store_->TriggerInit(/*success=*/true, empty_entries());
  EXPECT_CALL(start_callback_, Run(kGuid, StartResult::ACCEPTED));
  EXPECT_CALL(*download_helper_, StartDownload(_, _, _, _, _))
      .WillOnce(RunOnceCallback<3>(/*success=*/false, base::FilePath()));
  EXPECT_CALL(*client_,
              OnDownloadFailed(kGuid, CompletionInfoIs(base::FilePath()),
                               download::Client::FailureReason::UNKNOWN));
  auto download_params = CreateDownloadParams(kURL);
  service()->StartDownload(std::move(download_params));
  store_->TriggerUpdate(/*success=*/true);
  EXPECT_EQ(kGuid, store_->LastRemovedEntry());
  task_environment_.RunUntilIdle();
}

TEST_F(BackgroundDownloadServiceImplTest, StartDownloadSuccess) {
  store_->TriggerInit(/*success=*/true, empty_entries());
  EXPECT_CALL(start_callback_, Run(kGuid, StartResult::ACCEPTED));
  EXPECT_CALL(*download_helper_, StartDownload(_, _, _, _, _))
      .WillOnce(
          RunOnceCallback<3>(/*success=*/true, base::FilePath(kFilePath)));
  EXPECT_CALL(
      *client_,
      OnDownloadSucceeded(kGuid, CompletionInfoIs(base::FilePath(kFilePath))));
  auto download_params = CreateDownloadParams(kURL);
  service()->StartDownload(std::move(download_params));
  store_->TriggerUpdate(/*success=*/true);
  EXPECT_EQ(kGuid, store_->LastUpdatedEntry()->guid);
  task_environment_.RunUntilIdle();
}

// Verifies Client::OnDownloadUpdated() is called.
TEST_F(BackgroundDownloadServiceImplTest, OnDownloadUpdated) {
  store_->TriggerInit(/*success=*/true, empty_entries());
  EXPECT_CALL(start_callback_, Run(kGuid, StartResult::ACCEPTED));
  EXPECT_CALL(*download_helper_, StartDownload(_, _, _, _, _))
      .WillOnce(RunCallback<4>(10u));
  EXPECT_CALL(*client_, OnDownloadUpdated(kGuid, 0u, 10u));
  auto download_params = CreateDownloadParams(kURL);
  service()->StartDownload(std::move(download_params));
  store_->TriggerUpdate(/*success=*/true);
  EXPECT_EQ(kGuid, store_->LastUpdatedEntry()->guid);
  task_environment_.RunUntilIdle();
}

}  // namespace
}  // namespace download
