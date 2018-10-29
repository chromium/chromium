// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PLATFORM_AUDIO_INPUT_PROVIDER_IMPL_H_
#define CHROMEOS_SERVICES_ASSISTANT_PLATFORM_AUDIO_INPUT_PROVIDER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "libassistant/shared/public/platform_audio_input.h"
#include "media/base/audio_capturer_source.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace media {
class AudioBus;
}  // namespace media

namespace chromeos {
namespace assistant {

class AudioInputBufferImpl : public assistant_client::AudioBuffer {
 public:
  AudioInputBufferImpl(const void* data, uint32_t frame_count);
  ~AudioInputBufferImpl() override;

  // assistant_client::AudioBuffer overrides:
  assistant_client::BufferFormat GetFormat() const override;
  const void* GetData() const override;
  void* GetWritableData() override;
  int GetFrameCount() const override;

 private:
  const void* data_;
  int frame_count_;
  DISALLOW_COPY_AND_ASSIGN(AudioInputBufferImpl);
};

class AudioInputImpl : public assistant_client::AudioInput,
                       public media::AudioCapturerSource::CaptureCallback {
 public:
  AudioInputImpl(std::unique_ptr<service_manager::Connector> connector,
                 bool default_on);
  ~AudioInputImpl() override;

  // media::AudioCapturerSource::CaptureCallback overrides:
  void Capture(const media::AudioBus* audio_source,
               int audio_delay_milliseconds,
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

  // Called when hotword enabled status changed.
  void OnHotwordEnabled(bool enable);

 private:
  void StartRecording();
  void StopRecording();

  scoped_refptr<media::AudioCapturerSource> source_;

  // Should audio input always recording actively.
  bool default_on_;

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

  base::WeakPtrFactory<AudioInputImpl> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(AudioInputImpl);
};

class AudioInputProviderImpl : public assistant_client::AudioInputProvider {
 public:
  explicit AudioInputProviderImpl(service_manager::Connector* connector,
                                  bool default_on);
  ~AudioInputProviderImpl() override;

  // assistant_client::AudioInputProvider overrides:
  assistant_client::AudioInput& GetAudioInput() override;
  int64_t GetCurrentAudioTime() override;

  // Called when the mic state associated with the interaction is changed.
  void SetMicState(bool mic_open);

  // Called when hotword enabled status changed.
  void OnHotwordEnabled(bool enable);

 private:
  AudioInputImpl audio_input_;

  DISALLOW_COPY_AND_ASSIGN(AudioInputProviderImpl);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PLATFORM_AUDIO_INPUT_PROVIDER_IMPL_H_
