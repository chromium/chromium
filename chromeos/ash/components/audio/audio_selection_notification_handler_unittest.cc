// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/audio/audio_selection_notification_handler.h"

#include <optional>

#include "ash/strings/grit/ash_strings.h"
#include "base/test/metrics/histogram_tester.h"
#include "chromeos/ash/components/audio/audio_device.h"
#include "chromeos/ash/components/audio/audio_device_selection_test_base.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

class AudioSelectionNotificationHandlerTest
    : public AudioDeviceSelectionTestBase {
 public:
  void SetUp() override { message_center::MessageCenter::Initialize(); }

  void TearDown() override { message_center::MessageCenter::Shutdown(); }

  const base::HistogramTester& histogram_tester() { return histogram_tester_; }

  static void SwitchToDevice(const AudioDevice& device,
                             bool notify,
                             DeviceActivateType activate_by) {
    CrasAudioHandler::Get()->SwitchToDevice(device, notify, activate_by);
  }

  static void OpenSettingsAudioPage() {
    CrasAudioHandler::Get()->OpenSettingsAudioPage();
  }

  AudioSelectionNotificationHandler& audio_selection_notification_handler() {
    return audio_selection_notification_handler_;
  }

  bool AudioNodesBelongToSameSource(const AudioDevice& input_device,
                                    const AudioDevice& output_device) {
    return audio_selection_notification_handler_.AudioNodesBelongToSameSource(
        input_device, output_device);
  }

  void FakeSwitchToDevice(const AudioDevice& device,
                          bool notify,
                          DeviceActivateType activate_by) {
    if (device.is_input) {
      active_input_id_ = device.id;
    } else {
      active_output_id_ = device.id;
    }
  }

  void HandleSwitchButtonClicked(
      const AudioDeviceList& devices_to_activate,
      AudioSelectionNotificationHandler::NotificationType notification_type,
      std::optional<int> button_index) {
    audio_selection_notification_handler_.HandleSwitchButtonClicked(
        devices_to_activate,
        base::BindRepeating(
            &AudioSelectionNotificationHandlerTest::FakeSwitchToDevice,
            weak_ptr_factory_.GetWeakPtr()),
        notification_type, button_index);
  }

  void FakeOpenSettingsPage() { settings_audio_page_opened_ = true; }

  void HandleSettingsButtonClicked(std::optional<int> button_index) {
    audio_selection_notification_handler_.HandleSettingsButtonClicked(
        base::BindRepeating(
            &AudioSelectionNotificationHandlerTest::FakeOpenSettingsPage,
            weak_ptr_factory_.GetWeakPtr()),
        button_index);
  }

  // Gets the count of audio selection notification.
  size_t GetNotificationCount() {
    auto* message_center = message_center::MessageCenter::Get();
    return message_center->NotificationCount();
  }

  // Gets the title of audio selection notification. If not found, return
  // std::nullopt.
  const std::optional<std::u16string> GetNotificationTitle() {
    auto* message_center = message_center::MessageCenter::Get();
    message_center::Notification* notification =
        message_center->FindNotificationById(
            AudioSelectionNotificationHandler::kAudioSelectionNotificationId);
    return notification ? std::make_optional(notification->title())
                        : std::nullopt;
  }

  // Gets the message of audio selection notification. If not found, return
  // std::nullopt.
  const std::optional<std::u16string> GetNotificationMessage() {
    auto* message_center = message_center::MessageCenter::Get();
    message_center::Notification* notification =
        message_center->FindNotificationById(
            AudioSelectionNotificationHandler::kAudioSelectionNotificationId);
    return notification ? std::make_optional(notification->message())
                        : std::nullopt;
  }

  uint64_t active_input_id() { return active_input_id_; }

  uint64_t active_output_id() { return active_output_id_; }

  bool settings_audio_page_opened() { return settings_audio_page_opened_; }

 private:
  AudioSelectionNotificationHandler audio_selection_notification_handler_;

  // Initialized with an invalid id, to fix an issue in build: MemorySanitizer:
  // use-of-uninitialized-value.
  uint64_t active_input_id_ = 0;
  uint64_t active_output_id_ = 0;

  base::HistogramTester histogram_tester_;

  bool settings_audio_page_opened_ = false;

  base::WeakPtrFactory<AudioSelectionNotificationHandlerTest> weak_ptr_factory_{
      this};
};

