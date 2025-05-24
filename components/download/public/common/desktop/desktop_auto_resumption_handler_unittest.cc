// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/desktop/desktop_auto_resumption_handler.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/download/public/common/download_features.h"
#include "components/download/public/common/mock_download_item.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;

namespace download {

class DesktopAutoResumptionHandlerTest : public testing::Test {
 public:
  DesktopAutoResumptionHandlerTest()
      : task_environment_(
            base::test::TaskEnvironment::MainThreadType::UI,
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED,
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  DesktopAutoResumptionHandlerTest(const DesktopAutoResumptionHandlerTest&) =
      delete;
  DesktopAutoResumptionHandlerTest& operator=(
      const DesktopAutoResumptionHandlerTest&) = delete;

  ~DesktopAutoResumptionHandlerTest() override = default;

 protected:
  void SetUp() override {
    handler_ = std::make_unique<DesktopAutoResumptionHandler>();
    scoped_feature_list_.InitAndEnableFeature(features::kBackoffInDownloading);
  }

  void TearDown() override {}

  std::unique_ptr<DesktopAutoResumptionHandler> handler_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test IsAutoResumableDownload
TEST_F(DesktopAutoResumptionHandlerTest, IsAutoResumableDownload) {
  auto item = std::make_unique<NiceMock<MockDownloadItem>>();

  // Null check
  EXPECT_FALSE(handler_->IsAutoResumableDownload(nullptr));

  // IN_PROGRESS state, not paused
  EXPECT_CALL(*item, GetState()).WillOnce(Return(DownloadItem::IN_PROGRESS));
  EXPECT_CALL(*item, IsPaused()).WillOnce(Return(false));
  EXPECT_TRUE(handler_->IsAutoResumableDownload(item.get()));

  // IN_PROGRESS state, paused
  EXPECT_CALL(*item, GetState()).WillOnce(Return(DownloadItem::IN_PROGRESS));
  EXPECT_CALL(*item, IsPaused()).WillOnce(Return(true));
  EXPECT_FALSE(handler_->IsAutoResumableDownload(item.get()));

  // INTERRUPTED state, not paused
  EXPECT_CALL(*item, GetState()).WillOnce(Return(DownloadItem::INTERRUPTED));
  EXPECT_CALL(*item, IsPaused()).WillOnce(Return(false));
  EXPECT_TRUE(handler_->IsAutoResumableDownload(item.get()));

  // COMPLETE state
  EXPECT_CALL(*item, GetState()).WillOnce(Return(DownloadItem::COMPLETE));
  EXPECT_FALSE(handler_->IsAutoResumableDownload(item.get()));
}

// Test OnDownloadUpdated
TEST_F(DesktopAutoResumptionHandlerTest, DownloadHandling) {
  auto item = std::make_unique<NiceMock<MockDownloadItem>>();

  // Test OnDownloadUpdated with interrupted download
  ON_CALL(*item, GetState()).WillByDefault(Return(DownloadItem::INTERRUPTED));
  ON_CALL(*item, GetAutoResumeCount()).WillByDefault(Return(2));
  ON_CALL(*item, IsPaused()).WillByDefault(Return(false));
  static const std::string guid = "test_guid";
  ON_CALL(*item, GetGuid()).WillByDefault(ReturnRef(guid));
  handler_->OnDownloadUpdated(item.get());

  EXPECT_CALL(*item, Resume(false));
  // Fast forward time and check if resume is called
  task_environment_.FastForwardBy(base::Minutes(1));
}

// Test we won't call Resume if download is destroyed.
TEST_F(DesktopAutoResumptionHandlerTest, DownloadDestroyedWithReset) {
  auto item = std::make_unique<NiceMock<MockDownloadItem>>();
  item->AddObserver(handler_.get());

  ON_CALL(*item, GetState()).WillByDefault(Return(DownloadItem::INTERRUPTED));
  ON_CALL(*item, GetAutoResumeCount()).WillByDefault(Return(2));
  ON_CALL(*item, IsPaused()).WillByDefault(Return(false));
  static const std::string guid = "test_guid";
  ON_CALL(*item, GetGuid()).WillByDefault(ReturnRef(guid));
  // Verify Resume is not called with destroyed item
  EXPECT_CALL(*item, Resume(false)).Times(0);

  handler_->OnDownloadUpdated(item.get());
  // Reset item to simulate destruction
  item.reset();
  task_environment_.FastForwardBy(base::Minutes(1));
}

}  // namespace download
