// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function() {
  var kSecondaryAppId = 'kajpgkhinciaiihghpdamekpjpldgpfi';
  var kNotAllowedError = 'Not allowed in kiosk.';

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

        var secondary = result.find(entry => entry.id == kSecondaryAppId);
        chrome.test.assertTrue(!!secondary);
      }));
    },

    function getSecondary() {
      chrome.management.get(
          kSecondaryAppId, chrome.test.callbackPass(function(info) {
            chrome.test.assertEq(kSecondaryAppId, info.id);
            chrome.test.assertTrue(info.enabled);
          }));
    },

    function disableSelf() {
      chrome.management.setEnabled(
          chrome.runtime.id, false,
          chrome.test.callbackFail(
              'Cannot change the primary kiosk app state.'));
    },

    function disableSecondary() {
      chrome.test.listenOnce(chrome.management.onDisabled, function(info) {
        chrome.test.assertEq(kSecondaryAppId, info.id);
        chrome.test.assertFalse(info.enabled);
      });

      chrome.management.setEnabled(
          kSecondaryAppId, false, chrome.test.callbackPass(function() {
            chrome.management.get(
                kSecondaryAppId, chrome.test.callbackPass(function(info) {
                  chrome.test.assertFalse(info.enabled);
                  chrome.test.assertTrue(info.mayEnable);
                }));
          }));
    },

    function enableSecondary() {
      chrome.test.listenOnce(chrome.management.onEnabled, function(info) {
        chrome.test.assertEq(kSecondaryAppId, info.id);
        chrome.test.assertTrue(info.enabled);
      });

      chrome.management.setEnabled(
          kSecondaryAppId, true, chrome.test.callbackPass(function() {
            chrome.management.get(
                kSecondaryAppId, chrome.test.callbackPass(function(info) {
                  chrome.test.assertTrue(info.enabled);
                }));
          }));
    },

    function uninstallSelf() {
      chrome.management.uninstallSelf(
          chrome.test.callbackFail(kNotAllowedError));
    },

    function uninstallSecondary() {
      chrome.management.uninstall(
          kSecondaryAppId, chrome.test.callbackFail(kNotAllowedError));
    },

    function launchSecondary() {
      chrome.management.launchApp(
          kSecondaryAppId, chrome.test.callbackFail(kNotAllowedError));
    },

    function createAppShortcut() {
      chrome.management.createAppShortcut(
          kSecondaryAppId, chrome.test.callbackFail(kNotAllowedError));
    },

    function setLaunchType() {
      chrome.management.setLaunchType(
          kSecondaryAppId, 'OPEN_AS_WINDOW',
          chrome.test.callbackFail(kNotAllowedError));
    },

    function generateAppForLink() {
      chrome.management.generateAppForLink(
          'https://test.test', 'Test app',
          chrome.test.callbackFail(kNotAllowedError));
    },

    function testSecondaryApp() {
      chrome.runtime.sendMessage(
          kSecondaryAppId, 'runTests', function(response) {
            // If tests in secondary app failed to run, notify test runner of
            // failure before continuing tests in primary app - the test runner
            // expects two test results.
            if (chrome.runtime.lastError) {
              chrome.test.notifyFail(
                  'Secondary app tests failed to run: ' +
                  chrome.runtime.lastError.message);
            } else if (response != 'ok') {
              chrome.test.notifyFail(
                  'Secondary app tests failed to run: app responded: ' +
                  response);
            }

            chrome.test.succeed();
          });
    },
  ]);
});
