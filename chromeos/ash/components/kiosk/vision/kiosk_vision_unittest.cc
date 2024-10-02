// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/kiosk/vision/kiosk_vision.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice.pb.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/ash/components/dbus/dlcservice/fake_dlcservice_client.h"
#include "chromeos/ash/components/kiosk/vision/internal/camera_service_connector.h"
#include "chromeos/ash/components/kiosk/vision/internal/fake_cros_camera_service.h"
#include "chromeos/ash/components/kiosk/vision/internal/pref_observer.h"
#include "chromeos/ash/components/kiosk/vision/pref_names.h"
#include "components/media_effects/test/fake_video_source.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/video_capture_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_image_transport_factory.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom-forward.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom.h"
#include "services/video_capture/public/cpp/mock_video_capture_service.h"
#include "services/video_capture/public/cpp/mock_video_source_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/dlcservice/dbus-constants.h"

namespace ash::kiosk_vision {

using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::IsEmpty;

namespace {

void RegisterKioskVisionPrefs(TestingPrefServiceSimple& local_state) {
  RegisterLocalStatePrefs(local_state.registry());
}

void EnableKioskVisionTelemetryPref(PrefService& pref_service) {
  pref_service.SetBoolean(prefs::kKioskVisionTelemetryEnabled, true);
}

void DisableKioskVisionTelemetryPref(PrefService& pref_service) {
  pref_service.SetBoolean(prefs::kKioskVisionTelemetryEnabled, false);
}

DlcserviceClient::InstallResult InstallKioskVisionDlc(
    FakeDlcserviceClient& service) {
  base::test::TestFuture<const DlcserviceClient::InstallResult&> future;
  dlcservice::InstallRequest request;
  request.set_id(kKioskVisionDlcId);
  service.Install(request, future.GetCallback(), base::DoNothing());
  return future.Take();
}

dlcservice::DlcsWithContent GetExistingDlcs(FakeDlcserviceClient& service) {
  base::test::TestFuture<const dlcservice::DlcsWithContent&> future;
  service.GetExistingDlcs(
      base::BindOnce([](std::string_view err,
                        const dlcservice::DlcsWithContent& dlcs) {
        return dlcs;
      }).Then(future.GetCallback()));
  return future.Take();
}

bool IsKioskVisionDlcInstalled(FakeDlcserviceClient& service) {
  const dlcservice::DlcsWithContent& dlcs = GetExistingDlcs(service);
  return base::ranges::any_of(dlcs.dlc_infos(), [](const auto& info) {
    return info.id() == kKioskVisionDlcId;
  });
}

cros::mojom::KioskVisionAppearancePtr NewFakeAppearance(int person_id) {
  auto appearance = cros::mojom::KioskVisionAppearance::New();
  appearance->person_id = person_id;
  appearance->face = cros::mojom::KioskVisionFaceDetection::New();
  appearance->face->box = cros::mojom::KioskVisionBoundingBox::New();
  return appearance;
}

cros::mojom::KioskVisionDetectionPtr NewFakeDetectionOfPersons(
    std::vector<int> person_ids) {
  constexpr int64_t kFakeTimestamp = 1718727537817601;

  std::vector<cros::mojom::KioskVisionAppearancePtr> appearances;
  for (int person_id : person_ids) {
    appearances.emplace_back(NewFakeAppearance(person_id));
  }

  return cros::mojom::KioskVisionDetection::New(kFakeTimestamp,
                                                std::move(appearances));
}

cros::mojom::KioskVisionTrackPtr NewFakeTrackOfPerson(int person_id,
                                                      int count) {
  constexpr int64_t kFakeTimestamp = 1719215767226132;

  std::vector<cros::mojom::KioskVisionAppearancePtr> appearances;
  for (int i = 0; i < count; i++) {
    appearances.emplace_back(NewFakeAppearance(person_id));
  }

  return cros::mojom::KioskVisionTrack::New(
      person_id, kFakeTimestamp, kFakeTimestamp, std::move(appearances));
}

media::VideoCaptureFormat CreateCaptureFormat(const gfx::Size& frame_size,
                                              float frame_rate) {
  return media::VideoCaptureFormat(frame_size, frame_rate,
                                   media::PIXEL_FORMAT_I420);
}

media::VideoCaptureDeviceInfo CreateCaptureDeviceInfo(
    const std::string& device_id,
    const std::string& display_name,
    const std::string& model_id,
    media::VideoFacingMode camera_facing_mode,
    media::VideoCaptureFormats video_capture_formats =
        media::VideoCaptureFormats{
            CreateCaptureFormat(gfx::Size(720, 640), 30.f)}) {
  media::VideoCaptureDeviceInfo info{media::VideoCaptureDeviceDescriptor(
      display_name, device_id, model_id, media::VideoCaptureApi::UNKNOWN,
      media::VideoCaptureControlSupport())};
  info.supported_formats = video_capture_formats;
  info.descriptor.facing = camera_facing_mode;
  return info;
}

CameraServiceConnector::Status GetCameraConnectorStatus(
    const KioskVision& kiosk_vision) {
  auto& camera_connector =
      CHECK_DEREF(kiosk_vision.GetCameraConnectorForTesting());
  return camera_connector.status();
}

}  // namespace

class KioskVisionTest : public testing::Test {
 public:
  void SetUp() override {
    RegisterKioskVisionPrefs(local_state_);

    // Tell the fake DLC service to only record successful installs.
    fake_dlcservice_.set_skip_adding_dlc_info_on_error(true);

    content::ImageTransportFactory::SetFactory(
        std::make_unique<content::TestImageTransportFactory>());

    content::OverrideVideoCaptureServiceForTesting(
        &mock_video_capture_service_);
    ON_CALL(mock_video_capture_service_,
            DoConnectToVideoSourceProvider(testing::_))
        .WillByDefault(testing::Invoke(
            [this](
                mojo::PendingReceiver<video_capture::mojom::VideoSourceProvider>
                    receiver) {
              if (source_provider_receiver_.is_bound()) {
                source_provider_receiver_.reset();
              }
              source_provider_receiver_.Bind(std::move(receiver));
              base::RunLoop().RunUntilIdle();
            }));

    ON_CALL(mock_source_provider_, DoGetSourceInfos(testing::_))
        .WillByDefault(
            testing::Invoke([this](video_capture::mojom::VideoSourceProvider::
                                       GetSourceInfosCallback& callback) {
              std::move(callback).Run(
                  video_capture::mojom::VideoSourceProvider::
                      GetSourceInfosResult::kSuccess,
                  device_infos_);
            }));

    ON_CALL(mock_source_provider_, DoGetVideoSource(testing::_, testing::_))
        .WillByDefault(testing::Invoke(
            [this](const std::string& device_id,
                   mojo::PendingReceiver<video_capture::mojom::VideoSource>*
                       receiver) {
              source_receivers_.Add(&fake_video_source_, std::move(*receiver));
            }));
  }

