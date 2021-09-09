// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const parentMessagePipe = new MessagePipe('chrome://eche-app', window.parent);

let signalingCallback = null;
parentMessagePipe.registerHandler(Message.SEND_SIGNAL, async (message) => {
    if (!signalingCallback) {
        return;
    }
    signalingCallback(/** @type {Uint8Array} */ (message.signal));
})

let screenBacklightCallback = null;
parentMessagePipe.registerHandler(Message.SCREEN_BACKLIGHT_STATE,
 async (message) => {
    if (!screenBacklightCallback) {
        return;
    }
    screenBacklightCallback(/** @type {number} */ (message.state));
})

let tabletModeCallback = null;
parentMessagePipe.registerHandler(Message.TABLET_MODE, async (message) => {
    if (!tabletModeCallback) {
        return;
    }
    tabletModeCallback(/** @type {boolean} */ (message.isTabletMode));
})

let notificationCallback = null;
parentMessagePipe.registerHandler(
  Message.NOTIFICATION_INFO, async (message) => {
  if (!notificationCallback) {
      return;
  }
  notificationCallback(/** @type {!NotificationInfo} */ (message));
})

// The implementation of echeapi.d.ts
const EcheApiBindingImpl = new class {
  closeWindow() {
    parentMessagePipe.sendMessage(Message.CLOSE_WINDOW)
  }

  onWebRtcSignalReceived(callback) {
    signalingCallback = callback;
  }

  sendWebRtcSignal(signaling) {
    parentMessagePipe.sendMessage(Message.SEND_SIGNAL, signaling);
  }

  tearDownSignal() {
    parentMessagePipe.sendMessage(Message.TEAR_DOWN_SIGNAL);
  }

  getSystemInfo() {
    return /** @type {!SystemInfo} */ (
      parentMessagePipe.sendMessage(Message.GET_SYSTEM_INFO));
  }

  getLocalUid() {
    return /** @type {!UidInfo} */ (
      parentMessagePipe.sendMessage(Message.GET_UID));
  }

  onScreenBacklightStateChanged(callback) {
    screenBacklightCallback = callback;
  }

  onReceivedTabletModeChanged(callback) {
    tabletModeCallback = callback;
  }

  onReceivedNotification(callback) {
    notificationCallback = callback;
  }

  showNotification(title, message, notificationType) {
    parentMessagePipe.sendMessage(
        Message.SHOW_NOTIFICATION, {title, message, notificationType});
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
echeapi.webrtc.closeWindow =
    EcheApiBindingImpl.closeWindow.bind(EcheApiBindingImpl);
echeapi.system = {};
echeapi.system.getLocalUid =
    EcheApiBindingImpl.getLocalUid.bind(EcheApiBindingImpl);
echeapi.system.getSystemInfo =
    EcheApiBindingImpl.getSystemInfo.bind(EcheApiBindingImpl);
echeapi.system.registerScreenBacklightState =
    EcheApiBindingImpl.onScreenBacklightStateChanged.bind(EcheApiBindingImpl);
echeapi.system.registerTabletModeChangedReceiver =
    EcheApiBindingImpl.onReceivedTabletModeChanged.bind(EcheApiBindingImpl);
echeapi.system.registerNotificationReceiver =
    EcheApiBindingImpl.onReceivedNotification.bind(EcheApiBindingImpl);
echeapi.system.showCrOSNotification =
    EcheApiBindingImpl.showNotification.bind(EcheApiBindingImpl);
window['echeapi'] = echeapi;
