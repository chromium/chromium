// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const getCharacteristic = chrome.bluetoothLowEnergy.getCharacteristic;
const charId = 'char_id0';

getCharacteristic(charId, function(result) {
  if (chrome.runtime.lastError) {
    chrome.test.sendMessage(chrome.runtime.lastError.message);
  }

  chrome.test.assertEq(charId, result.instanceId);

  chrome.test.sendMessage('ready', function(message) {
    getCharacteristic(charId, function(result) {
      if (result || !chrome.runtime.lastError) {
        chrome.test.sendMessage('Call to getCharacteristic should have failed');
      }

      chrome.test.sendMessage('ready', function(message) {
        chrome.test.succeed();
      });
    });
  });
});