  void TearDown() override { content::ImageTransportFactory::Terminate(); }

  KioskVisionTest() : source_provider_receiver_(&mock_source_provider_) {}

 protected:
  content::BrowserTaskEnvironment browser_task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  FakeCrosCameraService fake_cros_camera_service_;
  TestingPrefServiceSimple local_state_;
  FakeDlcserviceClient fake_dlcservice_;
  std::vector<media::VideoCaptureDeviceInfo> device_infos_ = {
      CreateCaptureDeviceInfo("/dev/video0",
                              "Integrated Webcam",
                              "0123:4567",
                              media::MEDIA_VIDEO_FACING_NONE)};
  mojo::Receiver<video_capture::mojom::VideoSourceProvider>
      source_provider_receiver_;
  video_capture::MockVideoSourceProvider mock_source_provider_;
  video_capture::MockVideoCaptureService mock_video_capture_service_;
  FakeVideoSource fake_video_source_;
  mojo::ReceiverSet<video_capture::mojom::VideoSource> source_receivers_;
};

TEST_F(KioskVisionTest, TelemetryPrefIsDisabledByDefault) {
  ASSERT_FALSE(IsTelemetryPrefEnabled(local_state_));
}

TEST_F(KioskVisionTest, InstallsDlcWhenEnabled) {
  ASSERT_FALSE(IsKioskVisionDlcInstalled(fake_dlcservice_));
  EnableKioskVisionTelemetryPref(local_state_);

  KioskVision vision(&local_state_);

  EXPECT_TRUE(IsKioskVisionDlcInstalled(fake_dlcservice_));
}

TEST_F(KioskVisionTest, RetriesToInstallDlcOnErrorAfterDelay) {
  ASSERT_FALSE(IsKioskVisionDlcInstalled(fake_dlcservice_));
  EnableKioskVisionTelemetryPref(local_state_);

  // Prepare one install error, such that a second install attempt should work.
  fake_dlcservice_.set_install_errors({dlcservice::kErrorNoImageFound});

  KioskVision vision(&local_state_);

  EXPECT_FALSE(IsKioskVisionDlcInstalled(fake_dlcservice_))
      << "First install attempt should fail";

  browser_task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(IsKioskVisionDlcInstalled(fake_dlcservice_))
      << "Second install didn't work, or there was no retry";
}

TEST_F(KioskVisionTest, UninstallsDlcWhenDisabled) {
  DisableKioskVisionTelemetryPref(local_state_);

  const auto& result = InstallKioskVisionDlc(fake_dlcservice_);
  ASSERT_EQ(result.error, dlcservice::kErrorNone);
  EXPECT_TRUE(IsKioskVisionDlcInstalled(fake_dlcservice_));

  KioskVision vision(&local_state_);

  EXPECT_FALSE(IsKioskVisionDlcInstalled(fake_dlcservice_));
}

TEST_F(KioskVisionTest, BindsDetectionObserver) {
  ASSERT_FALSE(fake_cros_camera_service_.HasObserver());
  EnableKioskVisionTelemetryPref(local_state_);

  KioskVision vision(&local_state_);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(fake_cros_camera_service_.WaitForObserver());
  ASSERT_TRUE(fake_cros_camera_service_.HasObserver());
}

TEST_F(KioskVisionTest, TelemetryProcessorIsNullWhenDisabled) {
  DisableKioskVisionTelemetryPref(local_state_);

  KioskVision vision(&local_state_);

  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(vision.GetTelemetryProcessor(), nullptr);
}

TEST_F(KioskVisionTest, TelemetryProcessorIsNotNullWhenEnabled) {
  EnableKioskVisionTelemetryPref(local_state_);

  KioskVision vision(&local_state_);

  base::RunLoop().RunUntilIdle();
  ASSERT_NE(vision.GetTelemetryProcessor(), nullptr);
}

TEST_F(KioskVisionTest, TelemetryProcessorBecomesNullOnceDisabled) {
  EnableKioskVisionTelemetryPref(local_state_);

  KioskVision vision(&local_state_);

  base::RunLoop().RunUntilIdle();
  EXPECT_NE(vision.GetTelemetryProcessor(), nullptr);

  DisableKioskVisionTelemetryPref(local_state_);

  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(vision.GetTelemetryProcessor(), nullptr);
}

TEST_F(KioskVisionTest, TelemetryProcessorStartsWithoutDetections) {
  EnableKioskVisionTelemetryPref(local_state_);

  KioskVision vision(&local_state_);

  ASSERT_TRUE(fake_cros_camera_service_.WaitForObserver());

  auto& processor = CHECK_DEREF(vision.GetTelemetryProcessor());

  base::RunLoop().RunUntilIdle();

  ASSERT_THAT(processor.TakeIdsProcessed(), IsEmpty());
  ASSERT_THAT(processor.TakeErrors(), IsEmpty());
}

TEST_F(KioskVisionTest, TelemetryProcessorReceivesDetections) {
  EnableKioskVisionTelemetryPref(local_state_);

  KioskVision vision(&local_state_);

  ASSERT_TRUE(fake_cros_camera_service_.WaitForObserver());

  auto& processor = CHECK_DEREF(vision.GetTelemetryProcessor());

  fake_cros_camera_service_.EmitFakeDetection(
      NewFakeDetectionOfPersons({123, 45}));

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(processor.TakeIdsProcessed(), ElementsAreArray({123, 45}));
  EXPECT_THAT(processor.TakeErrors(), IsEmpty());
}

TEST_F(KioskVisionTest, TelemetryProcessorReceivesTracks) {
  EnableKioskVisionTelemetryPref(local_state_);

  KioskVision vision(&local_state_);

  ASSERT_TRUE(fake_cros_camera_service_.WaitForObserver());

  auto& processor = CHECK_DEREF(vision.GetTelemetryProcessor());

  fake_cros_camera_service_.EmitFakeTrack(
      NewFakeTrackOfPerson(123, /*count=*/3));

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(processor.TakeIdsProcessed(), ElementsAreArray({123}));
  EXPECT_THAT(processor.TakeErrors(), IsEmpty());
}

TEST_F(KioskVisionTest, TelemetryProcessorReceivesErrors) {
  EnableKioskVisionTelemetryPref(local_state_);

  KioskVision vision(&local_state_);

  ASSERT_TRUE(fake_cros_camera_service_.WaitForObserver());

  auto& processor = CHECK_DEREF(vision.GetTelemetryProcessor());

  auto error = cros::mojom::KioskVisionError::MODEL_ERROR;
  fake_cros_camera_service_.EmitFakeError(error);

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(processor.TakeIdsProcessed(), IsEmpty());
  EXPECT_THAT(processor.TakeErrors(), ElementsAre(error));
}

TEST_F(KioskVisionTest, VideoStreamHasStarted) {
  EnableKioskVisionTelemetryPref(local_state_);
  KioskVision vision(&local_state_);

  EXPECT_CALL(mock_source_provider_, DoGetVideoSource(testing::_, testing::_))
      .WillOnce(testing::Invoke(
          [this](const std::string& device_id,
                 mojo::PendingReceiver<video_capture::mojom::VideoSource>*
                     receiver) {
            source_receivers_.Add(&fake_video_source_, std::move(*receiver));
          }));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GetCameraConnectorStatus(vision),
            CameraServiceConnector::Status::kVideoStreamStarted);
}

