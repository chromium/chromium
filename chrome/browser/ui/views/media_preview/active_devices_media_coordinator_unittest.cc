// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/active_devices_media_coordinator.h"

#include <optional>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/media_preview/media_view.h"
#include "chrome/test/base/testing_profile.h"
#include "components/media_effects/test/fake_audio_service.h"
#include "components/media_effects/test/fake_video_capture_service.h"
#include "content/public/browser/audio_service.h"
#include "content/public/browser/background_tracing_manager.h"
#include "content/public/browser/video_capture_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kMutableCoordinatorId[] = "changeable";

constexpr char kDevice1Id[] = "device_1";
constexpr char kDevice2Id[] = "device_2";

blink::mojom::MediaStreamType ViewTypeToMediaStreamType(
    MediaCoordinator::ViewType view_type) {
  switch (view_type) {
    case MediaCoordinator::ViewType::kCameraOnly:
      return blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE;
    case MediaCoordinator::ViewType::kMicOnly:
      return blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE;
    default:
      return blink::mojom::MediaStreamType::NO_SERVICE;
  }
}

std::string ViewTypeToNumDevicesHistogramName(
    MediaCoordinator::ViewType view_type) {
  switch (view_type) {
    case MediaCoordinator::ViewType::kCameraOnly:
      return "MediaPreviews.UI.PageInfo.Camera.NumInUseDevices";
    case MediaCoordinator::ViewType::kMicOnly:
      return "MediaPreviews.UI.PageInfo.Mic.NumInUseDevices";
    default:
      return "UnmappedViewType";
  }
}

media_preview_metrics::Context GetMetricsContext(
    MediaCoordinator::ViewType type) {
  return media_preview_metrics::Context(
      media_preview_metrics::UiLocation::kPageInfo,
      media_coordinator::GetPreviewTypeFromMediaCoordinatorViewType(type));
}

}  // namespace

