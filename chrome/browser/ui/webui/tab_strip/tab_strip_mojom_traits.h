// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TAB_STRIP_TAB_STRIP_MOJOM_TRAITS_H_
#define CHROME_BROWSER_UI_WEBUI_TAB_STRIP_TAB_STRIP_MOJOM_TRAITS_H_

#include "base/containers/fixed_flat_map.h"
#include "chrome/browser/ui/tabs/tab_network_state.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip.mojom.h"

namespace mojo {

template <>
struct EnumTraits<tab_strip::mojom::TabAlertState, TabAlertState> {
  static tab_strip::mojom::TabAlertState ToMojom(TabAlertState input) {
    static constexpr auto alert_state_map =
        base::MakeFixedFlatMap<TabAlertState, tab_strip::mojom::TabAlertState>(
            {{TabAlertState::MEDIA_RECORDING,
              tab_strip::mojom::TabAlertState::kMediaRecording},
             {TabAlertState::TAB_CAPTURING,
              tab_strip::mojom::TabAlertState::kTabCapturing},
             {TabAlertState::AUDIO_PLAYING,
              tab_strip::mojom::TabAlertState::kAudioPlaying},
             {TabAlertState::AUDIO_MUTING,
              tab_strip::mojom::TabAlertState::kAudioMuting},
             {TabAlertState::BLUETOOTH_CONNECTED,
              tab_strip::mojom::TabAlertState::kBluetoothConnected},
             {TabAlertState::BLUETOOTH_SCAN_ACTIVE,
              tab_strip::mojom::TabAlertState::kBluetoothConnected},
             {TabAlertState::USB_CONNECTED,
              tab_strip::mojom::TabAlertState::kUsbConnected},
             {TabAlertState::HID_CONNECTED,
              tab_strip::mojom::TabAlertState::kHidConnected},
             {TabAlertState::SERIAL_CONNECTED,
              tab_strip::mojom::TabAlertState::kSerialConnected},
             {TabAlertState::PIP_PLAYING,
              tab_strip::mojom::TabAlertState::kPipPlaying},
             {TabAlertState::DESKTOP_CAPTURING,
              tab_strip::mojom::TabAlertState::kDesktopCapturing},
             {TabAlertState::VR_PRESENTING_IN_HEADSET,
              tab_strip::mojom::TabAlertState::kVrPresentingInHeadset}});
    return alert_state_map.at(input);
  }

  static bool FromMojom(tab_strip::mojom::TabAlertState input,
                        TabAlertState* out) {
    static constexpr auto alert_state_map =
        base::MakeFixedFlatMap<tab_strip::mojom::TabAlertState, TabAlertState>(
            {{tab_strip::mojom::TabAlertState::kMediaRecording,
              TabAlertState::MEDIA_RECORDING},
             {tab_strip::mojom::TabAlertState::kTabCapturing,
              TabAlertState::TAB_CAPTURING},
             {tab_strip::mojom::TabAlertState::kAudioPlaying,
              TabAlertState::AUDIO_PLAYING},
             {tab_strip::mojom::TabAlertState::kAudioMuting,
              TabAlertState::AUDIO_MUTING},
             {tab_strip::mojom::TabAlertState::kBluetoothConnected,
              TabAlertState::BLUETOOTH_CONNECTED},
             {tab_strip::mojom::TabAlertState::kUsbConnected,
              TabAlertState::USB_CONNECTED},
             {tab_strip::mojom::TabAlertState::kHidConnected,
              TabAlertState::HID_CONNECTED},
             {tab_strip::mojom::TabAlertState::kSerialConnected,
              TabAlertState::SERIAL_CONNECTED},
             {tab_strip::mojom::TabAlertState::kPipPlaying,
              TabAlertState::PIP_PLAYING},
             {tab_strip::mojom::TabAlertState::kDesktopCapturing,
              TabAlertState::DESKTOP_CAPTURING},
             {tab_strip::mojom::TabAlertState::kVrPresentingInHeadset,
              TabAlertState::VR_PRESENTING_IN_HEADSET}});
    *out = alert_state_map.at(input);
    return true;
  }
};

template <>
struct EnumTraits<tab_strip::mojom::TabNetworkState, TabNetworkState> {
  static tab_strip::mojom::TabNetworkState ToMojom(TabNetworkState input) {
    static constexpr auto network_state_map = base::MakeFixedFlatMap<
        TabNetworkState, tab_strip::mojom::TabNetworkState>(
        {{TabNetworkState::kNone, tab_strip::mojom::TabNetworkState::kNone},
         {TabNetworkState::kWaiting,
          tab_strip::mojom::TabNetworkState::kWaiting},
         {TabNetworkState::kLoading,
          tab_strip::mojom::TabNetworkState::kLoading},
         {TabNetworkState::kError, tab_strip::mojom::TabNetworkState::kError}});
    return network_state_map.at(input);
  }

  static bool FromMojom(tab_strip::mojom::TabNetworkState input,
                        TabNetworkState* out) {
    static constexpr auto network_state_map = base::MakeFixedFlatMap<
        tab_strip::mojom::TabNetworkState, TabNetworkState>(
        {{tab_strip::mojom::TabNetworkState::kNone, TabNetworkState::kNone},
         {tab_strip::mojom::TabNetworkState::kWaiting,
          TabNetworkState::kWaiting},
         {tab_strip::mojom::TabNetworkState::kLoading,
          TabNetworkState::kLoading},
         {tab_strip::mojom::TabNetworkState::kError, TabNetworkState::kError}});
    *out = network_state_map.at(input);
    return true;
  }
};

}  // namespace mojo

#endif  // CHROME_BROWSER_UI_WEBUI_TAB_STRIP_TAB_STRIP_MOJOM_TRAITS_H_
