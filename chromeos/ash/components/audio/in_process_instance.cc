// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/audio/in_process_instance.h"

#include "base/no_destructor.h"
#include "chromeos/ash/components/audio/cros_audio_config_impl.h"

namespace ash::audio_config {

void BindToInProcessInstance(
    mojo::PendingReceiver<mojom::CrosAudioConfig> pending_receiver) {
  static base::NoDestructor<CrosAudioConfigImpl> instance;
  instance->BindPendingReceiver(std::move(pending_receiver));
}

}  // namespace ash::audio_config