class ActiveDevicesMediaCoordinatorTestParameterized
    : public testing::TestWithParam<MediaCoordinator::ViewType>,
      views::ViewObserver {
 public:
  void SetUp() override {
    auto view_type = GetParam();
    media_stream_type_ = ViewTypeToMediaStreamType(view_type);
    num_devices_histogram_name_ = ViewTypeToNumDevicesHistogramName(view_type);
    fake_audio_service_auto_reset_ =
        content::OverrideAudioServiceForTesting(&fake_audio_service_);
    content::OverrideVideoCaptureServiceForTesting(
        &fake_video_capture_service_);
    background_tracing_manager_ =
        content::BackgroundTracingManager::CreateInstance();
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        &profile_, /*instance=*/nullptr);
    web_contents_tester_ = content::WebContentsTester::For(web_contents_.get());
    web_contents_tester_->SetMediaCaptureRawDeviceIdsOpened(media_stream_type_,
                                                            {});
    rfh_id_ = web_contents_->GetPrimaryMainFrame()->GetGlobalId();
    histogram_tester_.emplace();
    coordinator_.emplace(web_contents_->GetWeakPtr(), view_type, &parent_view_,
                         GetMetricsContext(view_type));
  }

  void TearDown() override {
    content::OverrideVideoCaptureServiceForTesting(nullptr);
  }

  void TriggerMediaRequestStateUpdate(std::string device_id,
                                      content::MediaRequestState state,
                                      content::GlobalRenderFrameHostId rfh_id) {
    if (state == content::MediaRequestState::MEDIA_REQUEST_STATE_DONE) {
      open_device_ids_.insert(device_id);
    } else if (state ==
               content::MediaRequestState::MEDIA_REQUEST_STATE_CLOSING) {
      open_device_ids_.erase(device_id);
    }

    web_contents_tester_->SetMediaCaptureRawDeviceIdsOpened(
        media_stream_type_,
        std::vector(open_device_ids_.begin(), open_device_ids_.end()));
    coordinator_->OnRequestUpdate(rfh_id.child_id, rfh_id.frame_routing_id,
                                  media_stream_type_, state);

    // Wait until the coordinator actually gets the new list of active devices.
    WaitForGetMediaCaptureRawDeviceIdsOpened();
  }

  void WaitForGetMediaCaptureRawDeviceIdsOpened() {
    base::test::TestFuture<std::vector<std::string>> get_ids_opened_future;
    web_contents_->GetMediaCaptureRawDeviceIdsOpened(
        media_stream_type_, get_ids_opened_future.GetCallback());
    EXPECT_TRUE(get_ids_opened_future.Wait());
  }

  void ExpectNumDevicesHistogramUpdate(size_t bucket_min_value, size_t count) {
    histogram_tester_->ExpectUniqueSample(num_devices_histogram_name_,
                                          bucket_min_value, count);
    ResetHistogramTester();
  }

  void ExpectNoNumDevicesHistogramUpdate() {
    histogram_tester_->ExpectTotalCount(num_devices_histogram_name_, 0);
    ResetHistogramTester();
  }

  void ResetHistogramTester() { histogram_tester_.emplace(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  ChromeLayoutProvider layout_provider_;
  std::unique_ptr<content::BackgroundTracingManager>
      background_tracing_manager_;
  TestingProfile profile_;
  MediaView parent_view_;
  media_effects::FakeAudioService fake_audio_service_;
  std::optional<base::AutoReset<audio::mojom::AudioService*>>
      fake_audio_service_auto_reset_;
  media_effects::FakeVideoCaptureService fake_video_capture_service_;
  base::flat_set<std::string> open_device_ids_;
  blink::mojom::MediaStreamType media_stream_type_;
  std::optional<base::HistogramTester> histogram_tester_;

 protected:
  std::unique_ptr<content::WebContents> web_contents_;
  raw_ptr<content::WebContentsTester> web_contents_tester_;
  content::GlobalRenderFrameHostId rfh_id_;
  std::optional<ActiveDevicesMediaCoordinator> coordinator_;
  std::string num_devices_histogram_name_;
};

INSTANTIATE_TEST_SUITE_P(
    ActiveDevicesMediaCoordinatorTest,
    ActiveDevicesMediaCoordinatorTestParameterized,
    testing::Values(MediaCoordinator::ViewType::kCameraOnly,
                    MediaCoordinator::ViewType::kMicOnly));

TEST_P(ActiveDevicesMediaCoordinatorTestParameterized, DefaultToMutable) {
  WaitForGetMediaCaptureRawDeviceIdsOpened();
  EXPECT_THAT(coordinator_->GetMediaCoordinatorKeys(),
              testing::ElementsAre(kMutableCoordinatorId));
  ExpectNumDevicesHistogramUpdate(/*bucket_min_value=*/0, /*count=*/1);
}

TEST_P(ActiveDevicesMediaCoordinatorTestParameterized, AddsActiveDevices) {
  WaitForGetMediaCaptureRawDeviceIdsOpened();
  ResetHistogramTester();

  // Add device 1 and report stream started. expect device 1 to be in
  // coordinator keys.
  TriggerMediaRequestStateUpdate(
      kDevice1Id, content::MediaRequestState::MEDIA_REQUEST_STATE_DONE,
      rfh_id_);
  EXPECT_THAT(coordinator_->GetMediaCoordinatorKeys(),
              testing::ElementsAre(kDevice1Id));
  ExpectNumDevicesHistogramUpdate(/*bucket_min_value=*/1, /*count=*/1);

  // Add device 2 and report stream started. Expect both device 1 and
  // device 2 to be in coordinator keys.
  TriggerMediaRequestStateUpdate(
      kDevice2Id, content::MediaRequestState::MEDIA_REQUEST_STATE_DONE,
      rfh_id_);
  EXPECT_THAT(coordinator_->GetMediaCoordinatorKeys(),
              testing::ElementsAre(kDevice1Id, kDevice2Id));
  ExpectNumDevicesHistogramUpdate(/*bucket_min_value=*/2, /*count=*/1);

  // Remove device 1 and report stream closed. Expect device 1 to be removed
  // from coordinator keys.
  TriggerMediaRequestStateUpdate(
      kDevice1Id, content::MediaRequestState::MEDIA_REQUEST_STATE_CLOSING,
      rfh_id_);
  EXPECT_THAT(coordinator_->GetMediaCoordinatorKeys(),
              testing::ElementsAre(kDevice2Id));
  ExpectNumDevicesHistogramUpdate(/*bucket_min_value=*/1, /*count=*/1);

  // Remove device 2 and report stream closed. Expect device 2 to be removed
  // from coordinator keys.
  TriggerMediaRequestStateUpdate(
      kDevice2Id, content::MediaRequestState::MEDIA_REQUEST_STATE_CLOSING,
      rfh_id_);
  EXPECT_THAT(coordinator_->GetMediaCoordinatorKeys(),
              testing::ElementsAre(kMutableCoordinatorId));
  ExpectNumDevicesHistogramUpdate(/*bucket_min_value=*/0, /*count=*/1);
}

TEST_P(ActiveDevicesMediaCoordinatorTestParameterized,
       NoChangeForIgnoredMediaRequestStates) {
  WaitForGetMediaCaptureRawDeviceIdsOpened();
  ResetHistogramTester();

  // Add device 2 and report stream started. Expect device 2 to be in
  // coordinator keys.
  TriggerMediaRequestStateUpdate(
      kDevice2Id, content::MediaRequestState::MEDIA_REQUEST_STATE_DONE,
      rfh_id_);
  EXPECT_THAT(coordinator_->GetMediaCoordinatorKeys(),
              testing::ElementsAre(kDevice2Id));
  ExpectNumDevicesHistogramUpdate(/*bucket_min_value=*/1, /*count=*/1);

  // Test that all states other than DONE and CLOSING are ignored.
  for (const auto& state : {
           content::MediaRequestState::MEDIA_REQUEST_STATE_NOT_REQUESTED,
           content::MediaRequestState::MEDIA_REQUEST_STATE_REQUESTED,
           content::MediaRequestState::MEDIA_REQUEST_STATE_PENDING_APPROVAL,
           content::MediaRequestState::MEDIA_REQUEST_STATE_OPENING,
           content::MediaRequestState::MEDIA_REQUEST_STATE_ERROR,
       }) {
    TriggerMediaRequestStateUpdate(kDevice2Id, state, rfh_id_);
    EXPECT_THAT(coordinator_->GetMediaCoordinatorKeys(),
                testing::ElementsAre(kDevice2Id));
    ExpectNoNumDevicesHistogramUpdate();
  }
}

TEST_P(ActiveDevicesMediaCoordinatorTestParameterized,
       NoChangeForRequestStateChangeInDifferentFrame) {
  WaitForGetMediaCaptureRawDeviceIdsOpened();
  ResetHistogramTester();

  // Add device 2 and report stream started. Expect device 2 to be in
  // coordinator keys.
  TriggerMediaRequestStateUpdate(
      kDevice2Id, content::MediaRequestState::MEDIA_REQUEST_STATE_DONE,
      rfh_id_);
  EXPECT_THAT(coordinator_->GetMediaCoordinatorKeys(),
              testing::ElementsAre(kDevice2Id));
  ExpectNumDevicesHistogramUpdate(/*bucket_min_value=*/1, /*count=*/1);

  // Event from another frame is ignored.
  TriggerMediaRequestStateUpdate(
      kDevice1Id, content::MediaRequestState::MEDIA_REQUEST_STATE_DONE,
      content::GlobalRenderFrameHostId{42, 35});
  EXPECT_THAT(coordinator_->GetMediaCoordinatorKeys(),
              testing::ElementsAre(kDevice2Id));
  ExpectNoNumDevicesHistogramUpdate();
}
