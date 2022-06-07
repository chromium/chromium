// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_TEST_SUPPORT_FAKE_PLATFORM_DELEGATE_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_TEST_SUPPORT_FAKE_PLATFORM_DELEGATE_H_

#include "chromeos/services/libassistant/public/mojom/platform_delegate.mojom.h"

namespace chromeos {
namespace assistant {

class FakePlatformDelegate
    : public chromeos::libassistant::mojom::PlatformDelegate {
 public:
  FakePlatformDelegate();
  FakePlatformDelegate(FakePlatformDelegate&) = delete;
  FakePlatformDelegate& operator=(FakePlatformDelegate&) = delete;
  ~FakePlatformDelegate() override;

  // mojom::PlatformDelegate implementation:
  void BindAudioStreamFactory(
      mojo::PendingReceiver<::media::mojom::AudioStreamFactory> receiver)
      override;
  void BindAudioDecoderFactory(
      mojo::PendingReceiver<
          chromeos::assistant::mojom::AssistantAudioDecoderFactory> receiver)
      override;
  void BindBatteryMonitor(
      mojo::PendingReceiver<::device::mojom::BatteryMonitor> receiver) override;
  void BindNetworkConfig(
      mojo::PendingReceiver<chromeos::network_config::mojom::CrosNetworkConfig>
          receiver) override {}
  void BindAssistantVolumeControl(
      mojo::PendingReceiver<::ash::mojom::AssistantVolumeControl> receiver)
      override {}
  void BindWakeLockProvider(
      mojo::PendingReceiver<::device::mojom::WakeLockProvider> receiver)
      override {}

  // Return the pending receiver passed to the last BindAudioStreamFactory call.
  mojo::PendingReceiver<::media::mojom::AudioStreamFactory>
  stream_factory_receiver() {
    return std::move(stream_factory_receiver_);
  }

  mojo::PendingReceiver<
      ::chromeos::assistant::mojom::AssistantAudioDecoderFactory>
  audio_decoder_factory_receiver() {
    return std::move(audio_decoder_factory_receiver_);
  }

  // Return the pending receiver passed to the last BindBatteryMonitor call.
  mojo::PendingReceiver<::device::mojom::BatteryMonitor>
  battery_monitor_receiver() {
    return std::move(battery_monitor_receiver_);
  }

 private:
  mojo::PendingReceiver<::media::mojom::AudioStreamFactory>
      stream_factory_receiver_;
  mojo::PendingReceiver<
      ::chromeos::assistant::mojom::AssistantAudioDecoderFactory>
      audio_decoder_factory_receiver_;
  mojo::PendingReceiver<::device::mojom::BatteryMonitor>
      battery_monitor_receiver_;
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_TEST_SUPPORT_FAKE_PLATFORM_DELEGATE_H_
