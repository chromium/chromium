// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tabs/tabs_mojom_traits.h"

namespace mojo {

MojoTabNetworkState
EnumTraits<MojoTabNetworkState, NativeTabNetworkState>::ToMojom(
    NativeTabNetworkState input) {
  switch (input) {
    case NativeTabNetworkState::kNone:
      return MojoTabNetworkState::kNone;
    case NativeTabNetworkState::kWaiting:
      return MojoTabNetworkState::kWaiting;
    case NativeTabNetworkState::kLoading:
      return MojoTabNetworkState::kLoading;
    case NativeTabNetworkState::kError:
      return MojoTabNetworkState::kError;
  }
  NOTREACHED();
}

bool EnumTraits<MojoTabNetworkState, NativeTabNetworkState>::FromMojom(
    MojoTabNetworkState in,
    NativeTabNetworkState* out) {
  switch (in) {
    case MojoTabNetworkState::kNone:
      *out = NativeTabNetworkState::kNone;
      return true;
    case MojoTabNetworkState::kWaiting:
      *out = NativeTabNetworkState::kWaiting;
      return true;
    case MojoTabNetworkState::kLoading:
      *out = NativeTabNetworkState::kLoading;
      return true;
    case MojoTabNetworkState::kError:
      *out = NativeTabNetworkState::kError;
      return true;
  }
  NOTREACHED();
}

MojoTabAlertState EnumTraits<MojoTabAlertState, NativeTabAlertState>::ToMojom(
    NativeTabAlertState input) {
  switch (input) {
    case NativeTabAlertState::MEDIA_RECORDING:
      return MojoTabAlertState::kMediaRecording;
    case NativeTabAlertState::TAB_CAPTURING:
      return MojoTabAlertState::kTabCapturing;
    case NativeTabAlertState::AUDIO_PLAYING:
      return MojoTabAlertState::kAudioPlaying;
    case NativeTabAlertState::AUDIO_MUTING:
      return MojoTabAlertState::kAudioMuting;
    case NativeTabAlertState::BLUETOOTH_CONNECTED:
      return MojoTabAlertState::kBluetoothConnected;
    case NativeTabAlertState::BLUETOOTH_SCAN_ACTIVE:
      return MojoTabAlertState::kBluetoothScanActive;
    case NativeTabAlertState::USB_CONNECTED:
      return MojoTabAlertState::kUsbConnected;
    case NativeTabAlertState::HID_CONNECTED:
      return MojoTabAlertState::kHidConnected;
    case NativeTabAlertState::SERIAL_CONNECTED:
      return MojoTabAlertState::kSerialConnected;
    case NativeTabAlertState::PIP_PLAYING:
      return MojoTabAlertState::kPipPlaying;
    case NativeTabAlertState::DESKTOP_CAPTURING:
      return MojoTabAlertState::kDesktopCapturing;
    case NativeTabAlertState::VR_PRESENTING_IN_HEADSET:
      return MojoTabAlertState::kVrPresentingInHeadset;
    case NativeTabAlertState::AUDIO_RECORDING:
      return MojoTabAlertState::kAudioRecording;
    case NativeTabAlertState::VIDEO_RECORDING:
      return MojoTabAlertState::kVideoRecording;
    case NativeTabAlertState::GLIC_ACCESSING:
      return MojoTabAlertState::kGlicAccessing;
  }
  NOTREACHED();
}

bool EnumTraits<MojoTabAlertState, NativeTabAlertState>::FromMojom(
    MojoTabAlertState in,
    NativeTabAlertState* out) {
  switch (in) {
    case MojoTabAlertState::kMediaRecording:
      *out = NativeTabAlertState::MEDIA_RECORDING;
      return true;
    case MojoTabAlertState::kTabCapturing:
      *out = NativeTabAlertState::TAB_CAPTURING;
      return true;
    case MojoTabAlertState::kAudioPlaying:
      *out = NativeTabAlertState::AUDIO_PLAYING;
      return true;
    case MojoTabAlertState::kAudioMuting:
      *out = NativeTabAlertState::AUDIO_MUTING;
      return true;
    case MojoTabAlertState::kBluetoothConnected:
      *out = NativeTabAlertState::BLUETOOTH_CONNECTED;
      return true;
    case MojoTabAlertState::kBluetoothScanActive:
      *out = NativeTabAlertState::BLUETOOTH_SCAN_ACTIVE;
      return true;
    case MojoTabAlertState::kUsbConnected:
      *out = NativeTabAlertState::USB_CONNECTED;
      return true;
    case MojoTabAlertState::kHidConnected:
      *out = NativeTabAlertState::HID_CONNECTED;
      return true;
    case MojoTabAlertState::kSerialConnected:
      *out = NativeTabAlertState::SERIAL_CONNECTED;
      return true;
    case MojoTabAlertState::kPipPlaying:
      *out = NativeTabAlertState::PIP_PLAYING;
      return true;
    case MojoTabAlertState::kDesktopCapturing:
      *out = NativeTabAlertState::DESKTOP_CAPTURING;
      return true;
    case MojoTabAlertState::kVrPresentingInHeadset:
      *out = NativeTabAlertState::VR_PRESENTING_IN_HEADSET;
      return true;
    case MojoTabAlertState::kAudioRecording:
      *out = NativeTabAlertState::AUDIO_RECORDING;
      return true;
    case MojoTabAlertState::kVideoRecording:
      *out = NativeTabAlertState::VIDEO_RECORDING;
      return true;
    case MojoTabAlertState::kGlicAccessing:
      *out = NativeTabAlertState::GLIC_ACCESSING;
      return true;
  }
  NOTREACHED();
}

}  // namespace mojo