TEST_F(AudioSelectionNotificationHandlerTest, ShowAudioSelectionNotification) {
  EXPECT_EQ(0u, GetNotificationCount());

  AudioDeviceList hotplug_input_devices = {
      AudioDevice(NewInputNode("INTERNAL_MIC"))};
  AudioDeviceList hotplug_output_devices = {
      AudioDevice(NewOutputNode("INTERNAL_SPEAKER"))};

  audio_selection_notification_handler().ShowAudioSelectionNotification(
      hotplug_input_devices, hotplug_output_devices, std::nullopt, std::nullopt,
      base::BindRepeating(
          &AudioSelectionNotificationHandlerTest::SwitchToDevice),
      base::BindRepeating(
          &AudioSelectionNotificationHandlerTest::OpenSettingsAudioPage));

  // Expect notification is shown.
  FastForwardBy(AudioSelectionNotificationHandler::kDebounceTime);
  EXPECT_EQ(1u, GetNotificationCount());

  // Expect new notification to replace the old one and the current notification
  // count does not change.
  hotplug_input_devices = {AudioDevice(NewInputNode("MIC"))};
  hotplug_output_devices = {AudioDevice(NewOutputNode("HEADPHONE"))};
  audio_selection_notification_handler().ShowAudioSelectionNotification(
      hotplug_input_devices, hotplug_output_devices, std::nullopt, std::nullopt,
      base::BindRepeating(
          &AudioSelectionNotificationHandlerTest::SwitchToDevice),
      base::BindRepeating(
          &AudioSelectionNotificationHandlerTest::OpenSettingsAudioPage));
  EXPECT_EQ(1u, GetNotificationCount());
}

// Tests that AudioNodesBelongToSameSource can tell if one audio input device
// and one audio output device belong to the same physical audio device.
TEST_F(AudioSelectionNotificationHandlerTest, AudioNodesBelongToSameSource) {
  struct {
    const AudioDevice input_device;
    const AudioDevice output_device;
    bool same_source;
  } items[] = {
      {AudioDevice(NewInputNode("INTERNAL_MIC")),
       AudioDevice(NewOutputNode("INTERNAL_SPEAKER")), true},
      {AudioDevice(NewInputNode("FRONT_MIC")),
       AudioDevice(NewOutputNode("INTERNAL_SPEAKER")), true},
      {AudioDevice(NewInputNode("REAR_MIC")),
       AudioDevice(NewOutputNode("INTERNAL_SPEAKER")), true},
      {AudioDevice(NewInputNode("MIC")),
       AudioDevice(NewOutputNode("HEADPHONE")), true},
      {AudioDevice(NewInputNode("BLUETOOTH_NB_MIC")),
       AudioDevice(NewOutputNode("BLUETOOTH")), true},
      {AudioDevice(NewNodeWithName(/*is_input=*/true, "USB",
                                   "Razer USB Sound Card: USB Audio:2,0: Mic")),
       AudioDevice(
           NewNodeWithName(/*is_input=*/false, "USB",
                           "Razer USB Sound Card: USB Audio:2,0: Speaker")),
       true},
      {AudioDevice(NewNodeWithName(/*is_input=*/true, "BLUETOOTH", "Airpods")),
       AudioDevice(NewNodeWithName(/*is_input=*/false, "BLUETOOTH", "Airpods")),
       true},
      // Audio devices with different types do not belong to the same physical
      // device.
      {AudioDevice(NewInputNode("INTERNAL_MIC")),
       AudioDevice(NewOutputNode("HEADPHONE")), false},
      // Audio devices with different types do not belong to the same physical
      // device.
      {AudioDevice(NewInputNode("BLUETOOTH")),
       AudioDevice(NewOutputNode("HDMI")), false},
      // Audio devices with different types do not belong to the same physical
      // device.
      {AudioDevice(NewInputNode("USB")), AudioDevice(NewOutputNode("HDMI")),
       false},
      // Audio devices with different types do not belong to the same physical
      // device.
      {AudioDevice(NewInputNode("BLUETOOTH")),
       AudioDevice(NewOutputNode("USB")), false},
      // Audio devices with different device source names do not belong to the
      // same physical device.
      {AudioDevice(
           NewNodeWithName(/*is_input=*/true, "BLUETOOTH", "Airpods Pro")),
       AudioDevice(NewNodeWithName(/*is_input=*/false, "BLUETOOTH", "Airpods")),
       false},
      // Audio devices with different device source names do not belong to the
      // same physical device.
      {AudioDevice(NewNodeWithName(/*is_input=*/true, "USB",
                                   "Razer USB Sound Card: USB Audio:2,0: Mic")),
       AudioDevice(NewNodeWithName(/*is_input=*/false, "USB",
                                   "CS201 USB AUDIO: USB Audio:2,0: PCM")),
       false},
  };

  for (const auto& item : items) {
    EXPECT_EQ(item.same_source, AudioNodesBelongToSameSource(
                                    item.input_device, item.output_device));
  }
}

