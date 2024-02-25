// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_ASSISTANT_PLATFORM_AUDIO_DEVICES_H_
#define CHROMEOS_ASH_SERVICES_ASSISTANT_PLATFORM_AUDIO_DEVICES_H_

#include <cstdint>
#include <optional>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation_traits.h"
#include "chromeos/ash/components/audio/audio_device.h"

namespace ash {

class CrasAudioHandler;

namespace assistant {

// This class will monitor the available audio devices (through
// |CrasAudioHandler|), and select the devices to use for audio input (both
// regular input and hotword detection).
// When the selected devices change, this class will:
//     - Inform the observers.
//     - Find the hotword model to use, and send it to
//       CrasAudioHandler::SetHotwordModel().
class COMPONENT_EXPORT(ASSISTANT_SERVICE) AudioDevices {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Set the input device to use for audio capture.
    virtual void SetDeviceId(const std::optional<std::string>& device_id) = 0;
    // Set the input device to use for hardware based hotword detection.
    virtual void SetHotwordDeviceId(
        const std::optional<std::string>& device_id) = 0;
  };

  AudioDevices(CrasAudioHandler* cras_audio_handler, const std::string& locale);
  AudioDevices(const AudioDevices&) = delete;
  AudioDevices& operator=(const AudioDevices&) = delete;
  ~AudioDevices();

  void AddAndFireObserver(Observer*);
  void RemoveObserver(Observer*);

  void SetLocale(const std::string& locale);

  // Used during unittests to simulate an update to the list of available audio
  // devices.
  void SetAudioDevicesForTest(const AudioDeviceList& audio_devices);

 private:
  class ScopedCrasAudioHandlerObserver;
  class HotwordModelUpdater;

  void SetAudioDevices(const AudioDeviceList& audio_devices);
  void UpdateHotwordDeviceId(const AudioDeviceList& devices);
  void UpdateDeviceId(const AudioDeviceList& devices);
  void UpdateHotwordModel();

  // Handles the asynchronous nature of sending a new hotword model to
  // |cras_audio_handler_|.
  std::unique_ptr<HotwordModelUpdater> hotword_model_updater_;

  base::ObserverList<Observer> observers_;

  // Owned by |AssistantManagerServiceImpl|.
  const raw_ptr<CrasAudioHandler> cras_audio_handler_;

  std::string locale_;
  std::optional<uint64_t> hotword_device_id_;
  std::optional<uint64_t> device_id_;

  // Observes changes to the available audio devices, and sends the list of
  // devices to SetAudioDevices().
  std::unique_ptr<ScopedCrasAudioHandlerObserver>
      scoped_cras_audio_handler_observer_;
};

}  // namespace assistant
}  // namespace ash

namespace base {

template <>
struct ScopedObservationTraits<ash::assistant::AudioDevices,
                               ash::assistant::AudioDevices::Observer> {
  static void AddObserver(ash::assistant::AudioDevices* source,
                          ash::assistant::AudioDevices::Observer* observer) {
    source->AddAndFireObserver(observer);
  }
  static void RemoveObserver(ash::assistant::AudioDevices* source,
                             ash::assistant::AudioDevices::Observer* observer) {
    source->RemoveObserver(observer);
  }
};

}  // namespace base

#endif  // CHROMEOS_ASH_SERVICES_ASSISTANT_PLATFORM_AUDIO_DEVICES_H_
