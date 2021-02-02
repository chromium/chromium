// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PLATFORM_API_IMPL_H_
#define CHROMEOS_SERVICES_ASSISTANT_PLATFORM_API_IMPL_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "chromeos/services/assistant/platform/audio_output_provider_impl.h"
#include "chromeos/services/assistant/platform/file_provider_impl.h"
#include "chromeos/services/assistant/platform/network_provider_impl.h"
#include "chromeos/services/assistant/platform/system_provider_impl.h"
#include "chromeos/services/assistant/public/cpp/assistant_service.h"
#include "chromeos/services/assistant/public/cpp/migration/cros_platform_api.h"
#include "chromeos/services/libassistant/public/mojom/platform_delegate.mojom-forward.h"
#include "libassistant/shared/public/platform_auth.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/mojom/battery_monitor.mojom.h"

namespace chromeos {
class PowerManagerClient;

namespace assistant {

class AssistantMediaSession;

// Platform API required by the voice assistant.
class PlatformApiImpl : public CrosPlatformApi {
 public:
  PlatformApiImpl(
      AssistantMediaSession* media_session,
      chromeos::libassistant::mojom::PlatformDelegate* platform_delegate,
      PowerManagerClient* power_manager_client,
      mojo::PendingRemote<device::mojom::BatteryMonitor> battery_monitor,
      scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> background_task_runner);
  ~PlatformApiImpl() override;

  assistant_client::AudioOutputProvider& GetAudioOutputProvider() override;
  assistant_client::FileProvider& GetFileProvider() override;
  assistant_client::NetworkProvider& GetNetworkProvider() override;
  assistant_client::SystemProvider& GetSystemProvider() override;

 private:
  AudioOutputProviderImpl audio_output_provider_;
  FileProviderImpl file_provider_;
  NetworkProviderImpl network_provider_;
  std::unique_ptr<SystemProviderImpl> system_provider_;

  DISALLOW_COPY_AND_ASSIGN(PlatformApiImpl);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PLATFORM_API_IMPL_H_