TEST_F(KioskVisionTest, CameraNotConnected) {
  EnableKioskVisionTelemetryPref(local_state_);
  device_infos_ = {};

  KioskVision vision(&local_state_);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GetCameraConnectorStatus(vision),
            CameraServiceConnector::Status::kCameraNotConnected);
}

TEST_F(KioskVisionTest, VideoStreamFatalError) {
  EnableKioskVisionTelemetryPref(local_state_);
  KioskVision vision(&local_state_);

  base::RunLoop().RunUntilIdle();
  fake_video_source_.SendError(
      media::VideoCaptureError::
          kCrosHalV3DeviceDelegateFailedToOpenCameraDevice);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GetCameraConnectorStatus(vision),
            CameraServiceConnector::Status::kFatalErrorOrDisconnection);
}

TEST_F(KioskVisionTest, CheckVideoFormat) {
  EnableKioskVisionTelemetryPref(local_state_);
  device_infos_ = {CreateCaptureDeviceInfo(
      "/dev/video0", "Integrated Webcam", "0123:4567",
      media::MEDIA_VIDEO_FACING_NONE,
      media::VideoCaptureFormats{
          CreateCaptureFormat(
              gfx::Size(kRequestedFormatWidth - 100, kRequestedFormatHeight),
              kMinimumFrameRate + 10),
          CreateCaptureFormat(
              gfx::Size(kRequestedFormatWidth, kRequestedFormatHeight - 5),
              kMinimumFrameRate + 3),
          CreateCaptureFormat(gfx::Size(kRequestedFormatWidth + 50,
                                        kRequestedFormatHeight + 100),
                              kMinimumFrameRate + 5),  // The best format
          CreateCaptureFormat(
              gfx::Size(kRequestedFormatWidth, kRequestedFormatHeight),
              kMinimumFrameRate - 1),
          CreateCaptureFormat(gfx::Size(kRequestedFormatWidth + 100,
                                        kRequestedFormatHeight + 100),
                              kMinimumFrameRate + 10),
          CreateCaptureFormat(gfx::Size(kRequestedFormatWidth + 100,
                                        kRequestedFormatHeight + 100),
                              kMinimumFrameRate + 5),
      })};

  KioskVision vision(&local_state_);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(fake_video_source_.get_requested_settings().requested_format,
            device_infos_[0].supported_formats[2]);
}

}  // namespace ash::kiosk_vision
