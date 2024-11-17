// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/audio/audio_device_metrics_handler.h"

#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/components/audio/audio_device_encoding.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"

namespace ash {

namespace {

// The time threshold whether user override system decision metrics would be
// fired or not. This is the time delta since the system decision was triggered.
constexpr int kUserOverrideSystemDecisionTimeThresholdInMinutes = 60;

}  // namespace

AudioDeviceMetricsHandler::AudioDeviceMetricsHandler() = default;
AudioDeviceMetricsHandler::~AudioDeviceMetricsHandler() = default;

void AudioDeviceMetricsHandler::MaybeRecordSystemSwitchDecisionAndContext(
    bool is_input,
    bool has_alternative_device,
    bool is_switched,
    const AudioDeviceMap& audio_devices_,
    const AudioDeviceMap& previous_audio_devices_) {
  if (is_input) {
    AudioDeviceList input_devices =
        CrasAudioHandler::GetSimpleUsageAudioDevices(audio_devices_,
                                                     /*is_input=*/true);

    // Do not record if there is only one audio device since it will definitely
    // be activated. The metric aims to measure how well the system selection
    // works when there are more than one available devices.
    if (!has_alternative_device || input_devices.size() <= 1) {
      // Reset timestamp since no interested system selection decision is made
      // and to prevent previous system decision from being used to record the
      // user override.
      ResetSystemSwitchTimestamp(is_input);
      return;
    }

    uint32_t input_devices_bits = EncodeAudioDeviceSet(input_devices);
    AudioDeviceList previous_input_devices =
        CrasAudioHandler::GetSimpleUsageAudioDevices(previous_audio_devices_,
                                                     /*is_input=*/true);
    uint32_t previous_input_devices_bits =
        EncodeAudioDeviceSet(previous_input_devices);

    // Do not record system decision metrics since the device set doesn't
    // change. No interested system selection decision in this case. This could
    // happen when cras lost the active device, or cras fires extra node change
    // signal.
    if (input_devices_bits == previous_input_devices_bits) {
      // Reset timestamp since no interested system selection decision is made
      // and to prevent previous system decision from being used to record the
      // user override.
      ResetSystemSwitchTimestamp(is_input);
      return;
    }

    base::UmaHistogramBoolean(kSystemSwitchInputAudio, is_switched);
    base::UmaHistogramEnumeration(
        kAudioSelectionPerformance,
        is_switched ? AudioSelectionEvents::kSystemSwitchInput
                    : AudioSelectionEvents::kSystemNotSwitchInput);

    // Record the number of audio devices at the moment.
    base::UmaHistogramExactLinear(is_switched
                                      ? kSystemSwitchInputAudioDeviceCount
                                      : kSystemNotSwitchInputAudioDeviceCount,
                                  input_devices.size(), kMaxAudioDevicesCount);

    // Record the encoded device set.
    base::UmaHistogramSparse(is_switched ? kSystemSwitchInputAudioDeviceSet
                                         : kSystemNotSwitchInputAudioDeviceSet,
                             input_devices_bits);

    // Record the before and after encoded device sets.
    uint32_t before_and_after_input_device_set_bits =
        EncodeBeforeAndAfterAudioDeviceSets(previous_input_devices,
                                            input_devices);
    base::UmaHistogramSparse(
        is_switched ? kSystemSwitchInputBeforeAndAfterAudioDeviceSet
                    : kSystemNotSwitchInputBeforeAndAfterAudioDeviceSet,
        before_and_after_input_device_set_bits);

    // Record chrome restarts related metrics.
    RecordAudioSelectionMetricsSeparatedByChromeRestarts(
        /*is_input=*/true, is_switched, is_chrome_restarts_,
        /*previous_device_list=*/previous_input_devices,
        /*current_device_list=*/input_devices);

    // Set up timestamp. Make sure setting one timestamp will reset the other,
    // since only one decision can be made either switching or not switching.
    input_switched_by_system_at_ =
        is_switched ? std::make_optional(base::TimeTicks::Now()) : std::nullopt;
    input_not_switched_by_system_at_ =
        is_switched ? std::nullopt : std::make_optional(base::TimeTicks::Now());
    is_system_decision_at_chrome_restarts_ = is_chrome_restarts_;
    before_and_after_input_device_set_bits_ =
        before_and_after_input_device_set_bits;
  } else {
    AudioDeviceList output_devices =
        CrasAudioHandler::GetSimpleUsageAudioDevices(audio_devices_,
                                                     /*is_input=*/false);

    // Do not record if there is only one audio device. Same as above.
    if (!has_alternative_device || output_devices.size() <= 1) {
      // Reset timestamp. Same as above.
      ResetSystemSwitchTimestamp(is_input);
      return;
    }

    uint32_t output_devices_bits = EncodeAudioDeviceSet(output_devices);
    AudioDeviceList previous_output_devices =
        CrasAudioHandler::GetSimpleUsageAudioDevices(previous_audio_devices_,
                                                     /*is_input=*/false);
    uint32_t previous_output_devices_bits =
        EncodeAudioDeviceSet(previous_output_devices);

    // Do not record system decision metrics since the device set doesn't
    // change. No interested system selection decision in this case. This could
    // happen when cras lost the active device, or cras fires extra node change
    // signal.
    if (output_devices_bits == previous_output_devices_bits) {
      // Reset timestamp since no interested system selection decision is made
      // and to prevent previous system decision from being used to record the
      // user override.
      ResetSystemSwitchTimestamp(is_input);
      return;
    }

    base::UmaHistogramBoolean(kSystemSwitchOutputAudio, is_switched);
    base::UmaHistogramEnumeration(
        kAudioSelectionPerformance,
        is_switched ? AudioSelectionEvents::kSystemSwitchOutput
                    : AudioSelectionEvents::kSystemNotSwitchOutput);

    // Record the number of audio devices at the moment.
    base::UmaHistogramExactLinear(is_switched
                                      ? kSystemSwitchOutputAudioDeviceCount
                                      : kSystemNotSwitchOutputAudioDeviceCount,
                                  output_devices.size(), kMaxAudioDevicesCount);

    // Record the encoded device set.
    base::UmaHistogramSparse(is_switched ? kSystemSwitchOutputAudioDeviceSet
                                         : kSystemNotSwitchOutputAudioDeviceSet,
                             output_devices_bits);

    // Record the before and after encoded device sets.
    uint32_t before_and_after_output_device_set_bits =
        EncodeBeforeAndAfterAudioDeviceSets(previous_output_devices,
                                            output_devices);
    base::UmaHistogramSparse(
        is_switched ? kSystemSwitchOutputBeforeAndAfterAudioDeviceSet
                    : kSystemNotSwitchOutputBeforeAndAfterAudioDeviceSet,
        before_and_after_output_device_set_bits);

    // Record chrome restarts related metrics.
    RecordAudioSelectionMetricsSeparatedByChromeRestarts(
        /*is_input=*/false, is_switched, is_chrome_restarts_,
        /*previous_device_list=*/previous_output_devices,
        /*current_device_list=*/output_devices);

    // Set up timestamp. Make sure setting one timestamp will reset the other,
    // same as above.
    output_switched_by_system_at_ =
        is_switched ? std::make_optional(base::TimeTicks::Now()) : std::nullopt;
    output_not_switched_by_system_at_ =
        is_switched ? std::nullopt : std::make_optional(base::TimeTicks::Now());
    is_system_decision_at_chrome_restarts_ = is_chrome_restarts_;
    before_and_after_output_device_set_bits_ =
        before_and_after_output_device_set_bits;
  }
}

void AudioDeviceMetricsHandler::ResetSystemSwitchTimestamp(bool is_input) {
  if (is_input) {
    input_switched_by_system_at_ = std::nullopt;
    input_not_switched_by_system_at_ = std::nullopt;
  } else {
    output_switched_by_system_at_ = std::nullopt;
    output_not_switched_by_system_at_ = std::nullopt;
  }
}

void AudioDeviceMetricsHandler::RecordUserSwitchAudioDevice(bool is_input) {
  if (is_input) {
    base::RecordAction(base::UserMetricsAction(kUserActionSwitchInput));
    if (!input_device_selected_by_user_) {
      base::RecordAction(
          base::UserMetricsAction(kUserActionSwitchInputOverridden));
    }

    MaybeRecordUserOverrideSystemDecision(
        is_input, is_system_decision_at_chrome_restarts_,
        input_switched_by_system_at_, input_not_switched_by_system_at_);
  } else {
    base::RecordAction(base::UserMetricsAction(kUserActionSwitchOutput));
    if (!output_device_selected_by_user_) {
      base::RecordAction(
          base::UserMetricsAction(kUserActionSwitchOutputOverridden));
    }

    MaybeRecordUserOverrideSystemDecision(
        is_input, is_system_decision_at_chrome_restarts_,
        output_switched_by_system_at_, output_not_switched_by_system_at_);
  }
}

void AudioDeviceMetricsHandler::MaybeRecordUserOverrideSystemDecision(
    bool is_input,
    bool is_system_decision_at_chrome_restarts,
    std::optional<base::TimeTicks>& switched_by_system_at,
    std::optional<base::TimeTicks>& not_switched_by_system_at) {
  if (switched_by_system_at.has_value()) {
    // There should be only one decision made by system, either switching or not
    // switching the audio device.
    CHECK(!not_switched_by_system_at.has_value());

    const std::string& histogram_name_switched =
        is_input ? kUserOverrideSystemSwitchInputAudio
                 : kUserOverrideSystemSwitchOutputAudio;
    int time_delta_since_system_decision =
        (base::TimeTicks::Now() - switched_by_system_at.value()).InMinutes();
    AudioSelectionEvents audio_selection_event =
        is_input ? AudioSelectionEvents::kUserOverrideSystemSwitchInput
                 : AudioSelectionEvents::kUserOverrideSystemSwitchOutput;
    RecordUserOverrideMetricsHelper(histogram_name_switched,
                                    audio_selection_event,
                                    time_delta_since_system_decision);

    // Record user override metrics separated by chrome restarts.
    RecordUserOverrideMetricsSeparatedByChromeRestarts(
        is_input, /*is_switched=*/true,
        /*is_chrome_restarts=*/is_system_decision_at_chrome_restarts,
        time_delta_since_system_decision);

    // Record the before and after encoded device sets.
    base::UmaHistogramSparse(
        is_input ? kUserOverrideSystemSwitchInputBeforeAndAfterAudioDeviceSet
                 : kUserOverrideSystemSwitchOutputBeforeAndAfterAudioDeviceSet,
        is_input ? before_and_after_input_device_set_bits_
                 : before_and_after_output_device_set_bits_);

    // Reset the system_switch timestamp since user has activated an audio
    // device now. User activating again is not considered overriding system
    // decision, thus not recorded.
    switched_by_system_at = std::nullopt;
  } else if (not_switched_by_system_at.has_value()) {
    // There should be only one decision made by system, same as above.
    CHECK(!switched_by_system_at.has_value());

    const std::string& histogram_name_not_switched =
        is_input ? kUserOverrideSystemNotSwitchInputAudio
                 : kUserOverrideSystemNotSwitchOutputAudio;
    int time_delta_since_system_decision =
        (base::TimeTicks::Now() - not_switched_by_system_at.value())
            .InMinutes();
    AudioSelectionEvents audio_selection_event =
        is_input ? AudioSelectionEvents::kUserOverrideSystemNotSwitchInput
                 : AudioSelectionEvents::kUserOverrideSystemNotSwitchOutput;
    RecordUserOverrideMetricsHelper(histogram_name_not_switched,
                                    audio_selection_event,
                                    time_delta_since_system_decision);

    // Record user override metrics separated by chrome restarts.
    RecordUserOverrideMetricsSeparatedByChromeRestarts(
        is_input, /*is_switched=*/false,
        /*is_chrome_restarts=*/is_system_decision_at_chrome_restarts,
        time_delta_since_system_decision);

    // Record the before and after encoded device sets.
    base::UmaHistogramSparse(
        is_input
            ? kUserOverrideSystemNotSwitchInputBeforeAndAfterAudioDeviceSet
            : kUserOverrideSystemNotSwitchOutputBeforeAndAfterAudioDeviceSet,
        is_input ? before_and_after_input_device_set_bits_
                 : before_and_after_output_device_set_bits_);

    // Reset the system_not_switch timestamp since user has activated an audio
    // device now.
    not_switched_by_system_at = std::nullopt;
  }
}

void AudioDeviceMetricsHandler::
    RecordAudioSelectionMetricsSeparatedByChromeRestarts(
        bool is_input,
        bool is_switched,
        bool is_chrome_restarts,
        const AudioDeviceList& previous_device_list,
        const AudioDeviceList& current_device_list) const {
  std::string system_switch_histogram_name;
  std::string device_count_histogram_name;
  std::string device_set_histogram_name;
  std::string before_and_after_device_set_histogram_name;
  AudioSelectionEvents audio_selection_event;

  if (is_chrome_restarts) {
    system_switch_histogram_name = is_input
                                       ? kSystemSwitchInputAudioChromeRestarts
                                       : kSystemSwitchOutputAudioChromeRestarts;

    if (is_switched) {
      audio_selection_event =
          is_input ? AudioSelectionEvents::kSystemSwitchInputChromeRestart
                   : AudioSelectionEvents::kSystemSwitchOutputChromeRestart;

      device_count_histogram_name =
          is_input ? kSystemSwitchInputAudioDeviceCountChromeRestarts
                   : kSystemSwitchOutputAudioDeviceCountChromeRestarts;
      device_set_histogram_name =
          is_input ? kSystemSwitchInputAudioDeviceSetChromeRestarts
                   : kSystemSwitchOutputAudioDeviceSetChromeRestarts;
      before_and_after_device_set_histogram_name =
          is_input
              ? kSystemSwitchInputBeforeAndAfterAudioDeviceSetChromeRestarts
              : kSystemSwitchOutputBeforeAndAfterAudioDeviceSetChromeRestarts;
    } else {
      audio_selection_event =
          is_input ? AudioSelectionEvents::kSystemNotSwitchInputChromeRestart
                   : AudioSelectionEvents::kSystemNotSwitchOutputChromeRestart;
      device_count_histogram_name =
          is_input ? kSystemNotSwitchInputAudioDeviceCountChromeRestarts
                   : kSystemNotSwitchOutputAudioDeviceCountChromeRestarts;
      device_set_histogram_name =
          is_input ? kSystemNotSwitchInputAudioDeviceSetChromeRestarts
                   : kSystemNotSwitchOutputAudioDeviceSetChromeRestarts;
      before_and_after_device_set_histogram_name =
          is_input
              ? kSystemNotSwitchInputBeforeAndAfterAudioDeviceSetChromeRestarts
              : kSystemNotSwitchOutputBeforeAndAfterAudioDeviceSetChromeRestarts;
    }
  } else {
    system_switch_histogram_name =
        is_input ? kSystemSwitchInputAudioNonChromeRestarts
                 : kSystemSwitchOutputAudioNonChromeRestarts;

    if (is_switched) {
      audio_selection_event =
          is_input ? AudioSelectionEvents::kSystemSwitchInputNonChromeRestart
                   : AudioSelectionEvents::kSystemSwitchOutputNonChromeRestart;
      device_count_histogram_name =
          is_input ? kSystemSwitchInputAudioDeviceCountNonChromeRestarts
                   : kSystemSwitchOutputAudioDeviceCountNonChromeRestarts;
      device_set_histogram_name =
          is_input ? kSystemSwitchInputAudioDeviceSetNonChromeRestarts
                   : kSystemSwitchOutputAudioDeviceSetNonChromeRestarts;
      before_and_after_device_set_histogram_name =
          is_input
              ? kSystemSwitchInputBeforeAndAfterAudioDeviceSetNonChromeRestarts
              : kSystemSwitchOutputBeforeAndAfterAudioDeviceSetNonChromeRestarts;
    } else {
      audio_selection_event =
          is_input
              ? AudioSelectionEvents::kSystemNotSwitchInputNonChromeRestart
              : AudioSelectionEvents::kSystemNotSwitchOutputNonChromeRestart;
      device_count_histogram_name =
          is_input ? kSystemNotSwitchInputAudioDeviceCountNonChromeRestarts
                   : kSystemNotSwitchOutputAudioDeviceCountNonChromeRestarts;
      device_set_histogram_name =
          is_input ? kSystemNotSwitchInputAudioDeviceSetNonChromeRestarts
                   : kSystemNotSwitchOutputAudioDeviceSetNonChromeRestarts;
      before_and_after_device_set_histogram_name =
          is_input
              ? kSystemNotSwitchInputBeforeAndAfterAudioDeviceSetNonChromeRestarts
              : kSystemNotSwitchOutputBeforeAndAfterAudioDeviceSetNonChromeRestarts;
    }
  }

  // Record the system switch decision.
  base::UmaHistogramBoolean(system_switch_histogram_name, is_switched);

  base::UmaHistogramEnumeration(kAudioSelectionPerformance,
                                audio_selection_event);

  // Record the number of audio devices.
  base::UmaHistogramExactLinear(device_count_histogram_name,
                                current_device_list.size(),
                                kMaxAudioDevicesCount);

  // Record the encoded device set.
  base::UmaHistogramSparse(device_set_histogram_name,
                           EncodeAudioDeviceSet(current_device_list));

  // Record the before and after encoded device sets.
  base::UmaHistogramSparse(before_and_after_device_set_histogram_name,
                           EncodeBeforeAndAfterAudioDeviceSets(
                               previous_device_list, current_device_list));
}

void AudioDeviceMetricsHandler::RecordUserOverrideMetricsHelper(
    std::string_view histogram_name,
    AudioSelectionEvents audio_selection_event,
    int time_delta_since_system_decision) const {
  base::UmaHistogramCustomCounts(
      histogram_name.data(), time_delta_since_system_decision,
      kMinTimeInMinuteOfUserOverrideSystemDecision,
      base::Hours(kMaxTimeInHourOfUserOverrideSystemDecision).InMinutes(),
      kUserOverrideSystemDecisionTimeDeltaBucketCount);

  // The user override system decision metrics are only recorded within a time
  // threshold of system making the decision. This is because the metrics aim to
  // measure how well the system makes the decision. The shorter the user
  // overrides the system decision, the more likely that the system didn't make
  // a good decision. Current threshold is a reasonable value picked based on
  // the existing metrics (UserOverrideSystemSwitchTimeElapsed).
  if (time_delta_since_system_decision <=
      kUserOverrideSystemDecisionTimeThresholdInMinutes) {
    base::UmaHistogramEnumeration(kAudioSelectionPerformance,
                                  audio_selection_event);
  }
}

void AudioDeviceMetricsHandler::
    RecordUserOverrideMetricsSeparatedByChromeRestarts(
        bool is_input,
        bool is_switched,
        bool is_chrome_restarts,
        int time_delta_since_system_decision) const {
  std::string user_override_histogram_name;
  AudioSelectionEvents audio_selection_event;
  if (is_chrome_restarts) {
    if (is_switched) {
      user_override_histogram_name =
          is_input ? kUserOverrideSystemSwitchInputAudioChromeRestarts
                   : kUserOverrideSystemSwitchOutputAudioChromeRestarts;
      audio_selection_event =
          is_input ? AudioSelectionEvents::
                         kUserOverrideSystemSwitchInputChromeRestart
                   : AudioSelectionEvents::
                         kUserOverrideSystemSwitchOutputChromeRestart;
    } else {
      user_override_histogram_name =
          is_input ? kUserOverrideSystemNotSwitchInputAudioChromeRestarts
                   : kUserOverrideSystemNotSwitchOutputAudioChromeRestarts;

      audio_selection_event =
          is_input ? AudioSelectionEvents::
                         kUserOverrideSystemNotSwitchInputChromeRestart
                   : AudioSelectionEvents::
                         kUserOverrideSystemNotSwitchOutputChromeRestart;
    }
  } else {
    if (is_switched) {
      user_override_histogram_name =
          is_input ? kUserOverrideSystemSwitchInputAudioNonChromeRestarts
                   : kUserOverrideSystemSwitchOutputAudioNonChromeRestarts;
      audio_selection_event =
          is_input ? AudioSelectionEvents::
                         kUserOverrideSystemSwitchInputNonChromeRestart
                   : AudioSelectionEvents::
                         kUserOverrideSystemSwitchOutputNonChromeRestart;
    } else {
      user_override_histogram_name =
          is_input ? kUserOverrideSystemNotSwitchInputAudioNonChromeRestarts
                   : kUserOverrideSystemNotSwitchOutputAudioNonChromeRestarts;
      audio_selection_event =
          is_input ? AudioSelectionEvents::
                         kUserOverrideSystemNotSwitchInputNonChromeRestart
                   : AudioSelectionEvents::
                         kUserOverrideSystemNotSwitchOutputNonChromeRestart;
    }
  }

  RecordUserOverrideMetricsHelper(user_override_histogram_name,
                                  audio_selection_event,
                                  time_delta_since_system_decision);
}

void AudioDeviceMetricsHandler::RecordConsecutiveAudioDevicsChangeTimeElapsed(
    bool is_input,
    bool is_device_added) {
  base::TimeTicks now = base::TimeTicks::Now();
  std::optional<base::TimeTicks>& devices_changed_at =
      is_input ? input_devices_changed_at_ : output_devices_changed_at_;
  if (devices_changed_at.has_value()) {
    int time_delta_since_system_decision =
        (now - devices_changed_at.value()).InSeconds();
    base::UmaHistogramSparse(is_input ? kConsecutiveInputDevicsChanged
                                      : kConsecutiveOutputDevicsChanged,
                             time_delta_since_system_decision);
  }
  devices_changed_at = now;

  if (is_device_added) {
    std::optional<base::TimeTicks>& devices_added_at =
        is_input ? input_devices_added_at_ : output_devices_added_at_;
    if (devices_added_at.has_value()) {
      int time_delta_since_system_decision =
          (now - devices_added_at.value()).InSeconds();
      base::UmaHistogramSparse(is_input ? kConsecutiveInputDevicsAdded
                                        : kConsecutiveOutputDevicsAdded,
                               time_delta_since_system_decision);
    }
    devices_added_at = now;
  }
}

void AudioDeviceMetricsHandler::RecordExceptionRulesMet(
    AudioSelectionExceptionRules rule) {
  base::UmaHistogramEnumeration(kAudioSelectionExceptionRuleMetrics, rule);
}

void AudioDeviceMetricsHandler::RecordNotificationEvents(
    AudioSelectionNotificationEvents notification_event) {
  base::UmaHistogramEnumeration(kAudioSelectionNotification,
                                notification_event);
}

}  // namespace ash
