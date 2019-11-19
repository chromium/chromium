// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PLATFORM_AUDIO_INPUT_IMPL_H_
#define CHROMEOS_SERVICES_ASSISTANT_PLATFORM_AUDIO_INPUT_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/scoped_observer.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "libassistant/shared/public/platform_audio_input.h"
#include "media/base/audio_capturer_source.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace chromeos {
class CrasAudioHandler;

namespace assistant {

class COMPONENT_EXPORT(ASSISTANT_SERVICE) AudioInputImpl
    : public assistant_client::AudioInput,
      public media::AudioCapturerSource::CaptureCallback,
      public chromeos::PowerManagerClient::Observer {
 public:
  AudioInputImpl(mojom::Client* client,
                 PowerManagerClient* power_manager_client,
                 CrasAudioHandler* cras_audio_handler,
                 const std::string& device_id);
  ~AudioInputImpl() override;

  class HotwordStateManager {
   public:
    explicit HotwordStateManager(AudioInputImpl* audio_input_);
    virtual ~HotwordStateManager() = default;
    virtual void OnConversationTurnStarted() {}
    virtual void OnConversationTurnFinished() {}
    virtual void OnCaptureDataArrived() {}
    virtual void RecreateAudioInputStream();

   protected:
    AudioInputImpl* input_;

   private:
    DISALLOW_COPY_AND_ASSIGN(HotwordStateManager);
  };

  void RecreateStateManager();

  // media::AudioCapturerSource::CaptureCallback overrides:
  void Capture(const media::AudioBus* audio_source,
               base::TimeTicks audio_capture_time,
               double volume,
               bool key_pressed) override;
  void OnCaptureError(const std::string& message) override;
  void OnCaptureMuted(bool is_muted) override;

  // assistant_client::AudioInput overrides. These function are called by
  // assistant from assistant thread, for which we should not assume any
  // //base related thread context to be in place.
  assistant_client::BufferFormat GetFormat() const override;
  void AddObserver(assistant_client::AudioInput::Observer* observer) override;
  void RemoveObserver(
      assistant_client::AudioInput::Observer* observer) override;

  // chromeos::PowerManagerClient::Observer overrides:
  void LidEventReceived(chromeos::PowerManagerClient::LidState state,
                        const base::TimeTicks& timestamp) override;

  // Called when the mic state associated with the interaction is changed.
  void SetMicState(bool mic_open);
  void OnConversationTurnStarted();
  void OnConversationTurnFinished();

  // Called when hotword enabled status changed.
  void OnHotwordEnabled(bool enable);

  void SetDeviceId(const std::string& device_id);
  void SetHotwordDeviceId(const std::string& device_id);
  void SetDspHotwordLocale(std::string pref_locale);
  void SetDspHotwordLocaleCallback(bool success);

  void RecreateAudioInputStream(bool use_dsp);

  bool IsHotwordAvailable() const;

  // Returns the recording state used in unittests.
  bool IsRecordingForTesting() const;
  // Returns if the hotword device is used for recording now.
  bool IsUsingHotwordDeviceForTesting() const;

 private:
  void StartRecording();
  void StopRecording();
  void UpdateRecordingState();

  // Updates lid state from received switch states.
  void OnSwitchStatesReceived(
      base::Optional<chromeos::PowerManagerClient::SwitchStates> switch_states);

  scoped_refptr<media::AudioCapturerSource> source_;

  // User explicitly requested to open microphone.
  bool mic_open_ = false;

  // Whether hotword is currently enabled.
  bool hotword_enabled_ = true;

  // Guards observers_;
  base::Lock lock_;
  std::vector<assistant_client::AudioInput::Observer*> observers_;

  // This is the total number of frames captured during the life time of this
  // object. We don't worry about overflow because this count is only used for
  // logging purposes. If in the future this changes, we should re-evaluate.
  int captured_frames_count_ = 0;
  base::TimeTicks last_frame_count_report_time_;

  // To be initialized on assistant thread the first call to AddObserver.
  // It ensures that AddObserver / RemoveObserver are called on the same
  // sequence.
  SEQUENCE_CHECKER(observer_sequence_checker_);

  mojom::Client* const client_;

  chromeos::PowerManagerClient* power_manager_client_;
  ScopedObserver<chromeos::PowerManagerClient,
                 chromeos::PowerManagerClient::Observer>
      power_manager_client_observer_;

  CrasAudioHandler* const cras_audio_handler_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  std::unique_ptr<HotwordStateManager> state_manager_;

  // Preferred audio input device which will be used for capture.
  std::string preferred_device_id_;
  // Hotword input device used for hardware based hotword detection.
  std::string hotword_device_id_;
  // Device currently being used for recording.
  std::string device_id_;

  chromeos::PowerManagerClient::LidState lid_state_ =
      chromeos::PowerManagerClient::LidState::NOT_PRESENT;

  base::WeakPtrFactory<AudioInputImpl> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(AudioInputImpl);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PLATFORM_AUDIO_INPUT_IMPL_H_
