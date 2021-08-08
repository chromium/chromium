// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "chrome/browser/ui/webui/chromeos/audio/audio_handler.h"

namespace chromeos {

AudioHandler::AudioHandler(
    mojo::PendingReceiver<audio::mojom::PageHandler> receiver,
    mojo::PendingRemote<audio::mojom::Page> page)
    : page_(std::move(page)), receiver_(this, std::move(receiver)) {}

AudioHandler::~AudioHandler() = default;

void AudioHandler::GetAudioDeviceInfo(
    audio::mojom::PageHandler::GetAudioDeviceInfoCallback callback) {
  const std::string mock_device_name = "mock";
  std::move(callback).Run(mock_device_name);
}

}  // namespace chromeos
