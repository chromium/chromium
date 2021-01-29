// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/platform_api_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "chromeos/services/assistant/media_session/assistant_media_session.h"
#include "chromeos/services/assistant/platform/audio_devices.h"
#include "chromeos/services/assistant/platform/power_manager_provider_impl.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "chromeos/services/assistant/public/cpp/migration/audio_input_host.h"
#include "chromeos/services/assistant/utils.h"
#include "libassistant/shared/public/assistant_export.h"
#include "libassistant/shared/public/platform_api.h"
#include "libassistant/shared/public/platform_factory.h"
#include "media/audio/audio_device_description.h"

using assistant_client::AudioOutputProvider;
using assistant_client::FileProvider;
using assistant_client::NetworkProvider;
using assistant_client::PlatformApi;
using assistant_client::SystemProvider;

namespace chromeos {
namespace assistant {

////////////////////////////////////////////////////////////////////////////////
// PlatformApiImpl
////////////////////////////////////////////////////////////////////////////////

PlatformApiImpl::PlatformApiImpl(
    AssistantMediaSession* media_session,
    PowerManagerClient* power_manager_client,
    mojo::PendingRemote<device::mojom::BatteryMonitor> battery_monitor,
    scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> background_task_runner)
    : audio_output_provider_(media_session,
                             background_task_runner,
                             media::AudioDeviceDescription::kDefaultDeviceId) {
  // Only enable native power features if they are supported by the UI.
  std::unique_ptr<PowerManagerProviderImpl> provider;
  if (features::IsPowerManagerEnabled()) {
    provider = std::make_unique<PowerManagerProviderImpl>(
        std::move(main_thread_task_runner));
  }
  system_provider_ = std::make_unique<SystemProviderImpl>(
      std::move(provider), std::move(battery_monitor));
}

PlatformApiImpl::~PlatformApiImpl() = default;

AudioOutputProvider& PlatformApiImpl::GetAudioOutputProvider() {
  return audio_output_provider_;
}

FileProvider& PlatformApiImpl::GetFileProvider() {
  return file_provider_;
}

NetworkProvider& PlatformApiImpl::GetNetworkProvider() {
  return network_provider_;
}

SystemProvider& PlatformApiImpl::GetSystemProvider() {
  return *system_provider_;
}

}  // namespace assistant
}  // namespace chromeos
