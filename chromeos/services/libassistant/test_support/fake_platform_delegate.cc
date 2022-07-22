// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/test_support/fake_platform_delegate.h"

namespace chromeos {
namespace assistant {

FakePlatformDelegate::FakePlatformDelegate() = default;
FakePlatformDelegate::~FakePlatformDelegate() = default;

void FakePlatformDelegate::BindAudioStreamFactory(
    mojo::PendingReceiver<::media::mojom::AudioStreamFactory> receiver) {
  stream_factory_receiver_ = std::move(receiver);
}

void FakePlatformDelegate::BindAudioDecoderFactory(
    mojo::PendingReceiver<::ash::assistant::mojom::AssistantAudioDecoderFactory>
        receiver) {
  audio_decoder_factory_receiver_ = std::move(receiver);
}

void FakePlatformDelegate::BindBatteryMonitor(
    mojo::PendingReceiver<::device::mojom::BatteryMonitor> receiver) {
  battery_monitor_receiver_ = std::move(receiver);
}

}  // namespace assistant
}  // namespace chromeos
