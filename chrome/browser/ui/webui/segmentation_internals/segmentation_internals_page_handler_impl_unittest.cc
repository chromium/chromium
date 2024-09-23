// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/segmentation_internals/segmentation_internals_page_handler_impl.h"

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "components/segmentation_platform/public/testing/mock_segmentation_platform_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::Return;

class MockPage : public segmentation_internals::mojom::Page {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<segmentation_internals::mojom::Page> BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

  MOCK_METHOD(void, OnServiceStatusChanged, (bool, int32_t));
  MOCK_METHOD(
      void,
      OnClientInfoAvailable,
      (std::vector<segmentation_internals::mojom::ClientInfoPtr> client_info));

  mojo::Receiver<segmentation_internals::mojom::Page> receiver_{this};
};

constexpr char kTestKey1[] = "test_key1";
constexpr char kTestKey2[] = "test_key2";

std::vector<segmentation_platform::ServiceProxy::ClientInfo>
GetSampleClientInfos() {
  std::vector<segmentation_platform::ServiceProxy::ClientInfo> client_infos;
  client_infos.emplace_back(kTestKey1,
                            segmentation_platform::proto::SegmentId::
                                OPTIMIZATION_TARGET_SEGMENTATION_FEED_USER);
  client_infos.back().segment_status.emplace_back(
      segmentation_platform::proto::SegmentId::
          OPTIMIZATION_TARGET_SEGMENTATION_FEED_USER,
      "test_metadata", "test_result", base::Time::Now(),
      /*can_execute_segment=*/false);
  client_infos.emplace_back(
      kTestKey2, segmentation_platform::proto::SegmentId::
                     OPTIMIZATION_TARGET_SEGMENTATION_ADAPTIVE_TOOLBAR);
  client_infos.back().segment_status.emplace_back(
      segmentation_platform::proto::SegmentId::
          OPTIMIZATION_TARGET_SEGMENTATION_ADAPTIVE_TOOLBAR,
      "test_metadata", "test_result", base::Time::Now() + base::Seconds(10),
      /*can_execute_segment=*/false);
  return client_infos;
}

}  // namespace

class SegmentationInternalsPageHandlerImplTest : public testing::Test {
 public:
  SegmentationInternalsPageHandlerImplTest()
      : handler_(std::make_unique<SegmentationInternalsPageHandlerImpl>(
            mojo::PendingReceiver<segmentation_internals::mojom::PageHandler>(),
            mock_client_.BindAndGetRemote(),
            &segmentation_platform_service_)) {}
  ~SegmentationInternalsPageHandlerImplTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  segmentation_platform::MockSegmentationPlatformService
      segmentation_platform_service_;
  testing::NiceMock<MockPage> mock_client_;
  std::unique_ptr<SegmentationInternalsPageHandlerImpl> handler_;
};

TEST_F(SegmentationInternalsPageHandlerImplTest, EmptyClientInfo) {
  EXPECT_CALL(mock_client_, OnClientInfoAvailable(IsEmpty()));
  handler_->OnClientInfoAvailable({});
  mock_client_.FlushForTesting();
}

TEST_F(SegmentationInternalsPageHandlerImplTest, ClientInfoNotified) {
  EXPECT_CALL(mock_client_, OnClientInfoAvailable(_))
      .WillOnce(Invoke(
          [](std::vector<mojo::StructPtr<
                 segmentation_internals::mojom::ClientInfo>> client_infos) {
            ASSERT_EQ(client_infos.size(), 2u);
            EXPECT_EQ(client_infos[0]->segmentation_key, kTestKey1);
            EXPECT_EQ(client_infos[1]->segmentation_key, kTestKey2);
            base::Time now = base::Time::Now();
            ASSERT_EQ(client_infos[0]->segment_info.size(), 1u);
            ASSERT_EQ(client_infos[1]->segment_info.size(), 1u);
            EXPECT_EQ(client_infos[0]->segment_info[0]->prediction_timestamp,
                      now);
            EXPECT_EQ(client_infos[1]->segment_info[0]->prediction_timestamp,
                      now + base::Seconds(10));
          }));
  handler_->OnClientInfoAvailable(GetSampleClientInfos());
  mock_client_.FlushForTesting();
}