// Tests audio selection notification with input only displays correctly.
TEST_F(AudioSelectionNotificationHandlerTest,
       Notification_SingleSourceWithInputOnly) {
  EXPECT_EQ(0u, GetNotificationCount());

  // Plug a web cam input.
  const std::string input_device_name =
      "HD Pro Webcam C920: USB Audio:2,0: Mic";
  AudioDeviceList hotplug_input_devices = {AudioDevice(NewNodeWithName(
      /*is_input=*/true, "USB", input_device_name))};
  AudioDeviceList hotplug_output_devices = {};

  // No notification event metrics are fired before showing notification.
  histogram_tester().ExpectTotalCount(
      AudioDeviceMetricsHandler::kAudioSelectionNotification, 0);

  audio_selection_notification_handler().ShowAudioSelectionNotification(
      hotplug_input_devices, hotplug_output_devices, std::nullopt, std::nullopt,
      base::BindRepeating(
          &AudioSelectionNotificationHandlerTest::SwitchToDevice),
      base::BindRepeating(
          &AudioSelectionNotificationHandlerTest::OpenSettingsAudioPage));
  FastForwardBy(AudioSelectionNotificationHandler::kDebounceTime);
  EXPECT_EQ(1u, GetNotificationCount());
  std::optional<std::u16string> title = GetNotificationTitle();
  EXPECT_TRUE(title.has_value());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_AUDIO_SELECTION_SWITCH_INPUT_TITLE),
      title.value());
  std::optional<std::u16string> message = GetNotificationMessage();
  EXPECT_TRUE(message.has_value());
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_AUDIO_SELECTION_SWITCH_INPUT_OR_OUTPUT_BODY,
                base::UTF8ToUTF16(input_device_name)),
            message.value());

  // Notification event metrics are fired after showing notification.
  histogram_tester().ExpectTotalCount(
      AudioDeviceMetricsHandler::kAudioSelectionNotification, 1);
  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionNotification,
      AudioDeviceMetricsHandler::AudioSelectionNotificationEvents::
          kNotificationWithInputOnlyDeviceShowsUp,
      1);

  // Clicking switch button, expect clicking notification event is fired.
  HandleSwitchButtonClicked(hotplug_input_devices,
                            AudioSelectionNotificationHandler::
                                NotificationType::kSingleSourceWithInputOnly,
                            /*button_index=*/1);

  // Clicking notification event metrics are fired after clicking switch button.
  histogram_tester().ExpectTotalCount(
      AudioDeviceMetricsHandler::kAudioSelectionNotification, 2);
  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionNotification,
      AudioDeviceMetricsHandler::AudioSelectionNotificationEvents::
          kNotificationWithInputOnlyDeviceClicked,
      1);
}

// Tests audio selection notification with output only displays correctly.
TEST_F(AudioSelectionNotificationHandlerTest,
       Notification_SingleSourceWithOutputOnly) {
  EXPECT_EQ(0u, GetNotificationCount());

  // Plug HDMI display with audio output.
  AudioDeviceList hotplug_input_devices = {};
  const std::string output_device_name = "Sceptre Z27";
  AudioDeviceList hotplug_output_devices = {AudioDevice(NewNodeWithName(
      /*is_input=*/false, "HDMI", output_device_name))};

  // No notification event metrics are fired before showing notification.
  histogram_tester().ExpectTotalCount(
      AudioDeviceMetricsHandler::kAudioSelectionNotification, 0);

  audio_selection_notification_handler().ShowAudioSelectionNotification(
      hotplug_input_devices, hotplug_output_devices, std::nullopt, std::nullopt,
      base::BindRepeating(
          &AudioSelectionNotificationHandlerTest::SwitchToDevice),
      base::BindRepeating(
          &AudioSelectionNotificationHandlerTest::OpenSettingsAudioPage));
  FastForwardBy(AudioSelectionNotificationHandler::kDebounceTime);
  EXPECT_EQ(1u, GetNotificationCount());
  std::optional<std::u16string> title = GetNotificationTitle();
  EXPECT_TRUE(title.has_value());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_AUDIO_SELECTION_SWITCH_OUTPUT_TITLE),
      title.value());
  std::optional<std::u16string> message = GetNotificationMessage();
  EXPECT_TRUE(message.has_value());
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_AUDIO_SELECTION_SWITCH_INPUT_OR_OUTPUT_BODY,
                base::UTF8ToUTF16(output_device_name)),
            message.value());

  // Notification event metrics are fired after showing notification.
  histogram_tester().ExpectTotalCount(
      AudioDeviceMetricsHandler::kAudioSelectionNotification, 1);
  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionNotification,
      AudioDeviceMetricsHandler::AudioSelectionNotificationEvents::
          kNotificationWithOutputOnlyDeviceShowsUp,
      1);

  // Clicking switch button, expect clicking notification event is fired.
  HandleSwitchButtonClicked(hotplug_output_devices,
                            AudioSelectionNotificationHandler::
                                NotificationType::kSingleSourceWithOutputOnly,
                            /*button_index=*/1);

  // Clicking notification event metrics are fired after clicking switch button.
  histogram_tester().ExpectTotalCount(
      AudioDeviceMetricsHandler::kAudioSelectionNotification, 2);
  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionNotification,
      AudioDeviceMetricsHandler::AudioSelectionNotificationEvents::
          kNotificationWithOutputOnlyDeviceClicked,
      1);
}

