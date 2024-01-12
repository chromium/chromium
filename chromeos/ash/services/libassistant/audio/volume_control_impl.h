// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_AUDIO_VOLUME_CONTROL_IMPL_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_AUDIO_VOLUME_CONTROL_IMPL_H_

#include "ash/public/mojom/assistant_volume_control.mojom.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/ash/services/libassistant/public/mojom/audio_output_delegate.mojom.h"
#include "chromeos/ash/services/libassistant/public/mojom/platform_delegate.mojom-forward.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::libassistant {

class VolumeControlImpl : public assistant_client::VolumeControl,
                          public ash::mojom::VolumeObserver {
 public:
  VolumeControlImpl();

  VolumeControlImpl(const VolumeControlImpl&) = delete;
  VolumeControlImpl& operator=(const VolumeControlImpl&) = delete;

  ~VolumeControlImpl() override;

  void Initialize(mojom::AudioOutputDelegate* audio_output_delegate,
                  mojom::PlatformDelegate* platform_delegate);

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

  // Owned by |AudioOutputProviderImpl|.
  raw_ptr<mojom::AudioOutputDelegate> audio_output_delegate_ = nullptr;
  mojo::Remote<ash::mojom::AssistantVolumeControl> volume_control_;
  mojo::Receiver<ash::mojom::VolumeObserver> receiver_{this};
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;

  int volume_ = 100;
  bool mute_ = false;

  base::WeakPtrFactory<VolumeControlImpl> weak_factory_;
};

}  // namespace ash::libassistant

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_AUDIO_VOLUME_CONTROL_IMPL_H_
