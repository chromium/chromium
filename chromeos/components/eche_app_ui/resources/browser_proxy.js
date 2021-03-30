// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Returns a remote for SignalingMessageExchanger interface which sends messages
// to the browser.
const signalMessageExchanger =
    chromeos.echeApp.mojom.SignalingMessageExchanger.getRemote();
// An object which receives request messages for the SignalingMessageObserver
// mojom interface and dispatches them as callbacks.
const signalingMessageObserverRouter =
    new chromeos.echeApp.mojom.SignalingMessageObserverCallbackRouter();
// Set up a message pipe to talk to the browser process.
signalMessageExchanger.setSignalingMessageObserver(
    signalingMessageObserverRouter.$.bindNewPipeAndPassRemote());

// The implementation of echeapi.d.ts
const EcheApiBindingImpl = new class {
  onWebRtcSignalReceived(callback) {
    signalingMessageObserverRouter.onReceivedSignalingMessage.addListener(
        callback);
  }

  sendWebRtcSignal(signaling) {
    signalMessageExchanger.sendSignalingMessage(signaling);
  }

  tearDownSignal() {
    signalMessageExchanger.tearDownSignaling();
  }
};

// Declare module echeapi and bind the implementation to echeapi.d.ts
const echeapi = {};
echeapi.webrtc = {};
echeapi.webrtc.sendSignal =
    EcheApiBindingImpl.sendWebRtcSignal.bind(EcheApiBindingImpl);
echeapi.webrtc.tearDownSignal =
    EcheApiBindingImpl.tearDownSignal.bind(EcheApiBindingImpl);
echeapi.webrtc.registerSignalReceiver =
    EcheApiBindingImpl.onWebRtcSignalReceived.bind(EcheApiBindingImpl);
window['echeapi'] = echeapi;
