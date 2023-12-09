// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TABS_TABS_MOJOM_TRAITS_H_
#define CHROME_BROWSER_UI_WEBUI_TABS_TABS_MOJOM_TRAITS_H_

#include "base/containers/fixed_flat_map.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/webui/tabs/tabs.mojom.h"

namespace mojo {

template <>
struct EnumTraits<tabs::mojom::TabAlertState, TabAlertState> {
  static tabs::mojom::TabAlertState ToMojom(TabAlertState input) {
    static constexpr auto alert_state_map =
        base::MakeFixedFlatMap<TabAlertState, tabs::mojom::TabAlertState>(
            {{TabAlertState::MEDIA_RECORDING,
              tabs::mojom::TabAlertState::kMediaRecording},
             {TabAlertState::TAB_CAPTURING,
              tabs::mojom::TabAlertState::kTabCapturing},
             {TabAlertState::AUDIO_PLAYING,
              tabs::mojom::TabAlertState::kAudioPlaying},
             {TabAlertState::AUDIO_MUTING,
              tabs::mojom::TabAlertState::kAudioMuting},
             {TabAlertState::BLUETOOTH_CONNECTED,
              tabs::mojom::TabAlertState::kBluetoothConnected},
             {TabAlertState::BLUETOOTH_SCAN_ACTIVE,
              tabs::mojom::TabAlertState::kBluetoothConnected},
             {TabAlertState::USB_CONNECTED,
              tabs::mojom::TabAlertState::kUsbConnected},
             {TabAlertState::HID_CONNECTED,
              tabs::mojom::TabAlertState::kHidConnected},
             {TabAlertState::SERIAL_CONNECTED,
              tabs::mojom::TabAlertState::kSerialConnected},
             {TabAlertState::PIP_PLAYING,
              tabs::mojom::TabAlertState::kPipPlaying},
             {TabAlertState::DESKTOP_CAPTURING,
              tabs::mojom::TabAlertState::kDesktopCapturing},
             {TabAlertState::VR_PRESENTING_IN_HEADSET,
              tabs::mojom::TabAlertState::kVrPresentingInHeadset},
             {TabAlertState::AUDIO_RECORDING,
              tabs::mojom::TabAlertState::kAudioRecording},
             {TabAlertState::VIDEO_RECORDING,
              tabs::mojom::TabAlertState::kVideoRecording}});
    return alert_state_map.at(input);
  }

  static bool FromMojom(tabs::mojom::TabAlertState input, TabAlertState* out) {
    static constexpr auto alert_state_map =
        base::MakeFixedFlatMap<tabs::mojom::TabAlertState, TabAlertState>(
            {{tabs::mojom::TabAlertState::kMediaRecording,
              TabAlertState::MEDIA_RECORDING},
             {tabs::mojom::TabAlertState::kTabCapturing,
              TabAlertState::TAB_CAPTURING},
             {tabs::mojom::TabAlertState::kAudioPlaying,
              TabAlertState::AUDIO_PLAYING},
             {tabs::mojom::TabAlertState::kAudioMuting,
              TabAlertState::AUDIO_MUTING},
             {tabs::mojom::TabAlertState::kBluetoothConnected,
              TabAlertState::BLUETOOTH_CONNECTED},
             {tabs::mojom::TabAlertState::kUsbConnected,
              TabAlertState::USB_CONNECTED},
             {tabs::mojom::TabAlertState::kHidConnected,
              TabAlertState::HID_CONNECTED},
             {tabs::mojom::TabAlertState::kSerialConnected,
              TabAlertState::SERIAL_CONNECTED},
             {tabs::mojom::TabAlertState::kPipPlaying,
              TabAlertState::PIP_PLAYING},
             {tabs::mojom::TabAlertState::kDesktopCapturing,
              TabAlertState::DESKTOP_CAPTURING},
             {tabs::mojom::TabAlertState::kVrPresentingInHeadset,
              TabAlertState::VR_PRESENTING_IN_HEADSET},
             {tabs::mojom::TabAlertState::kAudioRecording,
              TabAlertState::AUDIO_RECORDING},
             {tabs::mojom::TabAlertState::kVideoRecording,
              TabAlertState::VIDEO_RECORDING}});
    *out = alert_state_map.at(input);
    return true;
  }
};

}  // namespace mojo

#endif  // CHROME_BROWSER_UI_WEBUI_TABS_TABS_MOJOM_TRAITS_H_
