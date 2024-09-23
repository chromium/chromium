// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_AUDIO_AUDIO_DEVICES_PREF_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_AUDIO_AUDIO_DEVICES_PREF_HANDLER_H_

#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "chromeos/ash/components/audio/audio_device.h"
#include "chromeos/ash/components/audio/audio_pref_observer.h"

namespace ash {

struct AudioDevice;

// Interface that handles audio preference related work, reads and writes
// audio preferences, and notifies AudioPrefObserver for audio preference
// changes.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_AUDIO) AudioDevicesPrefHandler
    : public base::RefCountedThreadSafe<AudioDevicesPrefHandler> {
 public:
  static constexpr double kDefaultInputGainPercent = 50;
  static constexpr double kDefaultOutputVolumePercent = 75;
  static constexpr double kDefaultHdmiOutputVolumePercent = 100;
  static constexpr double kDefaultBluetoothOutputVolumePercent = 25;
  static constexpr double kDefaultUsbOutputVolumePercent = 25;

  // Gets the audio output volume value from prefs for a device. Since we can
  // only have either a gain or a volume for a device (depending on whether it
  // is input or output), we don't really care which value it is.
  virtual double GetOutputVolumeValue(const AudioDevice* device) = 0;
  virtual double GetInputGainValue(const AudioDevice* device) = 0;
  // Sets the audio volume or gain value to prefs for a device.
  virtual void SetVolumeGainValue(const AudioDevice& device, double value) = 0;

  // Reads the audio mute value from prefs for a device.
  virtual bool GetMuteValue(const AudioDevice& device) = 0;
  // Sets the audio mute value to prefs for a device.
  virtual void SetMuteValue(const AudioDevice& device, bool mute_on) = 0;

  // Reads whether input noise cancellation is on from profile prefs.
  virtual bool GetNoiseCancellationState() = 0;
  // Sets the input noise cancellation in profile prefs.
  virtual void SetNoiseCancellationState(bool noise_cancellation_state) = 0;

  // Reads whether input style transfer is on from profile prefs.
  virtual bool GetStyleTransferState() const = 0;
  // Sets the input style transfer in profile prefs.
  virtual void SetStyleTransferState(bool style_transfer_state) = 0;

  // Sets the device active state in prefs.
  // Note: |activate_by_user| indicates whether |device| is set to active
  // by user or by priority, and it only matters when |active| is true.
  virtual void SetDeviceActive(const AudioDevice& device,
                               bool active,
                               bool activate_by_user) = 0;
  // Returns false if it fails to get device active state from prefs.
  // Otherwise, returns true, pass the active state data in |*active|
  // and |*activate_by_user|.
  // Note: |*activate_by_user| only matters when |*active| is true.
  virtual bool GetDeviceActive(const AudioDevice& device,
                               bool* active,
                               bool* activate_by_user) = 0;

  // Sets the user priority of `target` to be one level higher
  // than `base`.
  // Given the user priorities ranking as: [.., target, ..., base, A, B]
  // After applying this function, the new ranking of user
  // priorities will be [.., base, target, A, B].
  // If both target and base have kUserPriorityNone,
  // set the target's user priority to kUserPriorityMin.
  // If base is nullptr, assign a minimal priority to `target`.
  // Do nothing if target already has a higher user priority.
  virtual void SetUserPriorityHigherThan(const AudioDevice& target,
                                         const AudioDevice* base) = 0;
  // Reads the user priority from prefs.
  virtual int GetUserPriority(const AudioDevice& device) = 0;

  // Gets the preferred device stable id given a set of devices from prefs.
  virtual const std::optional<uint64_t> GetPreferredDeviceFromPreferenceSet(
      bool is_input,
      const AudioDeviceList& devices) = 0;

  // Set |preferred_device| as the preferred device among a set of |devices|.
  virtual void UpdateDevicePreferenceSet(
      const AudioDeviceList& devices,
      const AudioDevice& preferred_device) = 0;

  // Gets the preferred device stable id given a set of devices from prefs.
  virtual const base::Value::List& GetMostRecentActivatedDeviceIdList(
      bool is_input) = 0;

  virtual void UpdateMostRecentActivatedDeviceIdList(
      const AudioDevice& device) = 0;

  // Reads the audio output allowed value from prefs.
  virtual bool GetAudioOutputAllowedValue() const = 0;

  // Adds an audio preference observer.
  virtual void AddAudioPrefObserver(AudioPrefObserver* observer) = 0;
  // Removes an audio preference observer.
  virtual void RemoveAudioPrefObserver(AudioPrefObserver* observer) = 0;

  // Mark `connected_devices` as seen and drop the least recently seen devices
  // if there are more than `keep_devices` stored in preferences.
  virtual void DropLeastRecentlySeenDevices(
      const std::vector<AudioDevice>& connected_devices,
      size_t keep_devices) = 0;

  // Reads whether input force respect ui gains is on from profile prefs.
  virtual bool GetForceRespectUiGainsState() = 0;
  // Sets the input force respect ui gains in profile prefs.
  virtual void SetForceRespectUiGainsState(bool force_respect_ui_gains) = 0;

  // Reads whether hfp_mic_sr is on from profile prefs.
  virtual bool GetHfpMicSrState() = 0;
  // Sets the hfp_mic_sr in profile prefs.
  virtual void SetHfpMicSrState(bool hfp_mic_sr_state) = 0;

 protected:
  virtual ~AudioDevicesPrefHandler() = default;

 private:
  friend class base::RefCountedThreadSafe<AudioDevicesPrefHandler>;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_AUDIO_AUDIO_DEVICES_PREF_HANDLER_H_