// Tests audio selection notification with single source and both input and
// output displays correctly.
TEST_F(AudioSelectionNotificationHandlerTest,
       Notification_SingleSourceWithBothInputAndOutput) {
  EXPECT_EQ(0u, GetNotificationCount());

  // Plug a USB input and a USB output device from the same source.
  const std::string device_source_name = "Razer USB Sound Card";
  const std::string input_device_name =
      device_source_name + ": USB Audio:2,0: Mic";
  const AudioDevice input_device = AudioDevice(NewNodeWithName(
      /*is_input=*/true, "USB", input_device_name));
  AudioDeviceList hotplug_input_devices = {input_device};
  const std::string output_device_name =
      device_source_name + ": USB Audio:2,0: Speaker";
  const AudioDevice output_device = AudioDevice(
      NewNodeWithName(/*is_input=*/false, "USB", output_device_name));
  AudioDeviceList hotplug_output_devices = {output_device};

  // No notification event metrics are fired before showing notification.
  histogram_tester().ExpectTotalCount(
      AudioDeviceMetricsHandler::kAudioSelectionNotification, 0);

  audio_selection_notification_handler().ShowAudioSelectionNotification(
      hotplug_input_devices, hotplug_output_devices, std::nullopt, std::nullopt,
      base::BindRepeating(
          &AudioSelectionNotificationHandlerTest::SwitchToDevice),
      base::BindRepeating(
          &AudioSelectionNotificationHandlerTest::OpenSettingsAudioPage));
  FastForwardBy(AudioSelectionNotificationHandler::kDebounceTime);
  EXPECT_EQ(1u, GetNotificationCount());
  std::optional<std::u16string> title = GetNotificationTitle();
  EXPECT_TRUE(title.has_value());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_AUDIO_SELECTION_SWITCH_SOURCE_TITLE),
      title.value());
  std::optional<std::u16string> message = GetNotificationMessage();
  EXPECT_TRUE(message.has_value());
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_ASH_AUDIO_SELECTION_SWITCH_INPUT_AND_OUTPUT_BODY,
                base::UTF8ToUTF16(device_source_name)),
            message.value());

  // Notification event metrics are fired after showing notification.
  histogram_tester().ExpectTotalCount(
      AudioDeviceMetricsHandler::kAudioSelectionNotification, 1);
  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionNotification,
      AudioDeviceMetricsHandler::AudioSelectionNotificationEvents::
          kNotificationWithBothInputAndOutputDevicesShowsUp,
      1);

  // Clicking switch button, expect clicking notification event is fired.
  HandleSwitchButtonClicked(
      {input_device, output_device},
      AudioSelectionNotificationHandler::NotificationType::
          kSingleSourceWithInputAndOutput,
      /*button_index=*/1);

  // Clicking notification event metrics are fired after clicking switch button.
  histogram_tester().ExpectTotalCount(
      AudioDeviceMetricsHandler::kAudioSelectionNotification, 2);
  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionNotification,
      AudioDeviceMetricsHandler::AudioSelectionNotificationEvents::
          kNotificationWithBothInputAndOutputDevicesClicked,
      1);
}

