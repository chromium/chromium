// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_effects/media_device_info.h"

#include <optional>

#include "base/system/system_monitor.h"
#include "base/test/test_future.h"
#include "components/media_effects/test/fake_audio_service.h"
#include "components/media_effects/test/fake_video_capture_service.h"
#include "content/public/browser/audio_service.h"
#include "content/public/browser/video_capture_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using media_effects::GetRealAudioDeviceNames;
using media_effects::GetRealDefaultDeviceId;
using media_effects::GetRealVideoDeviceNames;

media::AudioDeviceDescription GetAudioDeviceDescription(size_t index) {
  return media::AudioDeviceDescription{
      /*device_name=*/base::StringPrintf("name_%zu", index),
      /*unique_id=*/base::StringPrintf("id_%zu", index),
      /*group_id=*/base::StringPrintf("group_%zu", index)};
}

media::VideoCaptureDeviceDescriptor GetVideoCaptureDeviceDescriptor(
    size_t index) {
  return media::VideoCaptureDeviceDescriptor{
      /*display_name=*/base::StringPrintf("name_%zu", index),
      /*device_id=*/base::StringPrintf("id_%zu", index)};
}

class MockDeviceChangeObserver
    : public media_effects::MediaDeviceInfo::Observer {
 public:
  MOCK_METHOD(void,
              OnAudioDevicesChanged,
              (const std::optional<std::vector<media::AudioDeviceDescription>>&
                   device_infos));
  MOCK_METHOD(void,
              OnVideoDevicesChanged,
              (const std::optional<std::vector<media::VideoCaptureDeviceInfo>>&
                   device_infos));
};

auto IsVideoDevice(size_t expected_index) {
  return testing::Field(&media::VideoCaptureDeviceInfo::descriptor,
                        GetVideoCaptureDeviceDescriptor(expected_index));
}

}  // namespace

class MediaDeviceInfoTest : public testing::Test {
  void SetUp() override {
    auto_reset_audio_service_.emplace(
        content::OverrideAudioServiceForTesting(&fake_audio_service_));
    content::OverrideVideoCaptureServiceForTesting(
        &fake_video_capture_service_);
    auto_reset_media_device_info_override_.emplace(
        media_effects::MediaDeviceInfo::OverrideInstanceForTesting());
  }

  void TearDown() override {
    content::OverrideVideoCaptureServiceForTesting(nullptr);
  }

 protected:
  media::AudioDeviceDescription AddFakeAudioDevice(size_t index) {
    base::test::TestFuture<void> replied_audio_device_infos_future;
    fake_audio_service_.SetOnRepliedWithInputDeviceDescriptionsCallback(
        replied_audio_device_infos_future.GetCallback());
    auto device = GetAudioDeviceDescription(index);
    fake_audio_service_.AddFakeInputDevice(device);
    CHECK(replied_audio_device_infos_future.WaitAndClear());
    return device;
  }

  media::VideoCaptureDeviceDescriptor AddFakeVideoDevice(size_t index) {
    base::test::TestFuture<void> replied_video_device_infos_future;
    fake_video_capture_service_.SetOnRepliedWithSourceInfosCallback(
        replied_video_device_infos_future.GetCallback());
    auto device = GetVideoCaptureDeviceDescriptor(index);
    fake_video_capture_service_.AddFakeCamera(device);
    CHECK(replied_video_device_infos_future.WaitAndClear());
    return device;
  }

  content::BrowserTaskEnvironment task_environment_;
  base::SystemMonitor monitor_;
  media_effects::FakeAudioService fake_audio_service_;
  std::optional<base::AutoReset<audio::mojom::AudioService*>>
      auto_reset_audio_service_;
  media_effects::FakeVideoCaptureService fake_video_capture_service_;
  // When `MediaDeviceInfo::OverrideInstanceForTesting()` is called it returns
  // an `AutoReset` that removes the override when it's destructed.
  // std::optional is used to hold it because we don't get the value until
  // `SetUp()` and `AutoReset` doesn't have a default constructor.
  std::optional<std::pair<std::unique_ptr<media_effects::MediaDeviceInfo>,
                          base::AutoReset<media_effects::MediaDeviceInfo*>>>
      auto_reset_media_device_info_override_;
};

