// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testGetDevices() {
  chrome.test.assertEq(2, devices.length);
  chrome.test.assertEq('d1', devices[0].name);
  chrome.test.assertEq('d2', devices[1].name);

  chrome.test.succeed();
}

var devices = null;

function failOnError() {
  if (chrome.runtime.lastError) {
    chrome.test.fail(chrome.runtime.lastError.message);
  }
}

chrome.bluetooth.getDevices(
  function(result) {
    failOnError();
    devices = result;
    chrome.test.sendMessage('ready',
      function(message) {
        chrome.test.runTests([testGetDevices]);
      });
  });
