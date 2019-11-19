// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/platform/volume_control_impl.h"

#include <utility>

#include "ash/public/mojom/assistant_volume_control.mojom.h"
#include "ash/public/mojom/constants.mojom.h"
#include "chromeos/services/assistant/media_session/assistant_media_session.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace assistant {

VolumeControlImpl::VolumeControlImpl(mojom::Client* client,
                                     AssistantMediaSession* media_session)
    : media_session_(media_session),
      main_task_runner_(base::SequencedTaskRunnerHandle::Get()),
      weak_factory_(this) {
  client->RequestAssistantVolumeControl(
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
  // TODO(wutao): Fix the libassistant behavior.
  // Currently this is called with |STREAM_TTS| and |STREAM_ALARM| when
  // requesting focus. When releasing focus it calls with |STREAM_MEDIA|.
  // libassistant media code path does not request focus.
  switch (focused_stream) {
    case assistant_client::OutputStreamType::STREAM_ALARM:
      media_session_->RequestAudioFocus(
          media_session::mojom::AudioFocusType::kGainTransientMayDuck);
      break;
    case assistant_client::OutputStreamType::STREAM_TTS:
      media_session_->RequestAudioFocus(
          media_session::mojom::AudioFocusType::kGainTransient);
      break;
    case assistant_client::OutputStreamType::STREAM_MEDIA:
      media_session_->AbandonAudioFocusIfNeeded();
      break;
  }
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

}  // namespace assistant
}  // namespace chromeos
