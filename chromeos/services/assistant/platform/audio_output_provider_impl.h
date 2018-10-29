// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PLATFORM_AUDIO_OUTPUT_PROVIDER_IMPL_H_
#define CHROMEOS_SERVICES_ASSISTANT_PLATFORM_AUDIO_OUTPUT_PROVIDER_IMPL_H_

#include <memory>
#include <vector>

#include "ash/public/interfaces/assistant_volume_control.mojom.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "libassistant/shared/public/platform_audio_output.h"
#include "media/base/audio_block_fifo.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_renderer_sink.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/audio/public/cpp/output_device.h"

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace chromeos {
namespace assistant {

class VolumeControlImpl : public assistant_client::VolumeControl,
                          public ash::mojom::VolumeObserver {
 public:
  explicit VolumeControlImpl(service_manager::Connector* connector);
  ~VolumeControlImpl() override;

  // assistant_client::VolumeControl overrides:
  void SetAudioFocus(
      assistant_client::OutputStreamType focused_stream) override;
  float GetSystemVolume() override;
  void SetSystemVolume(float new_volume, bool user_initiated) override;
  float GetAlarmVolume() override;
  void SetAlarmVolume(float new_volume, bool user_initiated) override;
  bool IsSystemMuted() override;
  void SetSystemMuted(bool muted) override;

  // ash::mojom::VolumeObserver overrides:
  void OnVolumeChanged(int volume) override;
  void OnMuteStateChanged(bool mute) override;

 private:
  void SetSystemVolumeOnMainThread(float new_volume, bool user_initiated);
  void SetSystemMutedOnMainThread(bool muted);

  ash::mojom::AssistantVolumeControlPtr volume_control_ptr_;
  mojo::Binding<ash::mojom::VolumeObserver> binding_;
  scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner_;

  int volume_ = 100;
  bool mute_ = false;

  base::WeakPtrFactory<VolumeControlImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(VolumeControlImpl);
};

class AudioOutputProviderImpl : public assistant_client::AudioOutputProvider {
 public:
  explicit AudioOutputProviderImpl(
      service_manager::Connector* connector,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner);
  ~AudioOutputProviderImpl() override;

  // assistant_client::AudioOutputProvider overrides:
  assistant_client::AudioOutput* CreateAudioOutput(
      assistant_client::OutputStreamType type,
      const assistant_client::OutputStreamFormat& stream_format) override;

  std::vector<assistant_client::OutputStreamEncoding>
  GetSupportedStreamEncodings() override;

  assistant_client::AudioInput* GetReferenceInput() override;

  bool SupportsPlaybackTimestamp() const override;

  assistant_client::VolumeControl& GetVolumeControl() override;

  void RegisterAudioEmittingStateCallback(
      AudioEmittingStateCallback callback) override;

 private:
  VolumeControlImpl volume_control_impl_;
  service_manager::Connector* connector_;
  scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(AudioOutputProviderImpl);
};

class AudioDeviceOwner : public media::AudioRendererSink::RenderCallback {
 public:
  AudioDeviceOwner(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner);
  ~AudioDeviceOwner() override;

  void StartOnMainThread(assistant_client::AudioOutput::Delegate* delegate,
                         service_manager::Connector* connector,
                         const assistant_client::OutputStreamFormat& format);

  void StopOnBackgroundThread();

  // media::AudioRenderSink::RenderCallback overrides:
  int Render(base::TimeDelta delay,
             base::TimeTicks delay_timestamp,
             int prior_frames_skipped,
             media::AudioBus* dest) override;

  void OnRenderError() override;

  void SetDelegate(assistant_client::AudioOutput::Delegate* delegate);

 private:
  void StartDeviceOnBackgroundThread(
      std::unique_ptr<service_manager::Connector> connector);

  // Requests assistant to fill buffer with more data.
  void ScheduleFillLocked(const base::TimeTicks& time);

  // Callback for assistant to notify that it completes the filling.
  void BufferFillDone(int num_bytes);

  scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  base::Lock lock_;
  std::unique_ptr<media::AudioBlockFifo> audio_fifo_;  // guarded by lock_.
  // Whether assistant is filling the buffer -- delegate_->FillBuffer is called
  // and BufferFillDone() is not called yet.
  // guarded by lock_.
  bool is_filling_ = false;

  assistant_client::AudioOutput::Delegate* delegate_;
  std::unique_ptr<audio::OutputDevice> output_device_;
  // Stores audio frames generated by assistant.
  std::vector<uint8_t> audio_data_;
  assistant_client::OutputStreamFormat format_;
  media::AudioParameters audio_param_;

  DISALLOW_COPY_AND_ASSIGN(AudioDeviceOwner);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PLATFORM_AUDIO_OUTPUT_PROVIDER_IMPL_H_
