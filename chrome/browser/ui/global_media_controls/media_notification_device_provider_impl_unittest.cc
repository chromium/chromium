// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/media_notification_device_provider_impl.h"

#include "base/ranges/algorithm.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_system.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockAudioSystem : public media::AudioSystem {
 public:
  explicit MockAudioSystem(media::AudioDeviceDescriptions descriptions)
      : descriptions_(std::move(descriptions)) {}
  ~MockAudioSystem() override = default;

  void GetDeviceDescriptions(
      bool,
      OnDeviceDescriptionsCallback on_descriptions_cb) override {
    std::move(on_descriptions_cb).Run(descriptions_);
  }

  MOCK_METHOD(void,
              GetInputStreamParameters,
              (const std::string& device_id,
               OnAudioParamsCallback on_params_cb),
              (override));
  MOCK_METHOD(void,
              GetOutputStreamParameters,
              (const std::string& device_id,
               OnAudioParamsCallback on_params_cb),
              (override));
  MOCK_METHOD(void,
              HasInputDevices,
              (OnBoolCallback on_has_devices_cb),
              (override));
  MOCK_METHOD(void,
              HasOutputDevices,
              (OnBoolCallback on_has_devices_cb),
              (override));
  MOCK_METHOD(void,
              GetAssociatedOutputDeviceID,
              (const std::string& input_device_id,
               OnDeviceIdCallback on_device_id_cb),
              (override));
  MOCK_METHOD(void,
              GetInputDeviceInfo,
              (const std::string& input_device_id,
               OnInputDeviceInfoCallback on_input_device_info_cb),
              (override));

 private:
  media::AudioDeviceDescriptions descriptions_;
};

bool DescriptionsAreEqual(const media::AudioDeviceDescriptions& lhs,
                          const media::AudioDeviceDescriptions& rhs) {
  return base::ranges::equal(lhs, rhs, [](const auto& lhs, const auto& rhs) {
    // Group IDs are not used by this test and are therefore ignored in
    // comparison.
    return lhs.device_name == rhs.device_name && lhs.unique_id == rhs.unique_id;
  });
}

media::AudioDeviceDescriptions DescriptionsFromProvider(
    media::AudioDeviceDescriptions descriptions_from_audio_system) {
  MediaNotificationDeviceProviderImpl device_provider(
      std::make_unique<MockAudioSystem>(
          std::move(descriptions_from_audio_system)));
  media::AudioDeviceDescriptions result;
  device_provider.GetOutputDeviceDescriptions(base::BindOnce(
      [](media::AudioDeviceDescriptions* result,
         media::AudioDeviceDescriptions descriptions) {
        *result = std::move(descriptions);
      },
      &result));
  return result;
}

}  // anonymous namespace

TEST(MediaNotificationDeviceProviderTest,
     MaybeRemoveDefaultDeviceRemovesDefaultDevice) {
  media::AudioDeviceDescriptions descriptions;
  descriptions.emplace_back("Speaker", "1", "");
  descriptions.emplace_back("Headphones", "2", "");
  descriptions.emplace_back("Monitor", "3", "");
  // The name of this device indicates that it falls back to "Speaker".
  // MaybeRemoveDefaultDevice should remove the default device and change the ID
  // of "Speaker".
  descriptions.emplace_back(
      media::AudioDeviceDescription::GetDefaultDeviceName() + " - Speaker",
      media::AudioDeviceDescription::kDefaultDeviceId, "");

  media::AudioDeviceDescriptions expected_descriptions;
  expected_descriptions.emplace_back(
      "Speaker", media::AudioDeviceDescription::kDefaultDeviceId, "");
  expected_descriptions.emplace_back("Headphones", "2", "");
  expected_descriptions.emplace_back("Monitor", "3", "");

  auto result = DescriptionsFromProvider(std::move(descriptions));
  EXPECT_TRUE(DescriptionsAreEqual(result, expected_descriptions));
}

TEST(MediaNotificationDeviceProviderTest,
     MaybeRemoveDefaultDeviceDoesNotRemoveDefaultDevice) {
  media::AudioDeviceDescriptions descriptions;
  descriptions.emplace_back("Speaker", "1", "");
  descriptions.emplace_back("Headphones", "2", "");
  descriptions.emplace_back("Monitor", "3", "");
  // The default device descriptions name does not prefix another device's name.
  // Thus, it is ambigious which device is the fallback for this description.
  // MaybeRemoveDefaultDevice should not remove the default device description
  // in this case.
  descriptions.emplace_back(
      media::AudioDeviceDescription::GetDefaultDeviceName(),
      media::AudioDeviceDescription::kDefaultDeviceId, "");

  media::AudioDeviceDescriptions original_descriptions = descriptions;
  auto result = DescriptionsFromProvider(std::move(descriptions));
  EXPECT_TRUE(DescriptionsAreEqual(result, original_descriptions));
}

TEST(MediaNotificationDeviceProviderTest,
     MaybeRemoveDefaultDeviceWithMultipleRealDefaultDeviceNames) {
  media::AudioDeviceDescriptions descriptions;
  descriptions.emplace_back("Speaker", "1", "");
  descriptions.emplace_back("Speaker", "2", "");
  descriptions.emplace_back("Headphones", "3", "");
  // This name would indicate that the default device falls back to "Speaker",
  // however multiple devices have that name. Thus, it is ambigious which device
  // is the fallback for this description. MaybeRemoveDefaultDevice should not
  // remove the default device description in this case.
  descriptions.emplace_back(
      media::AudioDeviceDescription::GetDefaultDeviceName() + " - Speaker",
      media::AudioDeviceDescription::kDefaultDeviceId, "");

  media::AudioDeviceDescriptions original_descriptions = descriptions;
  auto result = DescriptionsFromProvider(std::move(descriptions));
  EXPECT_TRUE(DescriptionsAreEqual(result, original_descriptions));
}

TEST(MediaNotificationDeviceProviderTest, NoDefaultDevice) {
  media::AudioDeviceDescriptions descriptions;
  descriptions.emplace_back("Speaker", "1", "");
  descriptions.emplace_back("Headphones", "2", "");
  descriptions.emplace_back("Monitor", "3", "");

  media::AudioDeviceDescriptions original_descriptions = descriptions;
  auto result = DescriptionsFromProvider(std::move(descriptions));
  EXPECT_TRUE(DescriptionsAreEqual(result, original_descriptions));
}

TEST(MediaNotificationDeviceProviderTest,
     MaybeRemoveDefaultDeviceMultipleTimes) {
  media::AudioDeviceDescriptions descriptions;
  descriptions.emplace_back("Speaker", "1", "");
  descriptions.emplace_back("Headphones", "2", "");
  descriptions.emplace_back("Monitor", "3", "");
  descriptions.emplace_back(
      media::AudioDeviceDescription::GetDefaultDeviceName() + " - Speaker",
      media::AudioDeviceDescription::kDefaultDeviceId, "");

  media::AudioDeviceDescriptions expected_descriptions;
  expected_descriptions.emplace_back(
      "Speaker", media::AudioDeviceDescription::kDefaultDeviceId, "");
  expected_descriptions.emplace_back("Headphones", "2", "");
  expected_descriptions.emplace_back("Monitor", "3", "");

  auto result = DescriptionsFromProvider(descriptions);
  EXPECT_TRUE(DescriptionsAreEqual(result, expected_descriptions));
  // Subsequent calls should not modify the devices list any further.
  result = DescriptionsFromProvider(std::move(descriptions));
  EXPECT_TRUE(DescriptionsAreEqual(result, expected_descriptions));
}