// Test that audio device infos are retrieved on construction and updated when
// changes are observed from `SystemMonitor`.
TEST_F(MediaDeviceInfoTest, GetAudioDeviceInfos) {
  base::test::TestFuture<void> replied_audio_device_infos_future;
  fake_audio_service_.SetOnRepliedWithInputDeviceDescriptionsCallback(
      replied_audio_device_infos_future.GetCallback());
  EXPECT_FALSE(media_effects::MediaDeviceInfo::GetInstance()
                   ->GetAudioDeviceInfos()
                   .has_value());
  ASSERT_TRUE(replied_audio_device_infos_future.WaitAndClear());
  EXPECT_TRUE(media_effects::MediaDeviceInfo::GetInstance()
                  ->GetAudioDeviceInfos()
                  .has_value());

  auto device_0 = AddFakeAudioDevice(0);
  EXPECT_THAT(
      media_effects::MediaDeviceInfo::GetInstance()->GetAudioDeviceInfos(),
      testing::Optional(testing::ElementsAre(device_0)));
  auto device_1 = AddFakeAudioDevice(1);
  EXPECT_THAT(
      media_effects::MediaDeviceInfo::GetInstance()->GetAudioDeviceInfos(),
      testing::Optional(testing::ElementsAre(device_0, device_1)));
}

// Test that getting audio device info by ID returns the correct device or
// nullopt if not found.
TEST_F(MediaDeviceInfoTest, GetAudioDeviceInfoForId) {
  auto device_0 = AddFakeAudioDevice(0);
  auto device_1 = AddFakeAudioDevice(1);
  auto device_2 = AddFakeAudioDevice(2);
  EXPECT_EQ(device_1, media_effects::MediaDeviceInfo::GetInstance()
                          ->GetAudioDeviceInfoForId(device_1.unique_id));
  EXPECT_EQ(std::nullopt, media_effects::MediaDeviceInfo::GetInstance()
                              ->GetAudioDeviceInfoForId("not_found_id"));
}

// Test that video device infos are retrieved on construction and updated when
// changes are observed from `SystemMonitor`.
TEST_F(MediaDeviceInfoTest, GetVideoDeviceInfos) {
  base::test::TestFuture<void> replied_video_device_infos_future;
  fake_video_capture_service_.SetOnRepliedWithSourceInfosCallback(
      replied_video_device_infos_future.GetCallback());
  EXPECT_FALSE(media_effects::MediaDeviceInfo::GetInstance()
                   ->GetVideoDeviceInfos()
                   .has_value());
  ASSERT_TRUE(replied_video_device_infos_future.WaitAndClear());
  EXPECT_TRUE(media_effects::MediaDeviceInfo::GetInstance()
                  ->GetVideoDeviceInfos()
                  .has_value());

  auto device_0 = AddFakeVideoDevice(0);
  EXPECT_THAT(
      media_effects::MediaDeviceInfo::GetInstance()->GetVideoDeviceInfos(),
      testing::Optional(testing::ElementsAre(IsVideoDevice(0))));
  auto device_1 = AddFakeVideoDevice(1);
  EXPECT_THAT(
      media_effects::MediaDeviceInfo::GetInstance()->GetVideoDeviceInfos(),
      testing::Optional(
          testing::ElementsAre(IsVideoDevice(0), IsVideoDevice(1))));
}

// Test that getting video device info by ID returns the correct device or
// nullopt if not found.
TEST_F(MediaDeviceInfoTest, GetVideoDeviceInfoForId) {
  auto device_0 = AddFakeVideoDevice(0);
  auto device_1 = AddFakeVideoDevice(1);
  auto device_2 = AddFakeVideoDevice(2);
  EXPECT_EQ(device_1, media_effects::MediaDeviceInfo::GetInstance()
                          ->GetVideoDeviceInfoForId(device_1.device_id)
                          ->descriptor);
  EXPECT_EQ(std::nullopt, media_effects::MediaDeviceInfo::GetInstance()
                              ->GetVideoDeviceInfoForId("not_found_id"));
}

