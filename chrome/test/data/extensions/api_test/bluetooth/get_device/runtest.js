// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testGetDevice() {
  chrome.test.assertEq(1, devices.length);
  chrome.test.assertEq('d1', devices[0].name);

  chrome.test.succeed();
}

var devices = [];

function failOnError() {
  if (chrome.runtime.lastError) {
    chrome.test.fail(chrome.runtime.lastError.message);
  }
}

chrome.bluetooth.getDevice('11:12:13:14:15:16',
  function(device) {
    failOnError();
    devices.push(device);

    chrome.bluetooth.getDevice('21:22:23:24:25:26',
      function(device) {
        // device should not exists
        if (device || !chrome.runtime.lastError) {
          chrome.test.fail('Unexpected device or missing error');
        }

        chrome.test.sendMessage('ready',
          function(message) {
            chrome.test.runTests([testGetDevice]);
          });
      });
  });