// Tests audio selection notification with multiple audio sources displays
// correctly.
TEST_F(AudioSelectionNotificationHandlerTest,
       Notification_MultipleSources_SameAudioTypes) {
  EXPECT_EQ(0u, GetNotificationCount());

  // Plug a USB input and a USB output device from different sources.
  const std::string input_device_name = "CS201 USB AUDIO: USB Audio:2,0: Mic";
  AudioDeviceList hotplug_input_devices = {AudioDevice(NewNodeWithName(
      /*is_input=*/true, "USB", input_device_name))};
  const std::string output_device_name =
      "Razer USB Sound Card: USB Audio:2,0: Speaker";
  AudioDeviceList hotplug_output_devices = {AudioDevice(
      NewNodeWithName(/*is_input=*/false, "USB", output_device_name))};
  const std::string current_active_input = "internal_mic";
  const std::string current_active_output = "internal_speaker";

  // No notification event metrics are fired before showing notification.
  histogram_tester().ExpectTotalCount(
      AudioDeviceMetricsHandler::kAudioSelectionNotification, 0);

  audio_selection_notification_handler().ShowAudioSelectionNotification(
      hotplug_input_devices, hotplug_output_devices, current_active_input,
      current_active_output,
      base::BindRepeating(
          &AudioSelectionNotificationHandlerTest::SwitchToDevice),
      base::BindRepeating(
          &AudioSelectionNotificationHandlerTest::OpenSettingsAudioPage));
  FastForwardBy(AudioSelectionNotificationHandler::kDebounceTime);
  EXPECT_EQ(1u, GetNotificationCount());
  std::optional<std::u16string> title = GetNotificationTitle();
  EXPECT_TRUE(title.has_value());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_AUDIO_SELECTION_MULTIPLE_DEVICES_TITLE),
      title.value());
  std::optional<std::u16string> message = GetNotificationMessage();
  EXPECT_TRUE(message.has_value());
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(IDS_ASH_AUDIO_SELECTION_MULTIPLE_DEVICES_BODY,
                                 base::UTF8ToUTF16(current_active_input),
                                 base::UTF8ToUTF16(current_active_output)),
      message.value());

  // Notification event metrics are fired after showing notification.
  histogram_tester().ExpectTotalCount(
      AudioDeviceMetricsHandler::kAudioSelectionNotification, 1);
  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionNotification,
      AudioDeviceMetricsHandler::AudioSelectionNotificationEvents::
          kNotificationWithMultipleSourcesDevicesShowsUp,
      1);
}

// Tests audio selection notification with multiple audio sources displays
// correctly, when current active device name not available.
TEST_F(AudioSelectionNotificationHandlerTest,
       Notification_MultipleSources_NameUnavailable) {
  EXPECT_EQ(0u, GetNotificationCount());

  // Plug a USB input and a USB output device from different sources.
  const std::string input_device_name = "CS201 USB AUDIO: USB Audio:2,0: Mic";
  AudioDeviceList hotplug_input_devices = {AudioDevice(NewNodeWithName(
      /*is_input=*/true, "USB", input_device_name))};
  const std::string output_device_name =
      "Razer USB Sound Card: USB Audio:2,0: Speaker";
  AudioDeviceList hotplug_output_devices = {AudioDevice(
      NewNodeWithName(/*is_input=*/false, "USB", output_device_name))};

  // No notification event metrics are fired before showing notification.
  histogram_tester().ExpectTotalCount(
      AudioDeviceMetricsHandler::kAudioSelectionNotification, 0);

  audio_selection_notification_handler().ShowAudioSelectionNotification(
      hotplug_input_devices, hotplug_output_devices,
      /*active_input_device_name=*/std::nullopt,
      /*active_output_device_name=*/std::nullopt,
      base::BindRepeating(
          &AudioSelectionNotificationHandlerTest::SwitchToDevice),
      base::BindRepeating(
          &AudioSelectionNotificationHandlerTest::OpenSettingsAudioPage));
  FastForwardBy(AudioSelectionNotificationHandler::kDebounceTime);
  EXPECT_EQ(1u, GetNotificationCount());
  std::optional<std::u16string> title = GetNotificationTitle();
  EXPECT_TRUE(title.has_value());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_AUDIO_SELECTION_MULTIPLE_DEVICES_TITLE),
      title.value());
  std::optional<std::u16string> message = GetNotificationMessage();
  EXPECT_TRUE(message.has_value());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(
          IDS_ASH_AUDIO_SELECTION_MULTIPLE_DEVICES_BODY_WITH_NAME_UNAVAILABLE),
      message.value());

  // Notification event metrics are fired after showing notification.
  histogram_tester().ExpectTotalCount(
      AudioDeviceMetricsHandler::kAudioSelectionNotification, 1);
  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionNotification,
      AudioDeviceMetricsHandler::AudioSelectionNotificationEvents::
          kNotificationWithMultipleSourcesDevicesShowsUp,
      1);
}