// Test that appropriate observer methods are called when device infos change.
TEST_F(MediaDeviceInfoTest, Observers) {
  MockDeviceChangeObserver observer;
  media_effects::MediaDeviceInfo::GetInstance()->AddObserver(&observer);
  base::test::TestFuture<void> replied_video_device_infos_future;
  fake_video_capture_service_.SetOnRepliedWithSourceInfosCallback(
      replied_video_device_infos_future.GetCallback());
  EXPECT_CALL(observer,
              OnAudioDevicesChanged(testing::Optional(testing::IsEmpty())));
  EXPECT_CALL(observer,
              OnVideoDevicesChanged(testing::Optional(testing::IsEmpty())));
  ASSERT_TRUE(replied_video_device_infos_future.WaitAndClear());

  // Audio device changes
  EXPECT_CALL(observer,
              OnAudioDevicesChanged(testing::Optional(
                  testing::ElementsAre(GetAudioDeviceDescription(0)))));
  AddFakeAudioDevice(0);

  EXPECT_CALL(
      observer,
      OnAudioDevicesChanged(testing::Optional(testing::ElementsAre(
          GetAudioDeviceDescription(0), GetAudioDeviceDescription(1)))));
  AddFakeAudioDevice(1);

  EXPECT_CALL(observer,
              OnAudioDevicesChanged(testing::Optional(testing::ElementsAre(
                  GetAudioDeviceDescription(0), GetAudioDeviceDescription(1),
                  GetAudioDeviceDescription(2)))));
  AddFakeAudioDevice(2);

  // Video device changes
  EXPECT_CALL(observer, OnVideoDevicesChanged(testing::Optional(
                            testing::ElementsAre(IsVideoDevice(0)))));
  AddFakeVideoDevice(0);

  EXPECT_CALL(observer,
              OnVideoDevicesChanged(testing::Optional(
                  testing::ElementsAre(IsVideoDevice(0), IsVideoDevice(1)))));
  auto device_1 = AddFakeVideoDevice(1);

  EXPECT_CALL(observer,
              OnVideoDevicesChanged(testing::Optional(testing::ElementsAre(
                  IsVideoDevice(0), IsVideoDevice(1), IsVideoDevice(2)))));
  auto device_2 = AddFakeVideoDevice(2);

  // Remove observer
  EXPECT_CALL(observer, OnAudioDevicesChanged(testing::_)).Times(0);
  EXPECT_CALL(observer, OnVideoDevicesChanged(testing::_)).Times(0);
  media_effects::MediaDeviceInfo::GetInstance()->RemoveObserver(&observer);
  AddFakeAudioDevice(3);
  AddFakeVideoDevice(3);
}

TEST(MediaDeviceInfoTestGeneral, DefaultAudioDeviceHandling) {
  std::vector<media::AudioDeviceDescription> infos;
  EXPECT_EQ(GetRealDefaultDeviceId(infos), std::nullopt);
  EXPECT_EQ(GetRealAudioDeviceNames(infos).size(), 0u);

  infos.push_back(GetAudioDeviceDescription(0));
  infos.push_back(GetAudioDeviceDescription(1));
  infos.push_back(GetAudioDeviceDescription(2));
  EXPECT_EQ(GetRealDefaultDeviceId(infos), std::nullopt);
  EXPECT_THAT(GetRealAudioDeviceNames(infos),
              testing::ElementsAre(infos[0].device_name, infos[1].device_name,
                                   infos[2].device_name));

  infos.front().is_system_default = true;
  EXPECT_EQ(GetRealDefaultDeviceId(infos), infos.front().unique_id);
  EXPECT_THAT(GetRealAudioDeviceNames(infos),
              testing::ElementsAre(infos[0].device_name, infos[1].device_name,
                                   infos[2].device_name));

  infos.front().unique_id = media::AudioDeviceDescription::kDefaultDeviceId;
  EXPECT_EQ(GetRealDefaultDeviceId(infos), std::nullopt);
  EXPECT_THAT(GetRealAudioDeviceNames(infos),
              testing::ElementsAre(infos[1].device_name, infos[2].device_name));

  infos[1].is_system_default = true;
  EXPECT_EQ(GetRealDefaultDeviceId(infos), infos[1].unique_id);
  EXPECT_THAT(GetRealAudioDeviceNames(infos),
              testing::ElementsAre(infos[1].device_name, infos[2].device_name));

  infos[2].unique_id = media::AudioDeviceDescription::kCommunicationsDeviceId;
  EXPECT_EQ(GetRealDefaultDeviceId(infos), infos[1].unique_id);
  EXPECT_THAT(GetRealAudioDeviceNames(infos),
              testing::ElementsAre(infos[1].device_name));
}

TEST(MediaDeviceInfoTestGeneral, GetVideoDeviceNames) {
  std::vector<media::VideoCaptureDeviceInfo> infos;
  EXPECT_EQ(GetRealVideoDeviceNames(infos).size(), 0u);

  infos.emplace_back(GetVideoCaptureDeviceDescriptor(0));
  infos.emplace_back(GetVideoCaptureDeviceDescriptor(1));
  infos.emplace_back(GetVideoCaptureDeviceDescriptor(2));

  EXPECT_THAT(GetRealVideoDeviceNames(infos),
              testing::ElementsAre(infos[0].descriptor.GetNameAndModel(),
                                   infos[1].descriptor.GetNameAndModel(),
                                   infos[2].descriptor.GetNameAndModel()));
}
