// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_ASSISTANT_PUBLIC_CPP_ASSISTANT_BROWSER_DELEGATE_H_
#define CHROMEOS_ASH_SERVICES_ASSISTANT_PUBLIC_CPP_ASSISTANT_BROWSER_DELEGATE_H_

#include "ash/public/mojom/assistant_volume_control.mojom.h"
#include "base/component_export.h"
#include "base/one_shot_event.h"
#include "base/types/expected.h"
#include "chromeos/ash/components/assistant/buildflags.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_enums.h"
#include "chromeos/ash/services/assistant/public/mojom/assistant_audio_decoder.mojom.h"
#include "chromeos/ash/services/libassistant/public/cpp/assistant_notification.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "media/mojo/mojom/audio_stream_factory.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/device/public/mojom/battery_monitor.mojom.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "ui/accessibility/mojom/ax_assistant_structure.mojom.h"

#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#include "chromeos/ash/services/libassistant/public/mojom/service.mojom-forward.h"
#endif  // BUILDFLAG(ENABLE_CROS_LIBASSISTANT)

namespace ash::assistant {

// Main interface implemented in browser to provide dependencies to
// |ash::assistant::Service|.
class COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) AssistantBrowserDelegate {
 public:
  enum class Error {
    kProfileNotReady,
    kWebAppProviderNotReadyToRead,
    kNewEntryPointNotEnabled,
    kNewEntryPointNotFound,
    kNonGoogleChromeBuild,
  };

  AssistantBrowserDelegate();
  AssistantBrowserDelegate(const AssistantBrowserDelegate&) = delete;
  AssistantBrowserDelegate& operator=(const AssistantBrowserDelegate&) = delete;
  virtual ~AssistantBrowserDelegate();

  static AssistantBrowserDelegate* Get();

  // Notifies assistant client that assistant running status has changed.
  virtual void OnAssistantStatusChanged(AssistantStatus new_status) = 0;

  // Requests Ash's AssistantVolumeControl interface from the browser.
  virtual void RequestAssistantVolumeControl(
      mojo::PendingReceiver<::ash::mojom::AssistantVolumeControl> receiver) = 0;

  // Requests a BatteryMonitor from the browser.
  virtual void RequestBatteryMonitor(
      mojo::PendingReceiver<device::mojom::BatteryMonitor> receiver) = 0;

  // Requests a connection to the Device service's WakeLockProvider interface
  // from the browser.
  virtual void RequestWakeLockProvider(
      mojo::PendingReceiver<device::mojom::WakeLockProvider> receiver) = 0;

  // Requests an Audio Service AudioStreamFactory from the browser.
  virtual void RequestAudioStreamFactory(
      mojo::PendingReceiver<media::mojom::AudioStreamFactory> receiver) = 0;

  // Requests an audio decoder interface from the Assistant Audio Decoder
  // service, via the browser.
  virtual void RequestAudioDecoderFactory(
      mojo::PendingReceiver<ash::assistant::mojom::AssistantAudioDecoderFactory>
          receiver) = 0;

  // Requests a connection to the Media Session service's AudioFocusManager from
  // the browser.
  virtual void RequestAudioFocusManager(
      mojo::PendingReceiver<media_session::mojom::AudioFocusManager>
          receiver) = 0;

  // Requests a connection to the Media Session service's MediaControllerManager
  // interface from the browser.
  virtual void RequestMediaControllerManager(
      mojo::PendingReceiver<media_session::mojom::MediaControllerManager>
          receiver) = 0;

  // Requests a connection to the CrosNetworkConfig service interface via the
  // browser.
  virtual void RequestNetworkConfig(
      mojo::PendingReceiver<chromeos::network_config::mojom::CrosNetworkConfig>
          receiver) = 0;

  // Opens the specified `url` in a new browser tab. Special handling is applied
  // to OS Settings url which may cause deviation from this behavior.
  virtual void OpenUrl(GURL url) = 0;

  // Returns true if a primary profile is eligible for Assistant new entry
  // point. Note that an error might be returned if you read this value before
  // `is_new_entry_point_eligible_for_primary_profile_ready` event is signaled.
  virtual base::expected<bool, Error>
  IsNewEntryPointEligibleForPrimaryProfile() = 0;

  // An event signaled if the new entry point eligibility value is ready to
  // read. There is initialization happening as an async operation. Early read
  // of `IsNewEntryPointEligibleForPrimaryProfile` can return an error value.
  // TODO(crbug.com/386257055): update API as this supports eligibility change
  // events, e.g., uninstall.
  const base::OneShotEvent&
  is_new_entry_point_eligible_for_primary_profile_ready() {
    return on_is_new_entry_point_eligible_ready_;
  }

  // Opens the new entry point.
  virtual void OpenNewEntryPoint() = 0;

  // Returns name of the new entry point. `std::nullopt` is returned for any
  // error cases, e.g., the new entry point is not installed.
  virtual std::optional<std::string> GetNewEntryPointName() = 0;

#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
  // Requests a connection to Libassistant service interface via the browser.
  virtual void RequestLibassistantService(
      mojo::PendingReceiver<libassistant::mojom::LibassistantService>
          receiver) = 0;
#endif  // BUILDFLAG(ENABLE_CROS_LIBASSISTANT)

 protected:
  base::OneShotEvent on_is_new_entry_point_eligible_ready_;
};

}  // namespace ash::assistant

// TODO(b/258750971): remove when internal assistant codes are migrated to
// namespace ash.
namespace chromeos::assistant {
using ::ash::assistant::AssistantBrowserDelegate;
}

#endif  // CHROMEOS_ASH_SERVICES_ASSISTANT_PUBLIC_CPP_ASSISTANT_BROWSER_DELEGATE_H_