// Tests audio selection notification with multiple audio sources displays
// correctly.
TEST_F(AudioSelectionNotificationHandlerTest,
       Notification_MultipleSources_DifferentAudioTypes) {
  EXPECT_EQ(0u, GetNotificationCount());

  // Plug a USB input and a USB output device from different sources.
  const std::string input_device_name =
      "HD Pro Webcam C920: USB Audio:2,0: Mic";
  AudioDeviceList hotplug_input_devices = {AudioDevice(NewNodeWithName(
      /*is_input=*/true, "USB", input_device_name))};
  const std::string output_device_name = "Sceptre Z27";
  AudioDeviceList hotplug_output_devices = {AudioDevice(NewNodeWithName(
      /*is_input=*/false, "HDMI", output_device_name))};
  const std::string current_active_input = "internal_mic";
  const std::string current_active_output = "internal_speaker";

  // No notification event metrics are fired before showing notification.
  histogram_tester().ExpectTotalCount(
      AudioDeviceMetricsHandler::kAudioSelectionNotification, 0);

  audio_selection_notification_handler().ShowAudioSelectionNotification(
      hotplug_input_devices, hotplug_output_devices, current_active_input,
      current_active_output,
      base::BindRepeating(
          &AudioSelectionNotificationHandlerTest::SwitchToDevice),
      base::BindRepeating(
          &AudioSelectionNotificationHandlerTest::OpenSettingsAudioPage));
  FastForwardBy(AudioSelectionNotificationHandler::kDebounceTime);
  EXPECT_EQ(1u, GetNotificationCount());
  std::optional<std::u16string> title = GetNotificationTitle();
  EXPECT_TRUE(title.has_value());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_AUDIO_SELECTION_MULTIPLE_DEVICES_TITLE),
      title.value());
  std::optional<std::u16string> message = GetNotificationMessage();
  EXPECT_TRUE(message.has_value());
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(IDS_ASH_AUDIO_SELECTION_MULTIPLE_DEVICES_BODY,
                                 base::UTF8ToUTF16(current_active_input),
                                 base::UTF8ToUTF16(current_active_output)),
      message.value());

  // Notification event metrics are fired after showing notification.
  histogram_tester().ExpectTotalCount(
      AudioDeviceMetricsHandler::kAudioSelectionNotification, 1);
  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionNotification,
      AudioDeviceMetricsHandler::AudioSelectionNotificationEvents::
          kNotificationWithMultipleSourcesDevicesShowsUp,
      1);
}

// Tests clicking switch button on notification should activate the device.
TEST_F(AudioSelectionNotificationHandlerTest, HandleSwitchButtonClicked) {
  EXPECT_EQ(0u, GetNotificationCount());

  // Plug HTMI display with audio output.
  AudioDeviceList hotplug_input_devices = {};
  const std::string output_device_name = "Sceptre Z27";
  const AudioDevice output_hdmi = AudioDevice(NewNodeWithName(
      /*is_input=*/false, "HDMI", output_device_name));
  AudioDeviceList hotplug_output_devices = {output_hdmi};
  audio_selection_notification_handler().ShowAudioSelectionNotification(
      hotplug_input_devices, hotplug_output_devices, std::nullopt, std::nullopt,
      base::BindRepeating(
          &AudioSelectionNotificationHandlerTest::SwitchToDevice),
      base::BindRepeating(
          &AudioSelectionNotificationHandlerTest::OpenSettingsAudioPage));

  // Expect notification displays.
  FastForwardBy(AudioSelectionNotificationHandler::kDebounceTime);
  EXPECT_EQ(1u, GetNotificationCount());

  // Clicking notification body does not have any effects.
  HandleSwitchButtonClicked({output_hdmi},
                            AudioSelectionNotificationHandler::
                                NotificationType::kSingleSourceWithOutputOnly,
                            std::nullopt);
  EXPECT_NE(output_hdmi.id, active_output_id());
  EXPECT_EQ(1u, GetNotificationCount());

  // Clicking switch button, expect device being activated and notification is
  // removed.
  HandleSwitchButtonClicked({output_hdmi},
                            AudioSelectionNotificationHandler::
                                NotificationType::kSingleSourceWithOutputOnly,
                            /*button_index=*/1);

  EXPECT_EQ(output_hdmi.id, active_output_id());
  EXPECT_EQ(0u, GetNotificationCount());
}

// Tests clicking switch button on notification should fire notification event
// metrics.
TEST_F(AudioSelectionNotificationHandlerTest,
       HandleSwitchButtonClicked_OutputOnly) {
  // No clicking notification event metrics are fired before clicking switch
  // button.
  histogram_tester().ExpectTotalCount(
      AudioDeviceMetricsHandler::kAudioSelectionNotification, 0);
  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionNotification,
      AudioDeviceMetricsHandler::AudioSelectionNotificationEvents::
          kNotificationWithOutputOnlyDeviceClicked,
      0);

  // Clicking switch button, expect device being activated and notification is
  // removed.
  HandleSwitchButtonClicked({AudioDevice(NewOutputNode("HDMI"))},
                            AudioSelectionNotificationHandler::
                                NotificationType::kSingleSourceWithOutputOnly,
                            /*button_index=*/1);

  // Clicking notification event metrics are fired after clicking switch button.
  histogram_tester().ExpectTotalCount(
      AudioDeviceMetricsHandler::kAudioSelectionNotification, 1);
  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionNotification,
      AudioDeviceMetricsHandler::AudioSelectionNotificationEvents::
          kNotificationWithOutputOnlyDeviceClicked,
      1);
}

