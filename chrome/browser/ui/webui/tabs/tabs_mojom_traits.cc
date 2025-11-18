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
    case NativeTabAlertState::kMediaRecording:
      return MojoTabAlertState::kMediaRecording;
    case NativeTabAlertState::kTabCapturing:
      return MojoTabAlertState::kTabCapturing;
    case NativeTabAlertState::kAudioPlaying:
      return MojoTabAlertState::kAudioPlaying;
    case NativeTabAlertState::kAudioMuting:
      return MojoTabAlertState::kAudioMuting;
    case NativeTabAlertState::kBluetoothConnected:
      return MojoTabAlertState::kBluetoothConnected;
    case NativeTabAlertState::kBluetoothScanActive:
      return MojoTabAlertState::kBluetoothScanActive;
    case NativeTabAlertState::kUsbConnected:
      return MojoTabAlertState::kUsbConnected;
    case NativeTabAlertState::kHidConnected:
      return MojoTabAlertState::kHidConnected;
    case NativeTabAlertState::kSerialConnected:
      return MojoTabAlertState::kSerialConnected;
    case NativeTabAlertState::kPipPlaying:
      return MojoTabAlertState::kPipPlaying;
    case NativeTabAlertState::kDesktopCapturing:
      return MojoTabAlertState::kDesktopCapturing;
    case NativeTabAlertState::kVrPresentingInHeadset:
      return MojoTabAlertState::kVrPresentingInHeadset;
    case NativeTabAlertState::kAudioRecording:
      return MojoTabAlertState::kAudioRecording;
    case NativeTabAlertState::kVideoRecording:
      return MojoTabAlertState::kVideoRecording;
    case mojo::NativeTabAlertState::kActorAccessing:
      return MojoTabAlertState::kActorAccessing;
    case mojo::NativeTabAlertState::kActorWaitingOnUser:
      return MojoTabAlertState::kActorWaitingOnUser;
    case NativeTabAlertState::kGlicAccessing:
      return MojoTabAlertState::kGlicAccessing;
    case NativeTabAlertState::kGlicSharing:
      return MojoTabAlertState::kGlicSharing;
  }
  NOTREACHED();
}

bool EnumTraits<MojoTabAlertState, NativeTabAlertState>::FromMojom(
    MojoTabAlertState in,
    NativeTabAlertState* out) {
  switch (in) {
    case MojoTabAlertState::kMediaRecording:
      *out = NativeTabAlertState::kMediaRecording;
      return true;
    case MojoTabAlertState::kTabCapturing:
      *out = NativeTabAlertState::kTabCapturing;
      return true;
    case MojoTabAlertState::kAudioPlaying:
      *out = NativeTabAlertState::kAudioPlaying;
      return true;
    case MojoTabAlertState::kAudioMuting:
      *out = NativeTabAlertState::kAudioMuting;
      return true;
    case MojoTabAlertState::kBluetoothConnected:
      *out = NativeTabAlertState::kBluetoothConnected;
      return true;
    case MojoTabAlertState::kBluetoothScanActive:
      *out = NativeTabAlertState::kBluetoothScanActive;
      return true;
    case MojoTabAlertState::kUsbConnected:
      *out = NativeTabAlertState::kUsbConnected;
      return true;
    case MojoTabAlertState::kHidConnected:
      *out = NativeTabAlertState::kHidConnected;
      return true;
    case MojoTabAlertState::kSerialConnected:
      *out = NativeTabAlertState::kSerialConnected;
      return true;
    case MojoTabAlertState::kPipPlaying:
      *out = NativeTabAlertState::kPipPlaying;
      return true;
    case MojoTabAlertState::kDesktopCapturing:
      *out = NativeTabAlertState::kDesktopCapturing;
      return true;
    case MojoTabAlertState::kVrPresentingInHeadset:
      *out = NativeTabAlertState::kVrPresentingInHeadset;
      return true;
    case MojoTabAlertState::kAudioRecording:
      *out = NativeTabAlertState::kAudioRecording;
      return true;
    case MojoTabAlertState::kVideoRecording:
      *out = NativeTabAlertState::kVideoRecording;
      return true;
    case MojoTabAlertState::kActorAccessing:
      *out = NativeTabAlertState::kActorAccessing;
      return true;
    case MojoTabAlertState::kActorWaitingOnUser:
      *out = NativeTabAlertState::kActorWaitingOnUser;
      return true;
    case MojoTabAlertState::kGlicAccessing:
      *out = NativeTabAlertState::kGlicAccessing;
      return true;
    case MojoTabAlertState::kGlicSharing:
      *out = NativeTabAlertState::kGlicSharing;
      return true;
  }
  NOTREACHED();
}

}  // namespace mojo
