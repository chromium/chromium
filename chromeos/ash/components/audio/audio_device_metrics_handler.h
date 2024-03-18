// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_AUDIO_AUDIO_DEVICE_METRICS_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_AUDIO_AUDIO_DEVICE_METRICS_HANDLER_H_

#include "base/component_export.h"
#include "chromeos/ash/components/audio/audio_device.h"

namespace ash {

// AudioDeviceMetricsHandler handles the firing of cras audio related histogram
// metrics.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_AUDIO)
    AudioDeviceMetricsHandler {
 public:
  AudioDeviceMetricsHandler() = default;
  AudioDeviceMetricsHandler(const AudioDeviceMetricsHandler&) = delete;
  AudioDeviceMetricsHandler& operator=(const AudioDeviceMetricsHandler&) =
      delete;
  ~AudioDeviceMetricsHandler() = default;

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

  // Record system selection related metrics in the case of chrome restarts,
  // including system boots and users sign out, as well as the case of normal
  // user hotplug or unplug.
  void RecordAudioSelectionMetricsSeparatedByChromeRestarts(
      bool is_input,
      bool is_switched,
      bool is_chrome_restarts,
      const AudioDeviceList& previous_device_list,
      const AudioDeviceList& current_device_list) const;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_AUDIO_AUDIO_DEVICE_METRICS_HANDLER_H_