// Tests clicking switch button on notification should fire notification event
// metrics.
TEST_F(AudioSelectionNotificationHandlerTest,
       HandleSwitchButtonClicked_InputOnly) {
  // No clicking notification event metrics are fired before clicking switch
  // button.
  histogram_tester().ExpectTotalCount(
      AudioDeviceMetricsHandler::kAudioSelectionNotification, 0);
  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionNotification,
      AudioDeviceMetricsHandler::AudioSelectionNotificationEvents::
          kNotificationWithInputOnlyDeviceClicked,
      0);

  // Clicking switch button, expect device being activated and notification is
  // removed.
  HandleSwitchButtonClicked({AudioDevice(NewInputNode("USB"))},
                            AudioSelectionNotificationHandler::
                                NotificationType::kSingleSourceWithInputOnly,
                            /*button_index=*/1);

  // Clicking notification event metrics are fired after clicking switch button.
  histogram_tester().ExpectTotalCount(
      AudioDeviceMetricsHandler::kAudioSelectionNotification, 1);
  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionNotification,
      AudioDeviceMetricsHandler::AudioSelectionNotificationEvents::
          kNotificationWithInputOnlyDeviceClicked,
      1);
}

// Tests clicking switch button on notification should fire notification event
// metrics.
TEST_F(AudioSelectionNotificationHandlerTest,
       HandleSwitchButtonClicked_InputAndOutput) {
  // No clicking notification event metrics are fired before clicking switch
  // button.
  histogram_tester().ExpectTotalCount(
      AudioDeviceMetricsHandler::kAudioSelectionNotification, 0);
  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionNotification,
      AudioDeviceMetricsHandler::AudioSelectionNotificationEvents::
          kNotificationWithBothInputAndOutputDevicesClicked,
      0);

  // Clicking switch button, expect device being activated and notification is
  // removed.
  HandleSwitchButtonClicked(
      {AudioDevice(NewInputNode("USB")), AudioDevice(NewOutputNode("HDMI"))},
      AudioSelectionNotificationHandler::NotificationType::
          kSingleSourceWithInputAndOutput,
      /*button_index=*/1);

  // Clicking notification event metrics are fired after clicking switch button.
  histogram_tester().ExpectTotalCount(
      AudioDeviceMetricsHandler::kAudioSelectionNotification, 1);
  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionNotification,
      AudioDeviceMetricsHandler::AudioSelectionNotificationEvents::
          kNotificationWithBothInputAndOutputDevicesClicked,
      1);
}

// Tests audio selection notification was removed because the hotplugged input
// device is disconnected.
TEST_F(AudioSelectionNotificationHandlerTest,
       RemoveNotificationIfInputDeviceIsDisConnected) {
  EXPECT_EQ(0u, GetNotificationCount());

  // Plug a web cam input.
  const std::string input_device_name =
      "HD Pro Webcam C920: USB Audio:2,0: Mic";
  const AudioDevice input_device = AudioDevice(NewNodeWithName(
      /*is_input=*/true, "USB", input_device_name));
  AudioDeviceList hotplug_input_devices = {input_device};
  AudioDeviceList hotplug_output_devices = {};

  audio_selection_notification_handler().ShowAudioSelectionNotification(
      hotplug_input_devices, hotplug_output_devices, std::nullopt, std::nullopt,
      base::BindRepeating(
          &AudioSelectionNotificationHandlerTest::SwitchToDevice),
      base::BindRepeating(
          &AudioSelectionNotificationHandlerTest::OpenSettingsAudioPage));
  FastForwardBy(AudioSelectionNotificationHandler::kDebounceTime);
  EXPECT_EQ(1u, GetNotificationCount());

  // If a non related device is removed, notification should stay.
  const AudioDevice output_device = AudioDevice(NewOutputNode("USB"));
  audio_selection_notification_handler()
      .RemoveNotificationIfHotpluggedDeviceActivated({output_device});
  EXPECT_EQ(1u, GetNotificationCount());

  // If the device that triggers the notification is removed, notification
  // should be removed too.
  audio_selection_notification_handler()
      .RemoveNotificationIfHotpluggedDeviceActivated({input_device});
  EXPECT_EQ(0u, GetNotificationCount());
}

