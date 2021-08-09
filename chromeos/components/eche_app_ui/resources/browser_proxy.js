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
// Returns a remote for SystemInfoProvider interface which gets system info
// from the browser.
const systemInfo = chromeos.echeApp.mojom.SystemInfoProvider.getRemote();
// Returns a remote for UidGenerator interface which gets an uid from the
// browser.
const uidGenerator = chromeos.echeApp.mojom.UidGenerator.getRemote();
// An object which receives request messages for the SystemInfoObserver
// mojom interface and dispatches them as callbacks.
const systemInfoObserverRouter =
    new chromeos.echeApp.mojom.SystemInfoObserverCallbackRouter();
// Set up a message pipe to the browser process to monitor screen state.
systemInfo.setSystemInfoObserver(
    systemInfoObserverRouter.$.bindNewPipeAndPassRemote());

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

  getSystemInfo() {
    return systemInfo.getSystemInfo();
  }

  getLocalUid() {
    return uidGenerator.getUid();
  }

  onScreenBacklightStateChanged(callback) {
    systemInfoObserverRouter.onScreenBacklightStateChanged.addListener(
        callback);
  }

  onReceivedTabletModeChanged(callback) {
    systemInfoObserverRouter.onReceivedTabletModeChanged.addListener(
        callback);
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
echeapi.system = {};
echeapi.system.getLocalUid =
    EcheApiBindingImpl.getLocalUid.bind(EcheApiBindingImpl);
echeapi.system.getSystemInfo =
    EcheApiBindingImpl.getSystemInfo.bind(EcheApiBindingImpl);
echeapi.system.registerScreenBacklightState =
    EcheApiBindingImpl.onScreenBacklightStateChanged.bind(EcheApiBindingImpl);
echeapi.system.registerTabletModeChangedReceiver =
    EcheApiBindingImpl.onReceivedTabletModeChanged.bind(EcheApiBindingImpl);
window['echeapi'] = echeapi;
