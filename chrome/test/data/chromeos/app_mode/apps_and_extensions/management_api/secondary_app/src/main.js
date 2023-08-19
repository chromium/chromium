// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onMessageExternal.addListener(function(
    message, sender, callback) {
  var kPrimaryAppId = 'adinpkdaebaiabdlinlbjmenialdhibc';
  var kNotAllowedError = 'Not allowed in kiosk.';
  var kChangingPrimaryAppError = 'Cannot change the primary kiosk app state.';

  if (!sender || sender.id != kPrimaryAppId)
    return;

  if (message != 'runTests') {
    callback('invalidMessage: ' + message);
    return;
  }

  callback('ok');

  chrome.test.runTests([
    function getSelf() {
      chrome.management.getSelf(chrome.test.callbackPass(function(info) {
        chrome.test.assertEq(chrome.runtime.id, info.id);
      }));
    },

    function getAll() {
      chrome.management.getAll(chrome.test.callbackPass(function(result) {
        chrome.test.assertEq(2, result.length);
        var self = result.find(entry => entry.id == chrome.runtime.id);
        chrome.test.assertTrue(!!self);

        var secondary = result.find(entry => entry.id == kPrimaryAppId);
        chrome.test.assertTrue(!!secondary);
      }));
    },

    function getPrimary() {
      chrome.management.get(
          kPrimaryAppId, chrome.test.callbackPass(function(info) {
            chrome.test.assertEq(kPrimaryAppId, info.id);
            chrome.test.assertTrue(info.enabled);
          }));
    },

    function disablePrimary() {
      chrome.management.setEnabled(
          kPrimaryAppId, false,
          chrome.test.callbackFail(kChangingPrimaryAppError));
    },

    function enablePrimary() {
      chrome.management.setEnabled(
          kPrimaryAppId, true,
          chrome.test.callbackFail(kChangingPrimaryAppError));
    },

    function uninstallSelf() {
      chrome.management.uninstallSelf(
          chrome.test.callbackFail(kNotAllowedError));
    },

    function uninstallPrimary() {
      chrome.management.uninstall(
          kPrimaryAppId, chrome.test.callbackFail(kNotAllowedError));
    },

    function launchPrimary() {
      chrome.management.launchApp(
          kPrimaryAppId, chrome.test.callbackFail(kNotAllowedError));
    },

    function createAppShortcut() {
      chrome.management.createAppShortcut(
          chrome.runtime.id, chrome.test.callbackFail(kNotAllowedError));
    },

    function setLaunchType() {
      chrome.management.setLaunchType(
          chrome.runtime.id, 'OPEN_AS_WINDOW',
          chrome.test.callbackFail(kNotAllowedError));
    },

    function generateAppForLink() {
      chrome.management.generateAppForLink(
          'https://test.test', 'Test app',
          chrome.test.callbackFail(kNotAllowedError));
    }
  ]);
});
