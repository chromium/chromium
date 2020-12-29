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
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "chromeos/services/assistant/public/cpp/assistant_service.h"
#include "libassistant/shared/public/platform_audio_input.h"
#include "media/base/audio_capturer_source.h"

namespace chromeos {
namespace assistant {

class AudioStream;
class AudioStreamFactoryDelegate;

class COMPONENT_EXPORT(ASSISTANT_SERVICE) AudioInputImpl
    : public assistant_client::AudioInput,
      public media::AudioCapturerSource::CaptureCallback {
 public:
  enum class LidState {
    kOpen,
    kClosed,
  };

  explicit AudioInputImpl(
      AudioStreamFactoryDelegate* audio_stream_factory_delegate,
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

  // Called when the mic state associated with the interaction is changed.
  void SetMicState(bool mic_open);
  void OnConversationTurnStarted();
  void OnConversationTurnFinished();

  // Called when hotword enabled status changed.
  void OnHotwordEnabled(bool enable);

  void SetDeviceId(const std::string& device_id);
  void SetHotwordDeviceId(const std::string& device_id);

  // Called when the user opens/closes the lid.
  void OnLidStateChanged(LidState new_state);

  void RecreateAudioInputStream(bool use_dsp);

  bool IsHotwordAvailable() const;

  // Returns the recording state used in unittests.
  bool IsRecordingForTesting() const;
  // Returns if the hotword device is used for recording now.
  bool IsUsingHotwordDeviceForTesting() const;
  // Returns the id of the device that is currently recording audio.
  // Returns nullopt if no audio is being recorded.
  base::Optional<std::string> GetOpenDeviceIdForTesting() const;
  // Returns if dead stream detection is being used for the current audio
  // recording. Returns nullopt if no audio is being recorded.
  base::Optional<bool> IsUsingDeadStreamDetectionForTesting() const;

 private:
  void StartRecording();
  void StopRecording();
  void UpdateRecordingState();

  std::string GetDeviceId(bool use_dsp) const;
  bool ShouldEnableDeadStreamDetection(bool use_dsp) const;

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

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  std::unique_ptr<HotwordStateManager> state_manager_;

  // It is the responsibility of the classes that own |this| to ensure
  // |audio_stream_factory_deletate| outlives |this|.
  AudioStreamFactoryDelegate* const audio_stream_factory_delegate_;

  // Preferred audio input device which will be used for capture.
  std::string preferred_device_id_;
  // Hotword input device used for hardware based hotword detection.
  std::string hotword_device_id_;

  // Currently open audio stream. nullptr if no audio stream is open.
  std::unique_ptr<AudioStream> open_audio_stream_;

  // Start with lidstate |kClosed| so we do not open the microphone before we
  // know if the lid is open or closed.
  LidState lid_state_ = LidState::kClosed;

  base::WeakPtrFactory<AudioInputImpl> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(AudioInputImpl);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PLATFORM_AUDIO_INPUT_IMPL_H_
