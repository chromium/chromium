// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The implementation of echeapi.d.ts
const EcheApiBindingImpl = new class {
  onWebRtcSignalReceived(callback) {
    // TODO(b/178379656): hook the callback to mojo interface.
  }

  sendWebRtcSignal(signaling) {
    // TODO(b/178379656): hook the sending function to mojo interface.
  }
};

// Declare module echeapi and bind the implementation to echeapi.d.ts
const echeapi = {};
echeapi.webrtc = {};
echeapi.webrtc.sendSignal =
EcheApiBindingImpl.sendWebRtcSignal.bind(EcheApiBindingImpl);
echeapi.webrtc.registerSignalReceiver =
EcheApiBindingImpl.onWebRtcSignalReceived.bind(EcheApiBindingImpl);
window['echeapi'] = echeapi;
