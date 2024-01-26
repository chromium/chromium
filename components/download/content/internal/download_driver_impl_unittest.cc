// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/content/internal/download_driver_impl.h"

#include <memory>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "base/uuid.h"
#include "components/download/content/public/all_download_item_notifier.h"
#include "components/download/internal/background_service/test/mock_download_driver_client.h"
#include "components/download/public/common/download_features.h"
#include "components/download/public/common/mock_simple_download_manager.h"
#include "content/public/test/fake_download_item.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;

namespace download {

namespace {

ACTION_P(PopulateVector, items) {
  arg0->insert(arg0->begin(), items.begin(), items.end());
}

const char kFakeGuid[] = "fake_guid";
const char kFakeURL[] = "https://www.example.com/kale";

// Matcher to compare driver entries. Not all the memeber fields are compared.
// Currently no comparison in non test code, so no operator== override for
// driver entry.
MATCHER_P(DriverEntryEqual, entry, "") {
  return entry.guid == arg.guid && entry.state == arg.state &&
         entry.done == arg.done && entry.can_resume == arg.can_resume &&
         entry.bytes_downloaded == arg.bytes_downloaded &&
         entry.expected_total_size == arg.expected_total_size &&
         entry.current_file_path.value() == arg.current_file_path.value() &&
         entry.completion_time == arg.completion_time &&
         entry.hash256 == arg.hash256;
}

}  // namespace

class DownloadDriverImplTest : public testing::Test {
 public:
  DownloadDriverImplTest()
      : coordinator_(base::NullCallback()),
        task_runner_(new base::TestSimpleTaskRunner),
        current_default_handle_(task_runner_) {}

  DownloadDriverImplTest(const DownloadDriverImplTest&) = delete;
  DownloadDriverImplTest& operator=(const DownloadDriverImplTest&) = delete;

  ~DownloadDriverImplTest() override = default;

  void SetUp() override {
    EXPECT_CALL(mock_client_, IsTrackingDownload(_))
        .WillRepeatedly(Return(true));
    driver_ = std::make_unique<DownloadDriverImpl>(&coordinator_);
    coordinator_.SetSimpleDownloadManager(&mock_manager_, true);
    ON_CALL(mock_manager_, CanDownload(_)).WillByDefault(Return(true));
  }