// Tests audio selection notification was removed because the hotplugged output
// device is disconnected.
TEST_F(AudioSelectionNotificationHandlerTest,
       RemoveNotificationIfOutputDeviceIsDisConnected) {
  EXPECT_EQ(0u, GetNotificationCount());

  // Plug a web cam input.
  const std::string output_device_name =
      "HD Pro Webcam C920: USB Audio:2,0: Speaker";
  const AudioDevice output_device = AudioDevice(NewNodeWithName(
      /*is_input=*/false, "USB", output_device_name));
  AudioDeviceList hotplug_output_devices = {output_device};
  AudioDeviceList hotplug_input_devices = {};

  audio_selection_notification_handler().ShowAudioSelectionNotification(
      hotplug_input_devices, hotplug_output_devices, std::nullopt, std::nullopt,
      base::BindRepeating(
          &AudioSelectionNotificationHandlerTest::SwitchToDevice),
      base::BindRepeating(
          &AudioSelectionNotificationHandlerTest::OpenSettingsAudioPage));
  FastForwardBy(AudioSelectionNotificationHandler::kDebounceTime);
  EXPECT_EQ(1u, GetNotificationCount());

  // If a non related device is removed, notification should stay.
  const AudioDevice input_device = AudioDevice(NewInputNode("USB"));
  audio_selection_notification_handler()
      .RemoveNotificationIfHotpluggedDeviceActivated({input_device});
  EXPECT_EQ(1u, GetNotificationCount());

  // If the device that triggers the notification is removed, notification
  // should be removed too.
  audio_selection_notification_handler()
      .RemoveNotificationIfHotpluggedDeviceActivated({output_device});
  EXPECT_EQ(0u, GetNotificationCount());
}

// Tests clicking Settings button on notification should open OS Settings audio
// page.
TEST_F(AudioSelectionNotificationHandlerTest, HandleSettingsButtonClicked) {
  EXPECT_EQ(0u, GetNotificationCount());

  // Plug HTMI display and a USB output device.
  AudioDeviceList hotplug_input_devices = {};
  const AudioDevice output_hdmi = AudioDevice(NewNodeWithName(
      /*is_input=*/false, "HDMI", "Sceptre Z27"));
  const AudioDevice output_USB = AudioDevice(NewNodeWithName(
      /*is_input=*/false, "USB", "USB output device"));
  AudioDeviceList hotplug_output_devices = {output_hdmi, output_USB};
  audio_selection_notification_handler().ShowAudioSelectionNotification(
      hotplug_input_devices, hotplug_output_devices, std::nullopt, std::nullopt,
      base::BindRepeating(
          &AudioSelectionNotificationHandlerTest::SwitchToDevice),
      base::BindRepeating(
          &AudioSelectionNotificationHandlerTest::OpenSettingsAudioPage));

  // Expect notification displays.
  FastForwardBy(AudioSelectionNotificationHandler::kDebounceTime);
  EXPECT_EQ(1u, GetNotificationCount());

  // No clicking notification event metrics are fired before clicking settings
  // button, only the notification showing up metrics are fired.
  histogram_tester().ExpectTotalCount(
      AudioDeviceMetricsHandler::kAudioSelectionNotification, 1);
  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionNotification,
      AudioDeviceMetricsHandler::AudioSelectionNotificationEvents::
          kNotificationWithMultipleSourcesDevicesShowsUp,
      1);
  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionNotification,
      AudioDeviceMetricsHandler::AudioSelectionNotificationEvents::
          kNotificationWithMultipleSourcesDevicesClicked,
      0);

  // Clicking notification body does not have any effects.
  HandleSettingsButtonClicked(std::nullopt);
  EXPECT_EQ(1u, GetNotificationCount());

  // Clicking Settings button, expect OS Settings audio page is opened and
  // notification is removed.
  HandleSettingsButtonClicked(/*button_index=*/1);

  EXPECT_TRUE(settings_audio_page_opened());
  EXPECT_EQ(0u, GetNotificationCount());

  // Clicking notification event metrics are fired after clicking settings
  // button.
  histogram_tester().ExpectTotalCount(
      AudioDeviceMetricsHandler::kAudioSelectionNotification, 2);
  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionNotification,
      AudioDeviceMetricsHandler::AudioSelectionNotificationEvents::
          kNotificationWithMultipleSourcesDevicesShowsUp,
      1);
  histogram_tester().ExpectBucketCount(
      AudioDeviceMetricsHandler::kAudioSelectionNotification,
      AudioDeviceMetricsHandler::AudioSelectionNotificationEvents::
          kNotificationWithMultipleSourcesDevicesClicked,
      1);
}

}  // namespace ash
