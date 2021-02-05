// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_MIGRATION_FAKE_PLATFORM_DELEGATE_H_
#define CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_MIGRATION_FAKE_PLATFORM_DELEGATE_H_

#include "chromeos/services/libassistant/public/mojom/platform_delegate.mojom.h"

#include "base/component_export.h"

namespace chromeos {
namespace assistant {

class COMPONENT_EXPORT(ASSISTANT_SERVICE_MIGRATION_TEST_SUPPORT)
    FakePlatformDelegate : public libassistant::mojom::PlatformDelegate {
 public:
  FakePlatformDelegate();
  FakePlatformDelegate(FakePlatformDelegate&) = delete;
  FakePlatformDelegate& operator=(FakePlatformDelegate&) = delete;
  ~FakePlatformDelegate() override;

  // mojom::PlatformDelegate implementation:
  void BindAudioStreamFactory(
      mojo::PendingReceiver<::audio::mojom::StreamFactory> receiver) override;
  void BindAudioDecoderFactory(
      mojo::PendingReceiver<
          ::chromeos::assistant::mojom::AssistantAudioDecoderFactory> receiver)
      override {}
  void BindAssistantVolumeControl(
      mojo::PendingReceiver<::ash::mojom::AssistantVolumeControl> receiver)
      override {}
  void BindWakeLockProvider(
      mojo::PendingReceiver<::device::mojom::WakeLockProvider> receiver)
      override {}

  // Return the pending receiver passed to the last BindAudioStreamFactory call.
  mojo::PendingReceiver<::audio::mojom::StreamFactory>
  stream_factory_receiver() {
    return std::move(stream_factory_receiver_);
  }

 private:
  mojo::PendingReceiver<::audio::mojom::StreamFactory> stream_factory_receiver_;
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_MIGRATION_FAKE_PLATFORM_DELEGATE_H_
