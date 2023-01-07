// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_AUDIO_IN_PROCESS_INSTANCE_H_
#define CHROMEOS_ASH_COMPONENTS_AUDIO_IN_PROCESS_INSTANCE_H_

#include "base/component_export.h"
#include "chromeos/ash/components/audio/public/mojom/cros_audio_config.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace ash::audio_config {

// Binds to an instance of CrosAudioConfig from within the browser process.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_AUDIO_IN_PROCESS_AUDIO_CONFIG)
void BindToInProcessInstance(
    mojo::PendingReceiver<mojom::CrosAudioConfig> pending_receiver);

}  // namespace ash::audio_config

#endif  // CHROMEOS_ASH_COMPONENTS_AUDIO_IN_PROCESS_INSTANCE_H_
