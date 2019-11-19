// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PLATFORM_VOLUME_CONTROL_IMPL_H_
#define CHROMEOS_SERVICES_ASSISTANT_PLATFORM_VOLUME_CONTROL_IMPL_H_

#include "ash/public/mojom/assistant_volume_control.mojom.h"
#include "base/macros.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "libassistant/shared/public/platform_audio_output.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace assistant {

class AssistantMediaSession;

class VolumeControlImpl : public assistant_client::VolumeControl,
                          public ash::mojom::VolumeObserver {
 public:
  VolumeControlImpl(mojom::Client* client,
                    AssistantMediaSession* media_session);
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
  void SetAudioFocusOnMainThread(
      assistant_client::OutputStreamType focused_stream);
  void SetSystemVolumeOnMainThread(float new_volume, bool user_initiated);
  void SetSystemMutedOnMainThread(bool muted);

  AssistantMediaSession* media_session_;
  mojo::Remote<ash::mojom::AssistantVolumeControl> volume_control_;
  mojo::Receiver<ash::mojom::VolumeObserver> receiver_{this};
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;

  int volume_ = 100;
  bool mute_ = false;

  base::WeakPtrFactory<VolumeControlImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(VolumeControlImpl);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PLATFORM_VOLUME_CONTROL_IMPL_H_
