// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TABS_TABS_MOJOM_TRAITS_H_
#define CHROME_BROWSER_UI_WEBUI_TABS_TABS_MOJOM_TRAITS_H_

#include "base/containers/fixed_flat_map.h"
#include "chrome/browser/ui/tabs/alert/tab_alert.h"
#include "chrome/browser/ui/webui/tabs/tabs.mojom.h"

namespace mojo {

template <>
struct EnumTraits<tabs::mojom::TabAlertState, tabs::TabAlert> {
  static tabs::mojom::TabAlertState ToMojom(tabs::TabAlert input) {
    static constexpr auto alert_state_map =
        base::MakeFixedFlatMap<tabs::TabAlert, tabs::mojom::TabAlertState>(
            {{tabs::TabAlert::MEDIA_RECORDING,
              tabs::mojom::TabAlertState::kMediaRecording},
             {tabs::TabAlert::TAB_CAPTURING,
              tabs::mojom::TabAlertState::kTabCapturing},
             {tabs::TabAlert::AUDIO_PLAYING,
              tabs::mojom::TabAlertState::kAudioPlaying},
             {tabs::TabAlert::AUDIO_MUTING,
              tabs::mojom::TabAlertState::kAudioMuting},
             {tabs::TabAlert::BLUETOOTH_CONNECTED,
              tabs::mojom::TabAlertState::kBluetoothConnected},
             {tabs::TabAlert::BLUETOOTH_SCAN_ACTIVE,
              tabs::mojom::TabAlertState::kBluetoothConnected},
             {tabs::TabAlert::USB_CONNECTED,
              tabs::mojom::TabAlertState::kUsbConnected},
             {tabs::TabAlert::HID_CONNECTED,
              tabs::mojom::TabAlertState::kHidConnected},
             {tabs::TabAlert::SERIAL_CONNECTED,
              tabs::mojom::TabAlertState::kSerialConnected},
             {tabs::TabAlert::PIP_PLAYING,
              tabs::mojom::TabAlertState::kPipPlaying},
             {tabs::TabAlert::DESKTOP_CAPTURING,
              tabs::mojom::TabAlertState::kDesktopCapturing},
             {tabs::TabAlert::VR_PRESENTING_IN_HEADSET,
              tabs::mojom::TabAlertState::kVrPresentingInHeadset},
             {tabs::TabAlert::AUDIO_RECORDING,
              tabs::mojom::TabAlertState::kAudioRecording},
             {tabs::TabAlert::VIDEO_RECORDING,
              tabs::mojom::TabAlertState::kVideoRecording},
             {tabs::TabAlert::GLIC_ACCESSING,
              tabs::mojom::TabAlertState::kGlicAccessing}});
    return alert_state_map.at(input);
  }

  static bool FromMojom(tabs::mojom::TabAlertState input, tabs::TabAlert* out) {
    static constexpr auto alert_state_map =
        base::MakeFixedFlatMap<tabs::mojom::TabAlertState, tabs::TabAlert>(
            {{tabs::mojom::TabAlertState::kMediaRecording,
              tabs::TabAlert::MEDIA_RECORDING},
             {tabs::mojom::TabAlertState::kTabCapturing,
              tabs::TabAlert::TAB_CAPTURING},
             {tabs::mojom::TabAlertState::kAudioPlaying,
              tabs::TabAlert::AUDIO_PLAYING},
             {tabs::mojom::TabAlertState::kAudioMuting,
              tabs::TabAlert::AUDIO_MUTING},
             {tabs::mojom::TabAlertState::kBluetoothConnected,
              tabs::TabAlert::BLUETOOTH_CONNECTED},
             {tabs::mojom::TabAlertState::kUsbConnected,
              tabs::TabAlert::USB_CONNECTED},
             {tabs::mojom::TabAlertState::kHidConnected,
              tabs::TabAlert::HID_CONNECTED},
             {tabs::mojom::TabAlertState::kSerialConnected,
              tabs::TabAlert::SERIAL_CONNECTED},
             {tabs::mojom::TabAlertState::kPipPlaying,
              tabs::TabAlert::PIP_PLAYING},
             {tabs::mojom::TabAlertState::kDesktopCapturing,
              tabs::TabAlert::DESKTOP_CAPTURING},
             {tabs::mojom::TabAlertState::kVrPresentingInHeadset,
              tabs::TabAlert::VR_PRESENTING_IN_HEADSET},
             {tabs::mojom::TabAlertState::kAudioRecording,
              tabs::TabAlert::AUDIO_RECORDING},
             {tabs::mojom::TabAlertState::kVideoRecording,
              tabs::TabAlert::VIDEO_RECORDING}});
    *out = alert_state_map.at(input);
    return true;
  }
};

}  // namespace mojo

#endif  // CHROME_BROWSER_UI_WEBUI_TABS_TABS_MOJOM_TRAITS_H_