 protected:
  SimpleDownloadManagerCoordinator coordinator_;
  NiceMock<MockSimpleDownloadManager> mock_manager_;
  MockDriverClient mock_client_;
  std::unique_ptr<DownloadDriverImpl> driver_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::SingleThreadTaskRunner::CurrentDefaultHandle current_default_handle_;
};

// Ensure the download manager can be initialized after the download driver.
TEST_F(DownloadDriverImplTest, ManagerLateInitialization) {
  driver_->Initialize(&mock_client_);

  EXPECT_CALL(mock_client_, OnDriverReady(true)).Times(1);
  static_cast<AllDownloadEventNotifier::Observer*>(driver_.get())
      ->OnDownloadsInitialized(&coordinator_, true);

  EXPECT_CALL(mock_client_, OnDriverReady(true)).Times(0);
  static_cast<AllDownloadEventNotifier::Observer*>(driver_.get())
      ->OnDownloadsInitialized(&coordinator_, false);
}

TEST_F(DownloadDriverImplTest, TestHardRecover) {
  driver_->Initialize(&mock_client_);

  EXPECT_CALL(mock_client_, OnDriverHardRecoverComplete(true)).Times(1);
  driver_->HardRecover();
  task_runner_->RunUntilIdle();
}
// Ensure driver remove call before download created will result in content
// layer remove call and not propagating the event to driver's client.
TEST_F(DownloadDriverImplTest, RemoveBeforeCreated) {
  using DownloadState = download::DownloadItem::DownloadState;

  driver_->Initialize(&mock_client_);

  EXPECT_CALL(mock_client_, OnDriverReady(true));
  mock_manager_.NotifyOnDownloadInitialized();

  const std::string kTestGuid = "abc";
  content::FakeDownloadItem fake_item;
  fake_item.SetGuid(kTestGuid);
  fake_item.SetState(DownloadState::IN_PROGRESS);

  // Download is not created yet in content layer.
  ON_CALL(mock_manager_, GetDownloadByGuid(kTestGuid))
      .WillByDefault(Return(nullptr));
  driver_->Remove(kTestGuid, false);
  task_runner_->RunUntilIdle();

  // Download is created in content layer.
  ON_CALL(mock_manager_, GetDownloadByGuid(kTestGuid))
      .WillByDefault(Return(&fake_item));
  EXPECT_CALL(mock_client_, OnDownloadCreated(_)).Times(0);
  static_cast<AllDownloadEventNotifier::Observer*>(driver_.get())
      ->OnDownloadCreated(&coordinator_, &fake_item);
  task_runner_->RunUntilIdle();

  // Expect a remove call down to content layer download item.
  EXPECT_TRUE(fake_item.removed());
}

// Ensures download updates from download items are propagated correctly.
TEST_F(DownloadDriverImplTest, DownloadItemUpdateEvents) {
  using DownloadState = download::DownloadItem::DownloadState;
  using DownloadInterruptReason = download::DownloadInterruptReason;

  mock_manager_.NotifyOnDownloadInitialized();
  EXPECT_CALL(mock_client_, OnDriverReady(true)).Times(1);
  driver_->Initialize(&mock_client_);

  content::FakeDownloadItem fake_item;
  fake_item.SetState(DownloadState::IN_PROGRESS);
  fake_item.SetGuid(kFakeGuid);
  fake_item.SetReceivedBytes(0);
  fake_item.SetTotalBytes(1024);
  DriverEntry entry = DownloadDriverImpl::CreateDriverEntry(&fake_item);

  EXPECT_CALL(mock_client_, OnDownloadUpdated(DriverEntryEqual(entry)))
      .Times(1)
      .RetiresOnSaturation();
  static_cast<AllDownloadEventNotifier::Observer*>(driver_.get())
      ->OnDownloadUpdated(&coordinator_, &fake_item);

  // Nothing happens for cancelled state.
  fake_item.SetState(DownloadState::CANCELLED);
  static_cast<AllDownloadEventNotifier::Observer*>(driver_.get())
      ->OnDownloadUpdated(&coordinator_, &fake_item);

  fake_item.SetReceivedBytes(1024);
  fake_item.SetState(DownloadState::COMPLETE);
  fake_item.SetHash("01234567ABCDEF");
  entry = DownloadDriverImpl::CreateDriverEntry(&fake_item);
  EXPECT_CALL(mock_client_, OnDownloadSucceeded(DriverEntryEqual(entry)))
      .Times(1)
      .RetiresOnSaturation();
  static_cast<AllDownloadEventNotifier::Observer*>(driver_.get())
      ->OnDownloadUpdated(&coordinator_, &fake_item);

  fake_item.SetState(DownloadState::INTERRUPTED);
  fake_item.SetLastReason(
      DownloadInterruptReason::DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT);
  entry = DownloadDriverImpl::CreateDriverEntry(&fake_item);
  EXPECT_CALL(mock_client_, OnDownloadFailed(DriverEntryEqual(entry),
                                             FailureType::RECOVERABLE))
      .Times(1)
      .RetiresOnSaturation();
  static_cast<AllDownloadEventNotifier::Observer*>(driver_.get())
      ->OnDownloadUpdated(&coordinator_, &fake_item);
}

TEST_F(DownloadDriverImplTest, TestGetActiveDownloadsCall) {
  using DownloadState = download::DownloadItem::DownloadState;
  content::FakeDownloadItem item1;
  item1.SetState(DownloadState::IN_PROGRESS);
  item1.SetGuid(base::Uuid::GenerateRandomV4().AsLowercaseString());

  content::FakeDownloadItem item2;
  item2.SetState(DownloadState::CANCELLED);
  item2.SetGuid(base::Uuid::GenerateRandomV4().AsLowercaseString());

  content::FakeDownloadItem item3;
  item3.SetState(DownloadState::COMPLETE);
  item3.SetGuid(base::Uuid::GenerateRandomV4().AsLowercaseString());

  content::FakeDownloadItem item4;
  item4.SetState(DownloadState::INTERRUPTED);
  item4.SetGuid(base::Uuid::GenerateRandomV4().AsLowercaseString());

  std::vector<download::DownloadItem*> items{&item1, &item2, &item3, &item4};

  ON_CALL(mock_manager_, GetAllDownloads(_))
      .WillByDefault(PopulateVector(items));

  mock_manager_.NotifyOnDownloadInitialized();
  EXPECT_CALL(mock_client_, OnDriverReady(true)).Times(1);
  driver_->Initialize(&mock_client_);

  auto guids = driver_->GetActiveDownloads();

  EXPECT_EQ(1U, guids.size());
  EXPECT_NE(guids.end(), guids.find(item1.GetGuid()));
}

TEST_F(DownloadDriverImplTest, TestCreateDriverEntry) {
  using DownloadState = download::DownloadItem::DownloadState;
  content::FakeDownloadItem item;
  const std::string kGuid("dummy guid");
  const std::vector<GURL> kUrls = {GURL("http://www.example.com/foo.html"),
                                   GURL("http://www.example.com/bar.html")};
  scoped_refptr<net::HttpResponseHeaders> headers =
      new net::HttpResponseHeaders("HTTP/1.1 201\n");

  item.SetGuid(kGuid);
  item.SetUrlChain(kUrls);
  item.SetState(DownloadState::IN_PROGRESS);
  item.SetResponseHeaders(headers);

  DriverEntry entry = driver_->CreateDriverEntry(&item);

  EXPECT_EQ(kGuid, entry.guid);
  EXPECT_EQ(kUrls, entry.url_chain);
  EXPECT_EQ(DriverEntry::State::IN_PROGRESS, entry.state);
  EXPECT_EQ(headers, entry.response_headers);
}

bool HasHeader(const DownloadUrlParameters::RequestHeadersType& headers,
               const std::string& header) {
  for (const auto& pair : headers) {
    if (pair.first == header)
      return true;
  }
  return false;
}

// Range header set in RequestParams will be set correctly in
// DownloadUrlParameters when calling |DownloadDriver::Start|.
TEST_F(DownloadDriverImplTest, Start_WithRangeHeader) {
  RequestParams request_params;
  request_params.url = GURL(kFakeURL);
  request_params.request_headers.AddHeaderFromString("Range: bytes=5-10");
  EXPECT_CALL(mock_manager_, DownloadUrlMock(_)).RetiresOnSaturation();
  driver_->Start(request_params, kFakeGuid, base::FilePath(), nullptr,
                 TRAFFIC_ANNOTATION_FOR_TESTS);
  const auto* download_url_parameters =
      mock_manager_.GetDownloadUrlParameters();
  DCHECK(download_url_parameters);
  EXPECT_FALSE(download_url_parameters->use_if_range());
  auto range_request_offset = download_url_parameters->range_request_offset();
  EXPECT_EQ(5, range_request_offset.first);
  EXPECT_EQ(10, range_request_offset.second);
  EXPECT_FALSE(HasHeader(download_url_parameters->request_headers(),
                         net::HttpRequestHeaders::kRange));
  EXPECT_FALSE(HasHeader(download_url_parameters->request_headers(),
                         net::HttpRequestHeaders::kIfRange));

  request_params.request_headers.AddHeaderFromString("Range: bytes=-10");
  EXPECT_CALL(mock_manager_, DownloadUrlMock(_)).RetiresOnSaturation();
  driver_->Start(request_params, kFakeGuid, base::FilePath(), nullptr,
                 TRAFFIC_ANNOTATION_FOR_TESTS);
  download_url_parameters = mock_manager_.GetDownloadUrlParameters();
  DCHECK(download_url_parameters);
  EXPECT_FALSE(download_url_parameters->use_if_range());
  range_request_offset = download_url_parameters->range_request_offset();
  EXPECT_EQ(kInvalidRange, range_request_offset.first);
  EXPECT_EQ(10, range_request_offset.second);
  EXPECT_FALSE(HasHeader(download_url_parameters->request_headers(),
                         net::HttpRequestHeaders::kRange));
  EXPECT_FALSE(HasHeader(download_url_parameters->request_headers(),
                         net::HttpRequestHeaders::kIfRange));
}

}  // namespace download
