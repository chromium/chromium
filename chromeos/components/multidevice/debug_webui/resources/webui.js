/* Copyright 2017 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * JavaScript hooks into the native WebUI handler.
 */
const WebUI = {
  getLogMessages: function() {
    chrome.send('getLogMessages');
  },

  clearLogs: function() {
    chrome.send('clearLogBuffer');
  },

  findEligibleUnlockDevices: function() {
    chrome.send('findEligibleUnlockDevices');
  },

  forceDeviceSync: function() {
    chrome.send('forceDeviceSync');
  },

  forceEnrollment: function() {
    chrome.send('forceEnrollment');
  },

  generateChallenge: function() {
    chrome.send('generateChallenge');
  },

  getAssertion: function() {
    chrome.send('getAssertion');
  },

  getLocalState: function() {
    chrome.send('getLocalState');
  },

  onWebContentsInitialized: function() {
    chrome.send('onWebContentsInitialized');
  },

  toggleConnection: function(publicKey) {
    chrome.send('toggleConnection', [publicKey]);
  },

  toggleUnlockKey: function(publicKey, makeUnlockKey) {
    chrome.send('toggleUnlockKey', [publicKey, makeUnlockKey]);
  },
};

