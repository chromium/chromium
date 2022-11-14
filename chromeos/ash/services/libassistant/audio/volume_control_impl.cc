// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/audio/volume_control_impl.h"

#include <utility>

#include "ash/public/mojom/assistant_volume_control.mojom.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/ash/services/libassistant/public/mojom/platform_delegate.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/type_converter.h"

using ::ash::libassistant::mojom::AudioOutputStreamType;

namespace mojo {
template <>
struct TypeConverter<AudioOutputStreamType,
                     assistant_client::OutputStreamType> {
  static AudioOutputStreamType Convert(
      const assistant_client::OutputStreamType& input) {
    using assistant_client::OutputStreamType;
    switch (input) {
      case OutputStreamType::STREAM_ALARM:
        return AudioOutputStreamType::kAlarmStream;
      case OutputStreamType::STREAM_TTS:
        return AudioOutputStreamType::kTtsStream;
      case OutputStreamType::STREAM_MEDIA:
        return AudioOutputStreamType::kMediaStream;
    }
  }
};

}  // namespace mojo

namespace ash::libassistant {

VolumeControlImpl::VolumeControlImpl()
    : main_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      weak_factory_(this) {}

void VolumeControlImpl::Initialize(
    mojom::AudioOutputDelegate* audio_output_delegate,
    mojom::PlatformDelegate* platform_delegate) {
  audio_output_delegate_ = audio_output_delegate;
  platform_delegate->BindAssistantVolumeControl(
      volume_control_.BindNewPipeAndPassReceiver());
  mojo::PendingRemote<ash::mojom::VolumeObserver> observer;
  receiver_.Bind(observer.InitWithNewPipeAndPassReceiver());
  volume_control_->AddVolumeObserver(std::move(observer));
}

VolumeControlImpl::~VolumeControlImpl() = default;

void VolumeControlImpl::SetAudioFocus(
    assistant_client::OutputStreamType focused_stream) {
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VolumeControlImpl::SetAudioFocusOnMainThread,
                                weak_factory_.GetWeakPtr(), focused_stream));
}

float VolumeControlImpl::GetSystemVolume() {
  return volume_ * 1.0 / 100.0;
}

void VolumeControlImpl::SetSystemVolume(float new_volume, bool user_initiated) {
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VolumeControlImpl::SetSystemVolumeOnMainThread,
                     weak_factory_.GetWeakPtr(), new_volume, user_initiated));
}

float VolumeControlImpl::GetAlarmVolume() {
  // TODO(muyuanli): implement.
  return 1.0f;
}

void VolumeControlImpl::SetAlarmVolume(float new_volume, bool user_initiated) {
  // TODO(muyuanli): implement.
}

bool VolumeControlImpl::IsSystemMuted() {
  return mute_;
}

void VolumeControlImpl::SetSystemMuted(bool muted) {
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VolumeControlImpl::SetSystemMutedOnMainThread,
                                weak_factory_.GetWeakPtr(), muted));
}

void VolumeControlImpl::OnVolumeChanged(int volume) {
  volume_ = volume;
}

void VolumeControlImpl::OnMuteStateChanged(bool mute) {
  mute_ = mute;
}

void VolumeControlImpl::SetAudioFocusOnMainThread(
    assistant_client::OutputStreamType focused_stream) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  audio_output_delegate_->RequestAudioFocus(
      mojo::ConvertTo<AudioOutputStreamType>(focused_stream));
}

void VolumeControlImpl::SetSystemVolumeOnMainThread(float new_volume,
                                                    bool user_initiated) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  volume_control_->SetVolume(new_volume * 100.0, user_initiated);
}

void VolumeControlImpl::SetSystemMutedOnMainThread(bool muted) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  volume_control_->SetMuted(muted);
}

}  // namespace ash::libassistant
