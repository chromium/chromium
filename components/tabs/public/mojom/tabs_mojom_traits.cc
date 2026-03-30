// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tabs/public/mojom/tabs_mojom_traits.h"

#include "base/notreached.h"

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

NativeTabNetworkState
EnumTraits<MojoTabNetworkState, NativeTabNetworkState>::FromMojom(
    MojoTabNetworkState in) {
  switch (in) {
    case MojoTabNetworkState::kNone:
      return NativeTabNetworkState::kNone;
    case MojoTabNetworkState::kWaiting:
      return NativeTabNetworkState::kWaiting;
    case MojoTabNetworkState::kLoading:
      return NativeTabNetworkState::kLoading;
    case MojoTabNetworkState::kError:
      return NativeTabNetworkState::kError;
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

NativeTabAlertState
EnumTraits<MojoTabAlertState, NativeTabAlertState>::FromMojom(
    MojoTabAlertState in) {
  switch (in) {
    case MojoTabAlertState::kMediaRecording:
      return NativeTabAlertState::kMediaRecording;
    case MojoTabAlertState::kTabCapturing:
      return NativeTabAlertState::kTabCapturing;
    case MojoTabAlertState::kAudioPlaying:
      return NativeTabAlertState::kAudioPlaying;
    case MojoTabAlertState::kAudioMuting:
      return NativeTabAlertState::kAudioMuting;
    case MojoTabAlertState::kBluetoothConnected:
      return NativeTabAlertState::kBluetoothConnected;
    case MojoTabAlertState::kBluetoothScanActive:
      return NativeTabAlertState::kBluetoothScanActive;
    case MojoTabAlertState::kUsbConnected:
      return NativeTabAlertState::kUsbConnected;
    case MojoTabAlertState::kHidConnected:
      return NativeTabAlertState::kHidConnected;
    case MojoTabAlertState::kSerialConnected:
      return NativeTabAlertState::kSerialConnected;
    case MojoTabAlertState::kPipPlaying:
      return NativeTabAlertState::kPipPlaying;
    case MojoTabAlertState::kDesktopCapturing:
      return NativeTabAlertState::kDesktopCapturing;
    case MojoTabAlertState::kVrPresentingInHeadset:
      return NativeTabAlertState::kVrPresentingInHeadset;
    case MojoTabAlertState::kAudioRecording:
      return NativeTabAlertState::kAudioRecording;
    case MojoTabAlertState::kVideoRecording:
      return NativeTabAlertState::kVideoRecording;
    case MojoTabAlertState::kActorAccessing:
      return NativeTabAlertState::kActorAccessing;
    case MojoTabAlertState::kActorWaitingOnUser:
      return NativeTabAlertState::kActorWaitingOnUser;
    case MojoTabAlertState::kGlicAccessing:
      return NativeTabAlertState::kGlicAccessing;
    case MojoTabAlertState::kGlicSharing:
      return NativeTabAlertState::kGlicSharing;
  }
  NOTREACHED();
}

}  // namespace mojo
