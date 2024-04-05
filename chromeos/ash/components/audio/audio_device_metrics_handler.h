// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_AUDIO_AUDIO_DEVICE_METRICS_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_AUDIO_AUDIO_DEVICE_METRICS_HANDLER_H_

#include <string_view>

#include "base/component_export.h"
#include "base/time/time.h"
#include "chromeos/ash/components/audio/audio_device.h"

namespace ash {

// AudioDeviceMetricsHandler handles the firing of cras audio related histogram
// metrics.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_AUDIO)
    AudioDeviceMetricsHandler {
 public:
  AudioDeviceMetricsHandler();
  AudioDeviceMetricsHandler(const AudioDeviceMetricsHandler&) = delete;
  AudioDeviceMetricsHandler& operator=(const AudioDeviceMetricsHandler&) =
      delete;
  ~AudioDeviceMetricsHandler();

  // Minimum/maximum bucket value of user overriding system decision of
  // switching or not switching audio device.
  static constexpr int kMinTimeInMinuteOfUserOverrideSystemDecision = 1;
  static constexpr int kMaxTimeInHourOfUserOverrideSystemDecision = 8;

  // The histogram bucket count of user overriding system decision.
  static constexpr int kUserOverrideSystemDecisionTimeDeltaBucketCount = 100;

  // Maximum number of connected input or output audio devices to record
  // histogram metrics.
  static constexpr uint32_t kMaxAudioDevicesCount = 10;

  // A series of user action metrics to record when user switches the
  // input/output audio device and if this switch overrides the system decision.
  static constexpr char kUserActionSwitchInput[] =
      "StatusArea_Audio_SwitchInputDevice";
  static constexpr char kUserActionSwitchOutput[] =
      "StatusArea_Audio_SwitchOutputDevice";
  static constexpr char kUserActionSwitchInputOverridden[] =
      "StatusArea_Audio_AutoInputSelectionOverridden";
  static constexpr char kUserActionSwitchOutputOverridden[] =
      "StatusArea_Audio_AutoOutputSelectionOverridden";

  // A series of histogram metrics to record system selection decision after
  // audio device has changed. And the time delta if user has overridden the
  // system selection afterwards.
  static constexpr char kSystemSwitchInputAudio[] =
      "ChromeOS.AudioSelection.Input.SystemSwitchAudio";
  static constexpr char kSystemSwitchOutputAudio[] =
      "ChromeOS.AudioSelection.Output.SystemSwitchAudio";
  static constexpr char kUserOverrideSystemSwitchInputAudio[] =
      "ChromeOS.AudioSelection.Input.UserOverrideSystemSwitchTimeElapsed";
  static constexpr char kUserOverrideSystemSwitchOutputAudio[] =
      "ChromeOS.AudioSelection.Output.UserOverrideSystemSwitchTimeElapsed";
  static constexpr char kUserOverrideSystemNotSwitchInputAudio[] =
      "ChromeOS.AudioSelection.Input.UserOverrideSystemNotSwitchTimeElapsed";
  static constexpr char kUserOverrideSystemNotSwitchOutputAudio[] =
      "ChromeOS.AudioSelection.Output.UserOverrideSystemNotSwitchTimeElapsed";

  // A series of histogram metrics to record the audio device count when the
  // system selection decision is made after audio device has changed.
  static constexpr char kSystemSwitchInputAudioDeviceCount[] =
      "ChromeOS.AudioSelection.Input.SystemSwitchAudio.AudioDeviceCount";
  static constexpr char kSystemNotSwitchInputAudioDeviceCount[] =
      "ChromeOS.AudioSelection.Input.SystemNotSwitchAudio.AudioDeviceCount";
  static constexpr char kSystemSwitchOutputAudioDeviceCount[] =
      "ChromeOS.AudioSelection.Output.SystemSwitchAudio.AudioDeviceCount";
  static constexpr char kSystemNotSwitchOutputAudioDeviceCount[] =
      "ChromeOS.AudioSelection.Output.SystemNotSwitchAudio.AudioDeviceCount";

  // A series of histogram metrics to record the audio device types when the
  // system selection decision is made after audio device has changed.
  static constexpr char kSystemSwitchInputAudioDeviceSet[] =
      "ChromeOS.AudioSelection.Input.SystemSwitchAudio.AudioDeviceSet";
  static constexpr char kSystemNotSwitchInputAudioDeviceSet[] =
      "ChromeOS.AudioSelection.Input.SystemNotSwitchAudio.AudioDeviceSet";
  static constexpr char kSystemSwitchOutputAudioDeviceSet[] =
      "ChromeOS.AudioSelection.Output.SystemSwitchAudio.AudioDeviceSet";
  static constexpr char kSystemNotSwitchOutputAudioDeviceSet[] =
      "ChromeOS.AudioSelection.Output.SystemNotSwitchAudio.AudioDeviceSet";

  // A series of histogram metrics to record the before and after condition
  // of audio device types when the system selection decision is made after
  // audio device has changed.
  static constexpr char kSystemSwitchInputBeforeAndAfterAudioDeviceSet[] =
      "ChromeOS.AudioSelection.Input.SystemSwitchAudio."
      "BeforeAndAfterAudioDeviceSet";
  static constexpr char kSystemNotSwitchInputBeforeAndAfterAudioDeviceSet[] =
      "ChromeOS.AudioSelection.Input.SystemNotSwitchAudio."
      "BeforeAndAfterAudioDeviceSet";
  static constexpr char kSystemSwitchOutputBeforeAndAfterAudioDeviceSet[] =
      "ChromeOS.AudioSelection.Output.SystemSwitchAudio."
      "BeforeAndAfterAudioDeviceSet";
  static constexpr char kSystemNotSwitchOutputBeforeAndAfterAudioDeviceSet[] =
      "ChromeOS.AudioSelection.Output.SystemNotSwitchAudio."
      "BeforeAndAfterAudioDeviceSet";

  // A series of histogram metrics to record system selection decision after
  // audio device has changed.
  static constexpr char kSystemSwitchInputAudioChromeRestarts[] =
      "ChromeOS.AudioSelection.Input.SystemSwitchAudio.ChromeRestarts";
  static constexpr char kSystemSwitchOutputAudioChromeRestarts[] =
      "ChromeOS.AudioSelection.Output.SystemSwitchAudio.ChromeRestarts";
  static constexpr char kSystemSwitchInputAudioNonChromeRestarts[] =
      "ChromeOS.AudioSelection.Input.SystemSwitchAudio.NonChromeRestarts";
  static constexpr char kSystemSwitchOutputAudioNonChromeRestarts[] =
      "ChromeOS.AudioSelection.Output.SystemSwitchAudio.NonChromeRestarts";

  // A series of histogram metrics to record the time delta if user has
  // overridden the system selection afterwards.
  static constexpr char kUserOverrideSystemSwitchInputAudioChromeRestarts[] =
      "ChromeOS.AudioSelection.Input.UserOverrideSystemSwitchTimeElapsed."
      "ChromeRestarts";
  static constexpr char kUserOverrideSystemSwitchOutputAudioChromeRestarts[] =
      "ChromeOS.AudioSelection.Output.UserOverrideSystemSwitchTimeElapsed."
      "ChromeRestarts";
  static constexpr char kUserOverrideSystemNotSwitchInputAudioChromeRestarts[] =
      "ChromeOS.AudioSelection.Input.UserOverrideSystemNotSwitchTimeElapsed."
      "ChromeRestarts";
  static constexpr char
      kUserOverrideSystemNotSwitchOutputAudioChromeRestarts[] =
          "ChromeOS.AudioSelection.Output."
          "UserOverrideSystemNotSwitchTimeElapsed.ChromeRestarts";
  static constexpr char kUserOverrideSystemSwitchInputAudioNonChromeRestarts[] =
      "ChromeOS.AudioSelection.Input.UserOverrideSystemSwitchTimeElapsed."
      "NonChromeRestarts";
  static constexpr char
      kUserOverrideSystemSwitchOutputAudioNonChromeRestarts[] =
          "ChromeOS.AudioSelection.Output.UserOverrideSystemSwitchTimeElapsed."
          "NonChromeRestarts";
  static constexpr char
      kUserOverrideSystemNotSwitchInputAudioNonChromeRestarts[] =
          "ChromeOS.AudioSelection.Input."
          "UserOverrideSystemNotSwitchTimeElapsed.NonChromeRestarts";
  static constexpr char
      kUserOverrideSystemNotSwitchOutputAudioNonChromeRestarts[] =
          "ChromeOS.AudioSelection.Output."
          "UserOverrideSystemNotSwitchTimeElapsed.NonChromeRestarts";

  // A series of histogram metrics to record the audio device count when the
  // system selection decision is made after audio device has changed, separated
  // by chrome restarts or not.
  static constexpr char kSystemSwitchInputAudioDeviceCountChromeRestarts[] =
      "ChromeOS.AudioSelection.Input.SystemSwitchAudio.AudioDeviceCount."
      "ChromeRestarts";
  static constexpr char kSystemNotSwitchInputAudioDeviceCountChromeRestarts[] =
      "ChromeOS.AudioSelection.Input.SystemNotSwitchAudio.AudioDeviceCount."
      "ChromeRestarts";
  static constexpr char kSystemSwitchOutputAudioDeviceCountChromeRestarts[] =
      "ChromeOS.AudioSelection.Output.SystemSwitchAudio.AudioDeviceCount."
      "ChromeRestarts";
  static constexpr char kSystemNotSwitchOutputAudioDeviceCountChromeRestarts[] =
      "ChromeOS.AudioSelection.Output.SystemNotSwitchAudio.AudioDeviceCount."
      "ChromeRestarts";
  static constexpr char kSystemSwitchInputAudioDeviceCountNonChromeRestarts[] =
      "ChromeOS.AudioSelection.Input.SystemSwitchAudio.AudioDeviceCount."
      "NonChromeRestarts";
  static constexpr char
      kSystemNotSwitchInputAudioDeviceCountNonChromeRestarts[] =
          "ChromeOS.AudioSelection.Input.SystemNotSwitchAudio.AudioDeviceCount."
          "NonChromeRestarts";
  static constexpr char kSystemSwitchOutputAudioDeviceCountNonChromeRestarts[] =
      "ChromeOS.AudioSelection.Output.SystemSwitchAudio.AudioDeviceCount."
      "NonChromeRestarts";
  static constexpr char
      kSystemNotSwitchOutputAudioDeviceCountNonChromeRestarts[] =
          "ChromeOS.AudioSelection.Output.SystemNotSwitchAudio."
          "AudioDeviceCount.NonChromeRestarts";

  // A series of histogram metrics to record the audio device types when the
  // system selection decision is made after audio device has changed, separated
  // by chrome restarts or not.
  static constexpr char kSystemSwitchInputAudioDeviceSetChromeRestarts[] =
      "ChromeOS.AudioSelection.Input.SystemSwitchAudio.AudioDeviceSet."
      "ChromeRestarts";
  static constexpr char kSystemNotSwitchInputAudioDeviceSetChromeRestarts[] =
      "ChromeOS.AudioSelection.Input.SystemNotSwitchAudio.AudioDeviceSet."
      "ChromeRestarts";
  static constexpr char kSystemSwitchOutputAudioDeviceSetChromeRestarts[] =
      "ChromeOS.AudioSelection.Output.SystemSwitchAudio.AudioDeviceSet."
      "ChromeRestarts";
  static constexpr char kSystemNotSwitchOutputAudioDeviceSetChromeRestarts[] =
      "ChromeOS.AudioSelection.Output.SystemNotSwitchAudio.AudioDeviceSet."
      "ChromeRestarts";
  static constexpr char kSystemSwitchInputAudioDeviceSetNonChromeRestarts[] =
      "ChromeOS.AudioSelection.Input.SystemSwitchAudio.AudioDeviceSet."
      "NonChromeRestarts";
  static constexpr char kSystemNotSwitchInputAudioDeviceSetNonChromeRestarts[] =
      "ChromeOS.AudioSelection.Input.SystemNotSwitchAudio.AudioDeviceSet."
      "NonChromeRestarts";
  static constexpr char kSystemSwitchOutputAudioDeviceSetNonChromeRestarts[] =
      "ChromeOS.AudioSelection.Output.SystemSwitchAudio.AudioDeviceSet."
      "NonChromeRestarts";
  static constexpr char
      kSystemNotSwitchOutputAudioDeviceSetNonChromeRestarts[] =
          "ChromeOS.AudioSelection.Output.SystemNotSwitchAudio.AudioDeviceSet."
          "NonChromeRestarts";

  // A series of histogram metrics to record the before and after condition
  // of audio device types when the system selection decision is made after
  // audio device has changed, separated
  // by chrome restarts or not.
  static constexpr char
      kSystemSwitchInputBeforeAndAfterAudioDeviceSetChromeRestarts[] =
          "ChromeOS.AudioSelection.Input.SystemSwitchAudio."
          "BeforeAndAfterAudioDeviceSet.ChromeRestarts";
  static constexpr char
      kSystemNotSwitchInputBeforeAndAfterAudioDeviceSetChromeRestarts[] =
          "ChromeOS.AudioSelection.Input.SystemNotSwitchAudio."
          "BeforeAndAfterAudioDeviceSet.ChromeRestarts";
  static constexpr char
      kSystemSwitchOutputBeforeAndAfterAudioDeviceSetChromeRestarts[] =
          "ChromeOS.AudioSelection.Output.SystemSwitchAudio."
          "BeforeAndAfterAudioDeviceSet.ChromeRestarts";
  static constexpr char
      kSystemNotSwitchOutputBeforeAndAfterAudioDeviceSetChromeRestarts[] =
          "ChromeOS.AudioSelection.Output.SystemNotSwitchAudio."
          "BeforeAndAfterAudioDeviceSet.ChromeRestarts";
  static constexpr char
      kSystemSwitchInputBeforeAndAfterAudioDeviceSetNonChromeRestarts[] =
          "ChromeOS.AudioSelection.Input.SystemSwitchAudio."
          "BeforeAndAfterAudioDeviceSet.NonChromeRestarts";
  static constexpr char
      kSystemNotSwitchInputBeforeAndAfterAudioDeviceSetNonChromeRestarts[] =
          "ChromeOS.AudioSelection.Input.SystemNotSwitchAudio."
          "BeforeAndAfterAudioDeviceSet.NonChromeRestarts";
  static constexpr char
      kSystemSwitchOutputBeforeAndAfterAudioDeviceSetNonChromeRestarts[] =
          "ChromeOS.AudioSelection.Output.SystemSwitchAudio."
          "BeforeAndAfterAudioDeviceSet.NonChromeRestarts";
  static constexpr char
      kSystemNotSwitchOutputBeforeAndAfterAudioDeviceSetNonChromeRestarts[] =
          "ChromeOS.AudioSelection.Output.SystemNotSwitchAudio."
          "BeforeAndAfterAudioDeviceSet.NonChromeRestarts";

  // A series of histogram metrics to record consecutive devices change event
  // time elapsed.
  static constexpr char kConsecutiveInputDevicsChanged[] =
      "ChromeOS.AudioSelection.Input.ConsecutiveDevicesChangeTimeElapsed.Any";
  static constexpr char kConsecutiveOutputDevicsChanged[] =
      "ChromeOS.AudioSelection.Output.ConsecutiveDevicesChangeTimeElapsed.Any";
  static constexpr char kConsecutiveInputDevicsAdded[] =
      "ChromeOS.AudioSelection.Input.ConsecutiveDevicesChangeTimeElapsed.Add";
  static constexpr char kConsecutiveOutputDevicsAdded[] =
      "ChromeOS.AudioSelection.Output.ConsecutiveDevicesChangeTimeElapsed.Add";

  // Record the histogram of system decision of switching or not switching after
  // audio device is added or removed. Only record if there are more than one
  // available devices.
  void MaybeRecordSystemSwitchDecisionAndContext(
      bool is_input,
      bool has_alternative_device,
      bool is_switched,
      const AudioDeviceMap& audio_devices_,
      const AudioDeviceMap& previous_audio_devices_);

  // Record metrics when user switches audio device.
  void RecordUserSwitchAudioDevice(bool is_input);

  // Record system selection related metrics in the case of chrome restarts,
  // including system boots and users sign out, as well as the case of normal
  // user hotplug or unplug.
  void RecordAudioSelectionMetricsSeparatedByChromeRestarts(
      bool is_input,
      bool is_switched,
      bool is_chrome_restarts,
      const AudioDeviceList& previous_device_list,
      const AudioDeviceList& current_device_list) const;

  // Record user overrides system decision metrics.
  void RecordUserOverrideMetrics(const std::string_view histogram_name,
                                 int time_delta) const;

  // Record user overrides system decision metrics in the case of chrome
  // restarts, including system boots and users sign out, as well as the case of
  // normal user hotplug or unplug.
  void RecordUserOverrideMetricsSeparatedByChromeRestarts(
      bool is_input,
      bool is_switched,
      bool is_chrome_restarts,
      int time_delta) const;

  // Record the time elaspsed between two consecutive devices change event,
  // including devices added/removed and devices changed.
  void RecordConsecutiveAudioDevicsChangeTimeElapsed(bool is_input,
                                                     bool is_device_added);

  void set_is_chrome_restarts(bool is_chrome_restarts) {
    is_chrome_restarts_ = is_chrome_restarts;
  }

  void set_input_device_selected_by_user(bool input_device_selected_by_user) {
    input_device_selected_by_user_ = input_device_selected_by_user;
  }

  void set_output_device_selected_by_user(bool output_device_selected_by_user) {
    output_device_selected_by_user_ = output_device_selected_by_user;
  }

 private:
  // Clear the timer of system switch/not switch decision.
  void ResetSystemSwitchTimestamp(bool is_input);

  // Maybe record the histogram metrics of user overriding system decision of
  // switching or not switching audio device. Do not record if user doesn't
  // override system decision but override previous user action.
  void MaybeRecordUserOverrideSystemDecision(
      bool is_input,
      bool is_system_decision_at_chrome_restarts,
      std::optional<base::TimeTicks>& switched_by_system_at,
      std::optional<base::TimeTicks>& not_switched_by_system_at);

  // The timestamp for recording the metrics of user overriding system decision
  // of switching or not switching the active audio device.
  std::optional<base::TimeTicks> input_switched_by_system_at_ = std::nullopt;
  std::optional<base::TimeTicks> input_not_switched_by_system_at_ =
      std::nullopt;
  std::optional<base::TimeTicks> output_switched_by_system_at_ = std::nullopt;
  std::optional<base::TimeTicks> output_not_switched_by_system_at_ =
      std::nullopt;

  // The timestamp when devices have changed, including devices added/removed
  // and devices changed. Used for recording the time elaspsed between two
  // consecutive devices change event.
  std::optional<base::TimeTicks> input_devices_changed_at_ = std::nullopt;
  std::optional<base::TimeTicks> output_devices_changed_at_ = std::nullopt;
  std::optional<base::TimeTicks> input_devices_added_at_ = std::nullopt;
  std::optional<base::TimeTicks> output_devices_added_at_ = std::nullopt;

  // Whether the audio device was selected by user, to track user overrides
  bool input_device_selected_by_user_ = false;
  bool output_device_selected_by_user_ = false;

  // A boolean flag used to tell if system audio selection happens for the first
  // time when system boots or chrome restarts.
  bool is_chrome_restarts_ = true;

  // A boolean indicating if the system makes the switch or not switch decision
  // in the case of chrome restarts. Used for recording user override histogram
  // metrics.
  bool is_system_decision_at_chrome_restarts_ = true;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_AUDIO_AUDIO_DEVICE_METRICS_HANDLER_H_
